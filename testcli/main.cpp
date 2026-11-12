// bptestcli — скан BearPaw 2448 CU Pro из консоли (без SANE).
// Использование: bptestcli [out.pnm] [--gray]
//   (по умолчанию scan.pnm, цветной P6)
#include "bearpaw.h"

#include <cstdio>
#include <cstring>
#include <atomic>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
    std::string out = "scan.pnm";
    bearpaw::ScanParams params;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--gray"))
            params.color = false;
        else
            out = argv[i];
    }

    auto devices = bearpaw::Scanner::findDevices();
    if (devices.empty()) {
        std::cerr << "сканер 054f:0408 не найден (проверь USB/права)\n";
        return 1;
    }
    for (auto &d : devices)
        std::cerr << "найдено: " << d.product << " [" << d.id << "]\n";

    bearpaw::Scanner scanner([](const std::string &m) { std::cerr << "[bearpaw] " << m << "\n"; });

    std::ofstream f(out, std::ios::binary);
    if (!f) {
        std::cerr << "не открыть " << out << "\n";
        return 1;
    }
    f << (params.color ? "P6" : "P5") << "\n";
    const int W = 2122;
    f << W << " 3500\n255\n";   // высота уточнится при записи (перепишем заголовок в конце)

    std::vector<char> headerBackup;
    const std::streamoff headerSize = f.tellp();
    (void)headerSize;

    std::atomic_bool cancel{false};
    int rows = 0, lastPct = -1;
    const uint64_t t0 = [] { return 0; }();(void)t0;

    const bool ok = scanner.scan(params,
        [&](const uint8_t *line, int len) {
            f.write(reinterpret_cast<const char *>(line), len);
            ++rows;
        },
        [&](double pct, const std::string &) {
            const int p = int(pct);
            if (p != lastPct) {
                lastPct = p;
                std::fprintf(stderr, "\rпрогресс: %3d%%", p);
                std::fflush(stderr);
            }
        },
        cancel);

    // перезаписать высоту в заголовке
    f.flush();
    if (rows > 0) {
        const std::string head = (params.color ? "P6\n" : "P5\n") +
                                 std::to_string(W) + " " + std::to_string(rows) + "\n255\n";
        // заголовок фиксированной длины не гарантирован — перечитаем файл целиком
        f.close();
        std::ifstream rf(out, std::ios::binary);
        std::vector<char> data((std::istreambuf_iterator<char>(rf)),
                               std::istreambuf_iterator<char>());
        rf.close();
        const std::string pixelData(data.begin() + data.size() - uint64_t(rows) * (params.color ? W * 3 : W),
                                    data.end());
        std::ofstream wf(out, std::ios::binary);
        wf << head;
        wf.write(pixelData.data(), pixelData.size());
    }
    std::fprintf(stderr, "\n%s: %d строк -> %s\n", ok ? "готово" : "ОШИБКА", rows, out.c_str());
    return ok ? 0 : 2;
}
