# JailFlip

A **standalone app** (`.fap`) for the Flipper Zero — **not** custom firmware.
Built and targeted for **Momentum**, it also works on the official SDK.

> ⚠️ "Jailbreak" is borrowed loosely here. The Flipper Zero is **not** a locked
> device — its firmware is open source. JailFlip doesn't bypass any security;
> it's a convenience launcher built on the normal SDK. The name is just a nod
> to the iOS scene (Sileo → **Flipeo**).

## Features

| Module | What it does |
| --- | --- |
| **Flipeo** | Sileo-style plugin browser. The main list shows installed plugins (scanned from `/ext/apps/<Category>/*.fap`). The header reads **Flipeo**; the top **＋ Add plugin** row opens a catalog of available plugins read from a file. Selecting any entry shows its details and launches it (if installed) via the system loader. |
| **Boot Logo** | Pick a `128x64` `.bmx` image from the SD card and stage it as a boot splash. On **Momentum**: *Settings → Desktop → Boot Graphics* can point at it. |
| **Keys** | Read-only browser over your stored `SubGHz` / `NFC` / `iButton` / `LF RFID` files. Inspect what you've saved; emulation stays in the dedicated apps. |
| **About** | What it is, in-device. |

## The plugin catalog (the "＋" list)

Base plugins ship inside the `.fap` as `assets/catalog.txt`. `ufbt`/`fbt`
packs the `assets/` folder (`fap_file_assets`) and the loader extracts it to
`/ext/apps_assets/jailflip/catalog.txt` on first launch. Edit that file (on the
SD card or in `assets/` before building) to change the list:

```
# name | category | path | description
Clock | Tools | /ext/apps/Tools/clock.fap | Simple on-screen clock.
```

Flipeo marks an entry **installed** when its `path` exists on the SD card.

## Project layout

```
application.fam     # app manifest (ufbt/fbt)
jailflip.c          # entry point, main menu, view dispatcher
jailflip_i.h        # shared app struct + declarations
jailflip_store.c    # Flipeo plugin browser + launcher
jailflip_keys.c     # key-file browser
jailflip_logo.c     # boot-logo staging
assets/catalog.txt  # bundled base-plugin catalog (packed into the .fap)
images/jailflip.png # 10x10 app icon
```

## Build & run

Uses **ufbt** (micro Flipper Build Tool). Install it once:

```bash
python3 -m pip install --upgrade ufbt
```

Then from the project root:

```bash
ufbt              # build -> dist/jailflip.fap
ufbt launch       # build, upload to a connected Flipper, and run it
```

Copy `dist/jailflip.fap` to `SD:/apps/Tools/` to keep it on the device.

### Target firmware — Momentum

Point `ufbt` at the Momentum SDK, then build as usual:

```bash
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json
ufbt
```

(You can keep a separate official toolchain with `ufbt update --channel=release`.)

The Boot Logo feature only takes effect on firmwares that read a user splash
image; on stock firmware the file is simply staged and ignored.

## Status

Source is complete and written against the Flipper SDK APIs, but it has **not
been compiled in this environment** (no SDK/ufbt here). Run `ufbt` on your
machine; fix up any API drift against your firmware's headers if needed.

## Legal / scope

JailFlip only lists files and launches other already-installed apps — all
user-space, nothing that defeats device or firmware protections. Use it with
RF/NFC/RFID only on hardware and signals you own or are authorized to test.
