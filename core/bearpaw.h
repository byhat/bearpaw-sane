// libbearpawcore — движок сканера Mustek BearPaw 2448 CU Pro (055f:0408).
//
// Протокол реверснут из живого usb-захвата фирменного ПО (11.2026):
//   * управление: vendor control-передачи (bmReq 0x40/0xc0, bReq 4/12);
//   * данные: bulk IN 0x82; служебные записи: bulk OUT 0x01;
//   * строка изображения: 7680 байт = 2560 px, каналы GBR,
//     G-слот зеркален по X со сдвигом 66 px, кадр зеркален,
//     полезная ширина 2122 px (поля 219), 8 бит.
#pragma once

#include <cstdint>
#include <atomic>
#include <string>
#include <vector>
#include <functional>

namespace bearpaw {

constexpr uint16_t kVid = 0x055f;
constexpr uint16_t kPid = 0x0408;

struct DeviceInfo {
    int bus = 0, address = 0;
    std::string product;     // например "USB2.0 Scanner"
    std::string id;          // "bus:addr"
};

struct ScanParams {
    bool color = true;       // false = ч/б (G-канал)
    int  width = 2122;       // выходная ширина (по эталону)
};

// Одна строка декодированного изображения: RGB interleaved (len = width*3)
// либо gray (len = width).
using LineFn   = std::function<void(const uint8_t *line, int len)>;
using ProgressFn = std::function<void(double pct, const std::string &stage)>;
using LogFn    = std::function<void(const std::string &msg)>;

class Scanner {
public:
    explicit Scanner(LogFn log = nullptr);
    ~Scanner();

    static std::vector<DeviceInfo> findDevices();

    // Синхронный полный цикл сканирования (~50 c): инициализация ->
    // калибровка -> скан -> возврат каретки. Строки отдаются по мере
    // готовности через onLine. Возвращает false при ошибке (текст в логе).
    bool scan(const ScanParams &params,
              const LineFn &onLine,
              const ProgressFn &onProgress,
              const std::atomic_bool &cancel);

private:
    struct Impl;
    Impl *d = nullptr;
};

} // namespace bearpaw
