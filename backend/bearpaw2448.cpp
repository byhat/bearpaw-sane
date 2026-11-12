// sane-bearpaw2448 — SANE backend for the Mustek BearPaw 2448 CU Pro.
// Engine: libbearpawcore (protocol reverse-engineered Nov 2026, see bearpaw.h).
//
// Installation: libsane-bearpaw2448.so.1 into /usr/lib/sane/ plus an
// entry "bearpaw2448" in /etc/sane.d/dll.d/bearpaw2448 — and the scanner
// shows up in every frontend (scanimage, simple-scan, Skanlite, xsane…).
#include <sane/sane.h>
#include <sane/saneopts.h>

#include "bearpaw.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" {

void sane_bearpaw2448_cancel(SANE_Handle h);   // fwd decl: sane_read calls cancel

static const SANE_Device kDevice = {
    "0",                      // name (scanimage adds the backend prefix itself)
    "Mustek",                 // vendor
    "BearPaw 2448 CU Pro",    // model
    "USB scanner (rev-eng)",  // type
};

static const SANE_Device *kDeviceList[2] = { &kDevice, nullptr };
static const SANE_Device *kEmptyList[1] = { nullptr };

SANE_Status sane_bearpaw2448_init(SANE_Int *version_code, SANE_Auth_Callback) {
    if (version_code)
        *version_code = SANE_VERSION_CODE(1, 0, 0);
    return SANE_STATUS_GOOD;
}

void sane_bearpaw2448_exit() {}

SANE_Status sane_bearpaw2448_get_devices(const SANE_Device ***list, SANE_Bool) {
    if (!list)
        return SANE_STATUS_INVAL;
    const bool present = !bearpaw::Scanner::findDevices().empty();
    kDeviceList[0] = &kDevice;
    *list = present ? kDeviceList : kEmptyList;
    return SANE_STATUS_GOOD;
}

// ---------------- state of a single open device ----------------
namespace {

enum OptIndex {
    OPT_NUM = 0,
    OPT_MODE, OPT_RESOLUTION, OPT_PREVIEW,
    OPT_TL_X, OPT_TL_Y, OPT_BR_X, OPT_BR_Y,
    OPT_COUNT
};

constexpr int kWidth = 2122;
constexpr int kA4MmX1000 = 210000;  // micrometers
constexpr int kA4MmY1000 = 297000;

struct Session {
    std::mutex mu;
    std::condition_variable cv;
    std::deque<uint8_t> queue;
    bool scanning = false, failed = false;
    std::thread worker;
    std::atomic_bool cancel{false};
    bool color = true;
    SANE_Frame frame = SANE_FRAME_RGB;
    int lines = 0;             // 0 = unknown until the scan finishes
    SANE_Bool preview = SANE_FALSE;
    int bytesPerLine = kWidth * 3;
};

Session *g_session = nullptr;

static const SANE_Range kRangeX = {0, kA4MmX1000, 1000};
static const SANE_Range kRangeY = {0, kA4MmY1000, 1000};
static const SANE_Word kResList[] = {300};
static const SANE_String_Const kModeList[] = {
    SANE_VALUE_SCAN_MODE_COLOR, SANE_VALUE_SCAN_MODE_GRAY, nullptr};

SANE_Option_Descriptor opts[OPT_COUNT];

void initOptions() {
    std::memset(opts, 0, sizeof(opts));
    opts[OPT_NUM].name = SANE_NAME_NUM_OPTIONS;
    opts[OPT_NUM].title = SANE_TITLE_NUM_OPTIONS;
    opts[OPT_NUM].type = SANE_TYPE_INT;
    opts[OPT_NUM].unit = SANE_UNIT_NONE;
    opts[OPT_NUM].size = sizeof(SANE_Word);
    opts[OPT_NUM].cap = SANE_CAP_SOFT_DETECT;

    opts[OPT_MODE].name = SANE_NAME_SCAN_MODE;
    opts[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
    opts[OPT_MODE].type = SANE_TYPE_STRING;
    opts[OPT_MODE].size = 32;
    opts[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opts[OPT_MODE].constraint.string_list = kModeList;
    opts[OPT_MODE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

    opts[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
    opts[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
    opts[OPT_RESOLUTION].type = SANE_TYPE_INT;
    opts[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
    opts[OPT_RESOLUTION].size = sizeof(SANE_Word);
    opts[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
    static SANE_Word resList[2] = {1, 0};
    resList[0] = 1; resList[1] = 300;
    opts[OPT_RESOLUTION].constraint.word_list = resList;
    opts[OPT_RESOLUTION].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

    opts[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
    opts[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
    opts[OPT_PREVIEW].type = SANE_TYPE_BOOL;
    opts[OPT_PREVIEW].size = sizeof(SANE_Word);
    opts[OPT_PREVIEW].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

    const struct { OptIndex i; const char *n, *t; SANE_Unit u; const SANE_Range *r; } area[] = {
        {OPT_TL_X, SANE_NAME_SCAN_TL_X, SANE_TITLE_SCAN_TL_X, SANE_UNIT_MM, &kRangeX},
        {OPT_TL_Y, SANE_NAME_SCAN_TL_Y, SANE_TITLE_SCAN_TL_Y, SANE_UNIT_MM, &kRangeY},
        {OPT_BR_X, SANE_NAME_SCAN_BR_X, SANE_TITLE_SCAN_BR_X, SANE_UNIT_MM, &kRangeX},
        {OPT_BR_Y, SANE_NAME_SCAN_BR_Y, SANE_TITLE_SCAN_BR_Y, SANE_UNIT_MM, &kRangeY},
    };
    for (const auto &a : area) {
        opts[a.i].name = a.n;
        opts[a.i].title = a.t;
        opts[a.i].type = SANE_TYPE_FIXED;
        opts[a.i].unit = a.u;
        opts[a.i].size = sizeof(SANE_Word);
        opts[a.i].constraint_type = SANE_CONSTRAINT_RANGE;
        opts[a.i].constraint.range = a.r;
        opts[a.i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
}

} // namespace

SANE_Status sane_bearpaw2448_open(SANE_String_Const, SANE_Handle *h) {
    if (g_session)
        return SANE_STATUS_DEVICE_BUSY;   // one scanner, one connection
    initOptions();
    g_session = new Session;
    *h = g_session;
    return SANE_STATUS_GOOD;
}

void sane_bearpaw2448_close(SANE_Handle h) {
    auto *s = static_cast<Session *>(h);
    if (s != g_session)
        return;
    s->cancel = true;
    {
        std::lock_guard<std::mutex> lk(s->mu);
        s->scanning = false;
    }
    s->cv.notify_all();
    if (s->worker.joinable())
        s->worker.join();
    delete s;
    g_session = nullptr;
}

const SANE_Option_Descriptor *sane_bearpaw2448_get_option_descriptor(SANE_Handle, SANE_Int n) {
    if (n < 0 || n >= OPT_COUNT)
        return nullptr;
    return &opts[n];
}

static SANE_Word g_mode = 0;      // 0=color 1=gray
static SANE_Word g_res = 300;
static SANE_Word g_tlx = 0, g_tly = 0, g_brx = SANE_FIX(210.0), g_bry = SANE_FIX(297.0);
static SANE_Word g_preview = SANE_FALSE;

SANE_Status sane_bearpaw2448_control_option(SANE_Handle, SANE_Int n, SANE_Action action,
                                void *value, SANE_Int *info) {
    if (info)
        *info = 0;
    switch (action) {
    case SANE_ACTION_SET_AUTO:
        return SANE_STATUS_UNSUPPORTED;
    case SANE_ACTION_SET_VALUE:
        if (n == OPT_MODE) {
            g_mode = !std::strcmp(static_cast<char *>(value), SANE_VALUE_SCAN_MODE_GRAY);
        } else if (n == OPT_RESOLUTION) {
            g_res = *static_cast<SANE_Word *>(value);
        } else if (n == OPT_PREVIEW) {
            g_preview = *static_cast<SANE_Word *>(value);
        } else {
            SANE_Word v = *static_cast<SANE_Word *>(value);
            switch (n) {
            case OPT_TL_X: g_tlx = v; break;
            case OPT_TL_Y: g_tly = v; break;
            case OPT_BR_X: g_brx = v; break;
            case OPT_BR_Y: g_bry = v; break;
            default: return SANE_STATUS_INVAL;
            }
        }
        if (info)
            *info = SANE_INFO_RELOAD_PARAMS;
        return SANE_STATUS_GOOD;
    case SANE_ACTION_GET_VALUE:
        break;
    }
    switch (n) {
    case OPT_NUM: *static_cast<SANE_Word *>(value) = OPT_COUNT; break;
    case OPT_MODE: std::strcpy(static_cast<char *>(value),
                     g_mode ? SANE_VALUE_SCAN_MODE_GRAY : SANE_VALUE_SCAN_MODE_COLOR); break;
    case OPT_RESOLUTION: *static_cast<SANE_Word *>(value) = g_res; break;
    case OPT_PREVIEW: *static_cast<SANE_Word *>(value) = g_preview; break;
    case OPT_TL_X: *static_cast<SANE_Word *>(value) = g_tlx; break;
    case OPT_TL_Y: *static_cast<SANE_Word *>(value) = g_tly; break;
    case OPT_BR_X: *static_cast<SANE_Word *>(value) = g_brx; break;
    case OPT_BR_Y: *static_cast<SANE_Word *>(value) = g_bry; break;
    default: return SANE_STATUS_INVAL;
    }
    return SANE_STATUS_GOOD;
}

SANE_Status sane_bearpaw2448_get_parameters(SANE_Handle h, SANE_Parameters *p) {
    auto *s = static_cast<Session *>(h);
    p->depth = 8;
    p->pixels_per_line = kWidth;
    p->format = s->color ? SANE_FRAME_RGB : SANE_FRAME_GRAY;
    p->bytes_per_line = kWidth * (s->color ? 3 : 1);
    {
        std::lock_guard<std::mutex> lk(s->mu);
        p->lines = s->lines ? s->lines / (s->color ? 3 : 1) : 0;
    }
    p->last_frame = SANE_TRUE;
    return SANE_STATUS_GOOD;
}

SANE_Status sane_bearpaw2448_start(SANE_Handle h) {
    auto *s = static_cast<Session *>(h);
    if (s->scanning)
        return SANE_STATUS_INVAL;
    bearpaw::ScanParams params;
    params.color = g_mode == 0;
    s->color = params.color;
    s->cancel = false;
    s->failed = false;
    s->lines = 0;
    {
        std::lock_guard<std::mutex> lk(s->mu);
        s->queue.clear();
        s->scanning = true;
    }
    s->worker = std::thread([s, params] {
        const bool ok = bearpaw::Scanner().scan(
            params,
            [s](const uint8_t *line, int len) {
                std::lock_guard<std::mutex> lk(s->mu);
                s->queue.insert(s->queue.end(), line, line + len);
                s->lines += len;
                s->cv.notify_all();
            },
            [](double, const std::string &) {},
            s->cancel);
        std::lock_guard<std::mutex> lk(s->mu);
        s->failed = !ok;
        s->scanning = false;
        s->cv.notify_all();
    });
    return SANE_STATUS_GOOD;
}

SANE_Status sane_bearpaw2448_read(SANE_Handle h, SANE_Byte *data, SANE_Int max_length, SANE_Int *length) {
    auto *s = static_cast<Session *>(h);
    if (length)
        *length = 0;
    std::unique_lock<std::mutex> lk(s->mu);
    s->cv.wait(lk, [s] { return !s->queue.empty() || !s->scanning; });
    if (s->queue.empty()) {
        lk.unlock();
        sane_bearpaw2448_cancel(h);
        return SANE_STATUS_EOF;
    }
    const SANE_Int n = std::min<SANE_Int>(max_length, SANE_Int(s->queue.size()));
    std::copy(s->queue.begin(), s->queue.begin() + n, data);
    s->queue.erase(s->queue.begin(), s->queue.begin() + n);
    if (length)
        *length = n;
    return SANE_STATUS_GOOD;
}

void sane_bearpaw2448_cancel(SANE_Handle h) {
    auto *s = static_cast<Session *>(h);
    s->cancel = true;
    {
        std::lock_guard<std::mutex> lk(s->mu);
        if (!s->scanning)
            return;
    }
    // wait for the scan thread to finish
    std::unique_lock<std::mutex> lk(s->mu);
    s->cv.wait(lk, [s] { return !s->scanning; });
    if (s->worker.joinable())
        s->worker.join();
}

// Plain names as thin wrappers. IMPORTANT: inside a backend never call
// a plain sane_* symbol (it gets interposed to libsane/dll -> infinite
// recursion). Internal calls must use the sane_bearpaw2448_* names only.
#define PLAIN_ALIAS(fn, ret, args_decl, args_call) \
    extern "C" ret sane_##fn args_decl { return sane_bearpaw2448_##fn args_call; }

PLAIN_ALIAS(init, SANE_Status, (SANE_Int *v, SANE_Auth_Callback a), (v, a))
PLAIN_ALIAS(exit, void, (void), ())
PLAIN_ALIAS(get_devices, SANE_Status, (const SANE_Device ***l, SANE_Bool b), (l, b))
PLAIN_ALIAS(open, SANE_Status, (SANE_String_Const n, SANE_Handle *h), (n, h))
PLAIN_ALIAS(close, void, (SANE_Handle h), (h))
PLAIN_ALIAS(get_option_descriptor, const SANE_Option_Descriptor *, (SANE_Handle h, SANE_Int n), (h, n))
PLAIN_ALIAS(control_option, SANE_Status, (SANE_Handle h, SANE_Int o, SANE_Action a, void *v, SANE_Int *i), (h, o, a, v, i))
PLAIN_ALIAS(get_parameters, SANE_Status, (SANE_Handle h, SANE_Parameters *p), (h, p))
PLAIN_ALIAS(start, SANE_Status, (SANE_Handle h), (h))
PLAIN_ALIAS(read, SANE_Status, (SANE_Handle h, SANE_Byte *b, SANE_Int m, SANE_Int *l), (h, b, m, l))
PLAIN_ALIAS(cancel, void, (SANE_Handle h), (h))

} // extern "C"
