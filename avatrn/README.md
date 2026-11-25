# avatrn (Motorola Edge 2024, SM7435): working state, 2026-11-24

iOS 16.5 boots to the lock screen and Settings with the root on UFS; touch and buttons work.
Boot is RAM-only (`fastboot boot`). The iOS volume (userland/system.apfs) is in `userdata`, mounted
read-only.

## Read this first
Experimental. Use at your own risk, only on your own phone.
- **Slows down when hot only if asked** (see Thermals). Without those boot arguments nothing does. Do not
  leave it unattended.
- Saving writes to `userdata` (a fixed window of it); flashing the system image replaces `userdata`.
- You need your own firmware: Apple's IPSW and the phone's GPU firmware. Neither is in this repository.

## Known problems (2026-11-25)
- With the trimmed system image, opening search restarts the home screen. With all of Apple's files it
  needs libnodictation (below).
- searchd stops itself without SpotlightServices' ranking models, which select-system.py leaves out
  (userland/src/init.c overlays them if the boot image has /overlay-spotlight).
- Frame drops in the App Library. The battery icon is wrong. No sound, Wi-Fi or modem here.

## Boot arguments of the working image
    sb_delay=8 root_apfs=ufs_userdata ufs=2 ufs_native=1 bootcpu=7 cpus=8 touchscreen=1 touch_avdd=1
    buttons=1 amfi_get_out_of_my_way=1 cs_enforcement_disable=1
(add -v for the log). Diagnostics: touch_delay=N touch_trace=1 touch_step=N touch_boot_ms=N.

## GPU-accelerated iOS (Adreno 710)
The window server draws by the GPU through Mesa; reported a lot smoother, with some frame drops
(500 MHz, CPUs at top speed). Added to the boot arguments above:

    adreno=21 adreno_keep=1 adreno_pause=10 gpu_soc=sm7435 adreno_speed=3
    cpufreq=1 soc_epss=0x17d91000 epss_cores=4 soc_rsc=0x17a20000

(no gpu_delay: the GPU has to be there before the first program; gl=0: software drawing again).
adreno_speed 1, 2, 3 = 295, 345, 500 MHz. Nothing throttles the GPU when hot: watch the log's
temperature line. Kernel also needs CONFIG_HAKIOS_ADRENO=y.

The RAM image needs, besides the above:
- fw/ (stored plain): a710_sqe.fw, a710_zap.mdt, .b00 to .b02, gmu_gen70000.bin from the phone's own
  /vendor/firmware. Motorola's files: never commit them or pass on an image that holds them.
- lib/libOSMesa.8.dylib: tools/mk-mesa.sh (needs meson; its venv in xnu-deps/build/mesa/venv).
- lib/libhakios.gl.dylib: userland/build.sh GL=1 builds it; where theos's ld cannot read the SDK,
  add `--ld-path=/usr/bin/ld64.lld -mlinker-version=705` to its compile line (as mk-mesa.sh does).
  The one used here was compiled by hand that way, after mkeagl.py made eagl_gen.h.

## Saving (/private/var kept between boots)
Verified: dark mode and iOS's data are there after a restart. Add to the boot arguments:

    ufs_write=1 ufs_window=userdata var_hfs=ufs_darwin_data

/private/var is then a 16 GB journaled HFS Plus volume in blocks 8388608..12582911 of userdata (32 GiB
in; the system image is at its start). Loader and kernel each refuse a write anywhere else;
ufs_window_dry=1 shows the range and writes nothing. No partition table change.

Needed once: the empty volume (needs mkfs.hfsplus; flashing it again empties it):

    python3 tools/mkdatavol.py --userdata datavol.simg && fastboot flash userdata datavol.simg

Needed in the image: userland/src/init.c built (the prebuilt init is older and does not mount it), as
/sbin/init, /sbin/launchd and the /redwood copies; and a loader built as below.
No orderly shutdown: what changed in the last seconds before the power button may be lost. Once writing
has started the kernel reads through the loader too, not by its own driver.

## Thermals
    thermal=1 thermal_soc=sm7435 thermal_throttle=1 thermal_stop=95
(kernel with CONFIG_HAKIOS_THERMAL=y). By the hottest of 28 SoC sensors: at 60 / 70 / 80 C the processors'
top is cut to 70 / 55 / 40 % and the GPU asked for 345 / 295 / 295 MHz; at thermal_stop the run ends.
thermal_warm=, thermal_hot=, thermal_crit= change the thresholds. No sensor of the case is read. Checked
on the phone with thresholds set low (GPU message accepted, processors read back at 691 and 960 MHz); not
yet seen under real heat.

## Keyboard (libnodictation)
With no dictation service running, the keyboard was told over and over that dictation's availability had
changed, and SpringBoard did nothing else for minutes: no touch or button was answered. Found by sampling
its main thread. userland/src/nodictation.c ignores those two calls; search was then typed in on the phone.
Build it (see the linker note above for lld), put it in the RAM image as lib/ and redwood/lib/, and name it
in the kernel's insertion list (kern_exec-inject-default.patch). The kernel's list and the image go
together: a program named for a library that is not there does not start.

    xnu-clang -arch arm64 -isysroot $THEOS/sdks/iPhoneOS16.5.sdk -O1 -dynamiclib \
        -install_name /redwood/lib/libnodictation.dylib -framework UIKit -lobjc \
        userland/src/nodictation.c -o libnodictation.dylib

## Kernel
Build with CONFIG_HAKIOS_UFS, CPUFREQ, AVATRN_GOODIX, AVATRN_BUTTONS = y (tools/make-kernel.sh).

## Two things that are NOT yet fixed properly (a fresh build loses them)
1. Library insertion. In a flash-root boot init runs as /sbin/launchd, so after the switch of root
   in_image("/etc/inject.conf") points into iOS and nothing is inserted. Worked around by
   `kern_exec-inject-default.patch` (apply to xnu-src/xnu-8796.121.2). Proper fix: in userland/src/init.c.
2. /redwood in the RAM image. userland/build.sh makes /redwood/{bin,etc,lib,share} relative links,
   which after the bind resolve into iOS. The working RAM image was rebuilt with real copies of
   bin, etc, lib, share and sbin there:
       cp -a userland/root X; (cd X/redwood; for d in bin etc lib share sbin; do rm -f $d; cp -a ../$d $d; done)
       (cd userland; python3 ../tools/apfs/mkapfs.py -z --plain fw -o rd.apfs redwood=X)

## Loader
Build it with this phone's payload address, or it stops right after the firmware's "StartImage...":

    cd xnuloader && PAYLOAD_BASE=0xF4000000ULL ./build.sh

(the script's default, 0xF0000000, is another phone's; Mu-Silicium's avatrn memory map has "XNU Payload" at 0xF4000000)

## repack.py
Replaces cmdline (and optionally kernel and ramdisk) in a self-contained boot image:
    python3 -I avatrn/repack.py BASE.img OUT.img "CMDLINE" [KERNEL] [RAMDISK]
BASE.img used so far: xnu-boot-ufsnative-real.img (20 Nov, loader and UEFI inside).
