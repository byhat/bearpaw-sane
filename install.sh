#!/usr/bin/env bash
# install.sh — установка/удаление sane-bearpaw2448 на любой Linux.
#
#   ./install.sh              собрать (если нужно) и установить
#   ./install.sh --uninstall  удалить
#
# Требования для сборки: cmake, g++, libusb-1.0-dev, sane-backends (dev).
# (Arch: pacman -S cmake libusb sane;  Debian: apt install cmake g++ libusb-1.0-0-dev libsane-dev)
set -euo pipefail
cd "$(dirname "$0")"

# каталог sane для этого дистрибутива (Arch: /usr/lib/sane, Debian: /usr/lib/<triplet>/sane)
SANE_DIR=/usr/lib/sane
for d in /usr/lib/x86_64-linux-gnu/sane /usr/lib/i386-linux-gnu/sane; do
    [[ -d "$d" ]] && SANE_DIR="$d"
done

if [[ "${1:-}" == "--uninstall" ]]; then
    sudo rm -f "$SANE_DIR"/libsane-bearpaw2448.so.1 "$SANE_DIR"/libsane-bearpaw2448.so 2>/dev/null || true
    sudo rm -f /usr/lib/udev/rules.d/99-bearpaw2448.rules \
                /lib/udev/rules.d/99-bearpaw2448.rules \
                /etc/sane.d/dll.d/bearpaw2448 /etc/sane.d/bearpaw2448.conf 2>/dev/null || true
    echo "удалено. (проверь: scanimage -L)"
    exit 0
fi

# каталог sane для этого дистрибутива (Arch: /usr/lib/sane, Debian: /usr/lib/<triplet>/sane)
SANE_DIR=/usr/lib/sane
for d in /usr/lib/x86_64-linux-gnu/sane /usr/lib/i386-linux-gnu/sane; do
    [[ -d "$d" ]] && SANE_DIR="$d"
done

if [[ ! -f build/libsane-bearpaw2448.so.1 ]]; then
    echo "== сборка =="
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j"$(nproc)"
fi

echo "== установка в $SANE_DIR =="
sudo install -Dm755 build/libsane-bearpaw2448.so.1 "$SANE_DIR/libsane-bearpaw2448.so.1"
sudo ln -sf libsane-bearpaw2448.so.1 "$SANE_DIR/libsane-bearpaw2448.so"
echo bearpaw2448 | sudo tee /etc/sane.d/dll.d/bearpaw2448 >/dev/null
sudo install -Dm644 backend/bearpaw2448.conf /etc/sane.d/bearpaw2448.conf
sudo install -Dm644 udev/99-bearpaw2448.rules /usr/lib/udev/rules.d/99-bearpaw2448.rules
sudo udevadm control --reload 2>/dev/null || true
sudo udevadm trigger 2>/dev/null || true

echo ""
echo "ГОТОВО."
echo "  Если сканер уже был вставлен — передёрни USB-кабель (чтобы udev выдал права)."
echo "  Проверка:   scanimage -L        (должен показать bearpaw2448:0)"
echo "  Скан:       scanimage -d bearpaw2448 > scan.pnm"
echo "  Также появится в simple-scan / Skanlite / xsane."
echo "  Если в системе нет группы scanner: sudo groupadd -r scanner && sudo usermod -aG scanner \$USER"
