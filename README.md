> **Fork for the Motorola Edge 2024 (avatrn, SM7435)** of
> [oosexgang/hakios-xnu-kernel](https://github.com/oosexgang/hakios-xnu-kernel). Experimental.
> Read [avatrn/README.md](avatrn/README.md) first; UEFI: [devices/avatrn/uefi](devices/avatrn/uefi/README.md).

# HakiOS

English · [中文](README.zh.md) · [Українська](README.uk.md)

**Apple's open-source Darwin kernel (XNU, xnu-8796.121.2) on Android phones with a Qualcomm SoC**, the way Android is Linux on
phones: a boot loader of its own, drivers of its own, a configuration system and a version of its own. The same kernel runs in
QEMU with nothing of Apple's and nothing of any phone's (`make qemu-open`).

**Documentation: [docs/](docs/index.md)**, also on the project's website (`hakios-web`). Start with [Getting started](docs/getting-started/index.md), [Port a device](docs/porting/index.md) or the
[Architecture](docs/concepts/architecture.md).

## What works on the Motorola Edge 2024

This fork's state, as seen on one phone (XT2405, SM7435, Android 16 based firmware). It is an experiment, not a
usable system. **Running it can lose data or damage hardware; you alone carry that risk.**

| Motorola Edge 2024 (`avatrn`) | State |
|---|---|
| Lock screen, home screen, stock apps | yes; opening Files > Safari and Settings > Notifications crashes |
| Touch and buttons | yes |
| All 8 CPU cores, thermal limits | yes |
| GPU (Adreno 710, OpenGL ES through Mesa) at 600 MHz | yes; not a steady 60 fps |
| Saving `/private/var` across reboots | yes |
| Sound from both speakers | yes |
| Battery level and charging | yes with a wall charger; a computer's port is too weak |
| Date and time zone | yes |
| Wi-Fi | yes: WPA2 networks joined from Settings, Safari browses, YouTube plays; no radio interrupts yet |
| Brightness, sleep and wake, microphone, headphones, vibration, flashlight | no |
| Bluetooth, cellular, cameras, Apple's services | no |

Boot is by `fastboot boot` (RAM) with the system on the phone's `userdata`, which **erases Android's data**.
How it is built: [avatrn/README.md](avatrn/README.md). The upstream project's own results are for the POCO X5 Pro 5G:
see [oosexgang/hakios-xnu-kernel](https://github.com/oosexgang/hakios-xnu-kernel). The rest of this README is upstream's.

**No ready-made images are offered.** A bootable image contains Apple's files and the phone maker's firmware, which
nobody here may pass on: each user builds their own from their own copies.

## The three layers

1. **The open core**: the kernel port, the loader, the drivers, the tools. All source, in these repositories.
2. **What you put on top**: the kernel can start a user space that you supply. It was built so that it can start the Darwin userland 16.5
   user space, which you lay out yourself from files you download from Apple; nothing of it is shipped, and nothing is
   circumvented to get it. Whether you may do that, and whether you want to, is your decision.
3. **Apple's services** (activation, Apple ID, iCloud, the App Store): not supported and not a goal.

By default the loader goes into RAM through `fastboot boot`, the system lives in RAM for a session, and a reboot brings the phone
back to its own Android. The one thing written by default is the loader's own log (a few KB) to the `logdump` partition; with
`ufs_write=1` the kernel writes only to a partition `darwin` that you create yourself. This is a research bench, not a ROM.

## Quick start

```sh
git clone https://github.com/oosexgang/hakios-xnu-kernel.git hakios
cd hakios
make doctor           # what this machine lacks, and what to do
make qemu-open        # the open kernel in QEMU: nothing of Apple's, nothing of a phone's
```

Darwin in QEMU (Linux, 12 GB of disk, Apple's IPSW downloaded for you), and a session on the phone:
[QEMU](docs/getting-started/qemu.md), [On the phone](docs/getting-started/phone.md).

```sh
make redwood_defconfig   # the phone's configuration: the common kernel + the SC7280's + the phone's own
make kernel              # the kernel with the drivers that configuration builds
make phone SECS=600      # a 10-minute session on the phone, in RAM
```

## Repositories: common, SoC, device, UEFI

| Repository | What |
|---|---|
| [hakios-xnu-kernel](https://github.com/oosexgang/hakios-xnu-kernel/tree/main) | **common**: the kernel port, every common driver, the loader, user space, the tools, this documentation, and `devices/qemu` |
| [hakios-sm7325](https://github.com/oosexgang/hakios-sm7325) | POCO X5 Pro 5G: its defconfig, status, UEFI platform, firmware script, and its SoC's layer (`soc/`: the Qualcomm SC7280 family) |
| [hakios-uefi](https://github.com/oosexgang/hakios-uefi) | the firmware that starts the loader: Mu-Silicium with HakiOS's boot path, banner and name, and HakiOS's U-Boot (`u-boot/`) |
| [hakios-metal](https://github.com/oosexgang/hakios-metal) | Metal's command model on the Adreno, through the kernel's GPU driver |

`devices/manifest.conf` in the common repository names each one and the revision it is at; `tools/hakios-device.py sync` clones them
into `devices/` and `uefi/`. One repository per device: it carries its SoC's layer, and `tools/hakios-device.py new` makes one. [How the repositories fit](docs/porting/repositories.md).

```sh
git clone https://github.com/oosexgang/hakios-xnu-kernel.git hakios && cd hakios
tools/hakios-device.py sync      # the SoC, device and UEFI repositories, at the revisions the manifest names
```

A device of your own is a repository over the common one: `tools/hakios-device.py new` makes it from the phone's own device tree,
with every phone driver left out until it is ported. Each driver is an option of the kernel's own configuration (`CONFIG_HAKIOS_*`,
XNU's `options` and `conf/files`), so a device's defconfig builds exactly the drivers it has.
[Port a device](docs/porting/index.md), [Drivers in the configuration](docs/drivers-config.md).

## What is in the repository, and what never is

In it: only our own code and data: the patch to Apple's open-source XNU (`xnu/hakios.patch`, APSL 2.0) and our files, the boot
loader, our programs and libraries, the tools, and the device and SoC directories. This fork carries no built files: `make loader` and `make kernel`
build them on your machine (the loader that starts Darwin from your own IPSW).

Never in it: any file of Darwin (you build the system image on your machine from an IPSW you get yourself and hand to
`sysimage/unpack.sh`; nothing is downloaded for you, decrypted or circumvented); the phone's firmware (pulled from your own phone into
`fw-*/`, which git ignores); keys, certificates, activation, or anything that would make the phone pass as an iPhone to Apple.

## Independent, and Apple-free at the core

HakiOS is an independent research project. It is **not affiliated with or endorsed by Apple**;
"Darwin", "iPhone", "XNU" and "Apple" are named only to describe what the code works with. The core
kernel, the boot loader and the open tools build and run with nothing of Apple's:

```sh
make qemu-open      # the open kernel + a placeholder loader + a tiny raw-syscall user space, in QEMU
```

No IPSW, no Darwin user space, no firmware. Booting the stock Darwin user space (`make qemu`,
`make phone`) is an optional path that needs the IPSW and the phone's firmware, which you supply
from your own device.

## Licenses and the legal side

My code is under the license of the files it changes: the XNU patch under the Apple Public Source
License 2.0 (Apple publishes XNU under it; the terms are met: changes are published as source,
notices are kept), the rest as the file headers say. Darwin, its images, Apple's trademarks and the
phone's firmware belong to their owners, are not in this repository and are not distributed by me;
everyone takes them from their own phone and their own IPSW. "Darwin", "iPhone" and "Apple" are
named only to describe what the code works with. If you believe a file here infringes anyone's
rights, open an issue and I will remove it.

The detail is in separate files: `LEGAL.md` (what is and is not distributed, and the limits of
these statements), `COMPLIANCE.md` (how the APSL 2.0 and other terms are met, and the items still
to verify), `THIRD_PARTY_LICENSES.md` (every third-party component with origin and license), and
`NOTICE` (the short canonical list). None of this is legal advice.

hakios is an experimental project focused on bringing darwin to android hardware. our goal is to make hakios free and accessible to everyone.

owners:
@oodze - founder
@netusd - adversting / ceo-dev
@wannqn - dev 
@hxfuxyyXD - dev
Rhaenja Hun（杭润嘉） — Developer / Sponsor

tym sicilium for the sources!

