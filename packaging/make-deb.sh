#!/usr/bin/env bash
# make-deb.sh — build sane-bearpaw2448_*.deb without dpkg (plain ar+tar,
# works on any Linux including Arch). The result lands in the project root.
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION="${VERSION:-0.1.0-1}"
OUT="sane-bearpaw2448_${VERSION}_amd64.deb"

[[ -f build/libsane-bearpaw2448.so.1 ]] || {
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j"$(nproc)"
}

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# ---- data.tar.gz: package files ----
D="$TMP/data"
mkdir -p "$D/usr/lib/x86_64-linux-gnu/sane" "$D/etc/sane.d/dll.d" "$D/usr/lib/udev/rules.d"
install -m755 build/libsane-bearpaw2448.so.1 "$D/usr/lib/x86_64-linux-gnu/sane/"
ln -s libsane-bearpaw2448.so.1 "$D/usr/lib/x86_64-linux-gnu/sane/libsane-bearpaw2448.so"
install -m644 backend/bearpaw2448.conf "$D/etc/sane.d/bearpaw2448.conf"
echo bearpaw2448 > "$D/etc/sane.d/dll.d/bearpaw2448"
install -m644 udev/99-bearpaw2448.rules "$D/usr/lib/udev/rules.d/99-bearpaw2448.rules"
tar -czf "$TMP/data.tar.gz" -C "$D" .

# ---- control.tar.gz: metadata + postinst ----
C="$TMP/control.dir"
mkdir -p "$C"
cat > "$C/control" <<EOF
Package: sane-bearpaw2448
Version: ${VERSION}
Architecture: amd64
Maintainer: BearPawScanner project <bearpaw@local>
Depends: libsane, libusb-1.0-0
Section: graphics
Priority: optional
Homepage: https://github.com/bearpaw/bearpaw-sane
Description: SANE backend for Mustek BearPaw 2448 CU Pro scanner
 Reverse-engineered userspace driver (SANE backend) for the Mustek
 BearPaw 2448 CU Pro USB scanner (055f:0408): vendor control protocol,
 bulk data channel, GBR line format with channel registration.
EOF
cat > "$C/postinst" <<'EOF'
#!/bin/sh
udevadm control --reload 2>/dev/null || true
udevadm trigger 2>/dev/null || true
echo "sane-bearpaw2448 installed: replug the scanner USB cable and check scanimage -L"
exit 0
EOF
chmod 755 "$C/postinst"
tar -czf "$TMP/control.tar.gz" -C "$C" .

# ---- .deb = ar archive ----
echo "2.0" > "$TMP/debian-binary"
rm -f "$OUT"
ar rcs "$OUT" "$TMP/debian-binary" "$TMP/control.tar.gz" "$TMP/data.tar.gz"
echo "created $OUT ($(stat -c%s "$OUT") bytes)"
