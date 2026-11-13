# bearpaw-sane

[![CI](https://github.com/byhat/bearpaw-sane/actions/workflows/build.yml/badge.svg)](https://github.com/byhat/bearpaw-sane/actions/workflows/build.yml)
[![License: GPL-2.0+](https://img.shields.io/badge/license-GPL--2.0%2B-blue.svg)](#license)
[![Arch](https://img.shields.io/badge/Arch-.pkg.tar.zst-1793D1.svg)](https://github.com/byhat/bearpaw-sane/releases)
[![Debian](https://img.shields.io/badge/Debian-.deb-A80030.svg)](https://github.com/byhat/bearpaw-sane/releases)

**Linux SANE driver for the Mustek BearPaw 2448 CU Pro USB scanner** —
a fully reverse-engineered, community driver. No vendor software, no
kernel module: pure userspace via libusb.

| | |
|---|---|
| **Supported device** | Mustek BearPaw 2448 CU Pro (`USB 055f:0408`) |
| **Interface** | SANE backend — works in scanimage, simple-scan, Skanlite, xsane, gscan2pdf… |
| **Modes** | Color / Grayscale, 8 bit, 300 dpi, full A4 |
| **Privileges** | None needed (udev rule, `scanner` group) |
| **License** | GPL-2.0-or-later |

![scan sample](docs/scan-sample.png)
*Real scan produced by this driver (A4 notebook page, 300 dpi color).*

## Install

### Arch Linux

```bash
# from a release (see the Releases page):
sudo pacman -U sane-bearpaw2448-git-*-x86_64.pkg.tar.zst
# or build from a checkout:
git clone https://github.com/byhat/bearpaw-sane && cd bearpaw-sane && makepkg -si
```

### Debian / Ubuntu

Download the `.deb` from the [Releases](https://github.com/byhat/bearpaw-sane/releases) page:

```bash
sudo apt install ./sane-bearpaw2448_*_amd64.deb
```

### Any distro (from source / portable zip)

```bash
./install.sh              # builds (if needed) and installs everything
./install.sh --uninstall  # removes it
```

Build requirements: cmake, g++, libusb-1.0-dev, sane-backends (dev).
No build machine at hand? Grab `bearpaw2448-portable-*.zip` from the
Releases page — it contains the self-contained backend plus a one-line
installer (`sudo ./install.sh` inside).

After installing, replug the scanner's USB cable (so udev grants
access) and check:

```bash
scanimage -L                          # must list bearpaw2448:0
scanimage -d bearpaw2448 > scan.pnm   # scan!
```

If the `scanner` group is missing on your system:
`sudo groupadd -r scanner && sudo usermod -aG scanner $USER` (then re-login).

## How it works

The vendor protocol was reverse-engineered from a live USB capture of
the original Windows software (see the companion research project).
The driver replays the recorded engine session with an absolute
schedule and decodes the raw line stream:

```
core/    libbearpawcore — libusb transport, session replay, decoder:
         7680 B/line = 2560 px, channels in GBR order, the G die is
         mirrored in X (+66 px), the frame is mirrored, usable width
         2122 px, fixed white-balance LUT
backend/ sane-bearpaw2448 — SANE API (Color/Gray, 300 dpi)
testcli/ bptestcli — console scanner → PNM (SANE-free testing)
data/    session.ops — the recorded vendor command sequence
         (embedded into the .so at link time)
```

Quick engine test without SANE: `sudo ./build/bptestcli out.pnm [--gray]`.

## Roadmap

- [ ] 75 / 150 dpi, scan area selection
- [ ] homing the carriage on cancel
- [ ] live shading from the calibration lines (instead of the fixed LUT)
- [ ] AUR package (`yay -S sane-bearpaw2448-git`)
- [ ] submission upstream to [sane-backends](https://gitlab.com/sane-project/backends)

## Building and packaging from source

```bash
cmake -S . -B build && cmake --build build -j   # backend + testcli
packaging/make-deb.sh    # builds the .deb (works on any Linux, even Arch)
./portable.sh            # portable zip: .so + udev rule + installer
makepkg -f               # Arch package
```

## License

GPL-2.0-or-later, same as [sane-backends](https://gitlab.com/sane-project/backends).
The protocol was reverse-engineered from traffic captured from the
user's own device.
