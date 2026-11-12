# bearpaw-sane — Linux-драйвер (SANE) Mustek BearPaw 2448 CU Pro

Userspace-драйвер сканера **055f:0408** по реверснутому протоколу
(см. соседний проект `BearPawScanner/STATUS.md`). Модуль ядра не нужен:
libusb → usbfs.

## Состав

```
core/    libbearpawcore: libusb-транспорт, реплей эталонной сессии
         (data/session.ops, абсолютное расписание), декодер
         (7680Б/строка, GBR, G-зеркало+66px, кроп 2122px, LUT)
backend/ sane-bearpaw2448: SANE-бэкенд (режим Color/Gray, 300 dpi)
testcli/ bptestcli: скан из консоли в PNM (тесты без SANE)
packaging/aur,packaging/debian  пакеты (yay / apt)
udev/    правило доступа без root
```

## Сборка

    cmake -S . -B build && cmake --build build -j

## Проверка без SANE

    sudo ./build/bptestcli out.pnm          # цветной, ~50 сек
    sudo ./build/bptestcli out.pnm --gray

## Установка на другой ПК

Бэкенд — **один самодостаточный файл** (эталонная сессия вшита в .so),
внешних данных не требует.

**Вариант 1 — из исходников (универсальный):** скопировать папку проекта
на целевую машину и выполнить

    ./install.sh            # сборка (если не собрано) + установка
    ./install.sh --uninstall

Зависимости для сборки: cmake, g++, libusb-1.0-dev, sane-backends(dev):
Arch: `pacman -S cmake libusb sane`; Debian: `apt install cmake g++ libusb-1.0-0-dev libsane-dev`.

**Вариант 2 — без сборки (x86_64):** забрать `build/libsane-bearpaw2448.so.1`
(или `make pkgzip`: `install.sh` собирает zip с .so и правилами) и на цели:

    sudo install -m755 libsane-bearpaw2448.so.1 /usr/lib/sane/   # Debian: /usr/lib/x86_64-linux-gnu/sane/
    echo bearpaw2448 | sudo tee /etc/sane.d/dll.d/bearpaw2448
    sudo install -m644 99-bearpaw2448.rules /usr/lib/udev/rules.d/
    sudo udevadm control --reload && sudo udevadm trigger

**Вариант 3 — пакеты:** Arch — `packaging/aur/PKGBUILD` (после публикации
репозитория: `yay -S sane-bearpaw2448-git`); Debian/Ubuntu —
`packaging/debian/` → `dpkg-buildpackage -us -uc` → `apt install ./sane-bearpaw2448_*.deb`.

## Использование

    scanimage -L                        # список устройств
    scanimage -d bearpaw2448 > scan.pnm # скан (цвет, 300 dpi, A4)
    simple-scan / Skanlite / xsane      # из GUI

Права: udev-правило даёт доступ группе `scanner` (+`uaccess` для
активной консоли). Если группы нет: `sudo groupadd -r scanner &&
sudo usermod -aG scanner $USER` и перелогиниться. После установки
правила передёрнуть USB-кабель сканера.

## Установка бэкенда (для теста, без пакета)

    sudo install -m755 build/backend/libsane-bearpaw2448.so.1 /usr/lib/sane/
    echo bearpaw2448 | sudo tee /etc/sane.d/dll.d/bearpaw2448
    sudo install -m644 udev/99-bearpaw2448.rules /usr/lib/udev/rules.d/
    sudo udevadm control --reload && sudo udevadm trigger
    scanimage -L                            # должен показать bearpaw2448
    scanimage -d bearpaw2448 > scan.pnm     # проверка скана

## Пакеты

* **Arch/AUR**: `packaging/aur/PKGBUILD` (после публикации git-репо:
  `yay -S sane-bearpaw2448-git`)
* **Debian/Ubuntu**: `packaging/debian/` → `dpkg-buildpackage -us -uc`

## Ограничения (v0.1)

* только 300 dpi и полная область A4 (эталонная сессия одна);
* отмена скана останавливает каретку на месте (возврат домой — в планах);
* цветокор фиксированным LUT (шейдинг из калибровочных строк — в планах).

## Обновление эталонной сессии

    python ../BearPawScanner/tools/make_session_bin.py \
        ../BearPawScanner/session.json data/session.ops
    cmake -S . -B build && cmake --build build -j
