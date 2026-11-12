#!/usr/bin/env bash
# portable.sh — build a portable zip: the self-contained backend + the udev
# rule + a one-command installer. Unpack on the target machine and run
# sudo ./install.sh inside.
set -euo pipefail
cd "$(dirname "$0")"

[[ -f build/libsane-bearpaw2448.so.1 ]] || {
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j"$(nproc)"
}

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/bearpaw-sane"
cp build/libsane-bearpaw2448.so.1 "$TMP/bearpaw-sane/"
cp udev/99-bearpaw2448.rules "$TMP/bearpaw-sane/"
cat > "$TMP/bearpaw-sane/install.sh" <<'EOS'
#!/usr/bin/env bash
# install the self-contained backend (no build step)
set -euo pipefail
cd "$(dirname "$0")"
SANE_DIR=/usr/lib/sane
for d in /usr/lib/x86_64-linux-gnu/sane /usr/lib/i386-linux-gnu/sane; do
    [[ -d "$d" ]] && SANE_DIR="$d"
done
sudo install -Dm755 libsane-bearpaw2448.so.1 "$SANE_DIR/libsane-bearpaw2448.so.1"
sudo ln -sf libsane-bearpaw2448.so.1 "$SANE_DIR/libsane-bearpaw2448.so"
echo bearpaw2448 | sudo tee /etc/sane.d/dll.d/bearpaw2448 >/dev/null
sudo install -Dm644 99-bearpaw2448.rules /usr/lib/udev/rules.d/99-bearpaw2448.rules
sudo udevadm control --reload 2>/dev/null || true
sudo udevadm trigger 2>/dev/null || true
echo "DONE: replug the scanner's USB cable, then run scanimage -L"
EOS
chmod +x "$TMP/bearpaw-sane/install.sh"

OUT="bearpaw2448-portable-$(date +%Y%m%d).zip"
(cd "$TMP" && zip -qr "$OLDPWD/$OUT" bearpaw-sane)
echo "created $OUT (contains: .so, udev rule, install.sh)"
