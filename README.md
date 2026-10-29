# JailFlip

A **mod hub** for the Flipper Zero, packaged as a single app (`.fap`).

> ⚠️ "Jailbreak" is borrowed loosely here. The Flipper Zero is **not** a locked
> device — its firmware is open source. JailFlip doesn't bypass any security;
> it's a convenience launcher built on the normal SDK. The name is just a nod
> to the iOS scene (Sileo → **Flipeo**).

## Features

| Module | What it does |
| --- | --- |
| **Flipeo** | Sileo-style browser of installed plugins. Scans `/ext/apps/<Category>/*.fap`, lists them, and launches the selected one through the system loader. |
| **Boot Logo** | Pick a `128x64` `.bmx` image from the SD card and stage it as a boot splash (applied at startup by custom firmwares that support a user boot logo, e.g. Momentum). |
| **Keys** | Read-only browser over your stored `SubGHz` / `NFC` / `iButton` / `LF RFID` files. Inspect what you've saved; emulation stays in the dedicated apps. |
| **About** | What it is, in-device. |

## Project layout

```
application.fam     # app manifest (ufbt/fbt)
jailflip.c          # entry point, main menu, view dispatcher
jailflip_i.h        # shared app struct + declarations
jailflip_store.c    # Flipeo plugin browser + launcher
jailflip_keys.c     # key-file browser
jailflip_logo.c     # boot-logo staging
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

### Target firmware

Builds against the current official SDK by default. To target a specific
firmware (Momentum / Unleashed / RogueMaster):

```bash
ufbt update --channel=release          # official
# or point ufbt at a custom firmware's SDK per that firmware's docs
```

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
