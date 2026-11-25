# avatrn (Motorola Edge 2024, SM7435): working state, 2026-11-24

iOS 16.5 boots to the lock screen and Settings with the root on UFS; touch and buttons work.
Boot is RAM-only (`fastboot boot`). The iOS volume (userland/system.apfs) is in `userdata`, mounted
read-only.

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

## repack.py
Replaces cmdline (and optionally kernel and ramdisk) in a self-contained boot image:
    python3 -I avatrn/repack.py BASE.img OUT.img "CMDLINE" [KERNEL] [RAMDISK]
BASE.img used so far: xnu-boot-ufsnative-real.img (20 Nov, loader and UEFI inside).
