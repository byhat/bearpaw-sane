# bearpaw-sane — Linux SANE driver for Mustek BearPaw 2448 CU Pro

Userspace driver (SANE backend) for the **Mustek BearPaw 2448 CU Pro** USB
scanner (`055f:0408`), based on a fully reverse-engineered protocol
(see the companion `BearPawScanner` project). No kernel module needed:
libusb → usbfs.

## Overview

```
core/    libbearpawcore: libusb transport, replay of the recorded vendor
         session (data/session.ops, embedded into the .so), absolute
         schedule pacing, decoder (7680 B/line, GBR order, mirrored G-die
         +66px, 2122px crop, white-balance LUT)
backend/ sane-bearpaw2448: SANE backend (Color/Gray, 300 dpi)
testcli/ bptestcli: console scanner → PNM (no SANE needed for testing)
packaging/aur,packaging/debian  AUR PKGBUILD and Debian packaging
udev/    device access rule (no root needed, `scanner` group)
```

## Build

    cmake -S . -B build && cmake --build build -j

Requirements: cmake, g++, libusb-1.0 (dev), sane-backends (dev headers).
Arch: `pacman -S cmake libusb sane`; Debian: `apt install cmake g++ libusb-1.0-0-dev libsane-dev`.

## Test without SANE

    sudo ./build/bptestcli out.pnm          # color, ~50 s
    sudo ./build/bptestcli out.pnm --gray

## Installing 

The backend is a **single self-contained file** (the recorded session is
embedded into the .so), no external data files required.

**Option 1 — from source (any distro):** copy the project folder and run

    ./install.sh              # builds (if needed) and installs
    ./install.sh --uninstall  # removes everything

**Option 2 — no build (x86_64):** run `./portable.sh`, copy the produced
`bearpaw2448-portable-*.zip` to the target machine, unpack and run
`sudo ./install.sh` inside.

**Option 3 — Arch package (yay):** once the repo is published on GitHub:

    yay -S sane-bearpaw2448-git          # after the PKGBUILD is submitted to AUR
    # or straight from GitHub, no AUR:
    git clone https://github.com/byhat/bearpaw-sane && cd bearpaw-sane && makepkg -si

Local package check: `makepkg -f && pacman -U sane-bearpaw2448-git-*.pkg.tar.zst`.

**Option 4 — Debian/Ubuntu (.deb):**

    packaging/make-deb.sh                 # builds a real .deb (works even on Arch)
    sudo apt install ./sane-bearpaw2448_0.1.0-1_amd64.deb

On a Debian machine the classic route also works: `packaging/debian/` →
`dpkg-buildpackage -us -uc` → `apt install ../sane-bearpaw2448_*.deb`.

## Usage

    scanimage -L                        # list devices
    scanimage -d bearpaw2448 > scan.pnm # scan (color, 300 dpi, A4)
    simple-scan / Skanlite / xsane      # from the GUI

Permissions: the udev rule grants access to the `scanner` group
(plus `uaccess` for the active seat). If the group is missing:
`sudo groupadd -r scanner && sudo usermod -aG scanner $USER`, re-login,
and replug the scanner's USB cable after installing the rule.

## Limitations (v0.1)

* 300 dpi and full A4 area only (single recorded session);
* cancelling a scan stops the carriage in place (homing on cancel is
  planned);
* white balance uses a fixed LUT (proper shading from the calibration
  lines is planned).

## Updating the recorded session

    python ../BearPawScanner/tools/make_session_bin.py \
        ../BearPawScanner/session.json data/session.ops
    cmake -S . -B build && cmake --build build -j
