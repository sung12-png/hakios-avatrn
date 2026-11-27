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

## Known problems (2026-11-26)
- With the trimmed system image, opening search restarts the home screen. With all of Apple's files it
  needs libnodictation (below).
- searchd stops itself without SpotlightServices' ranking models, which select-system.py leaves out
  (userland/src/init.c overlays them if the boot image has /overlay-spotlight).
- Settings > Notifications crashes Settings (cause not found yet).
- fbscale=2 is faster, but SpringBoard shows a red strip and icons cached at 3 pixels a point come out
  too large.
- Frame drops in the App Library; typing and the system at large are on the slow side. No modem here; sound is from the speakers only.

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
Icons and widgets showed small pieces missing or added until tools/mesa/z-a7xx-fb-read-wait.patch:
every framebuffer-reading draw waits for the GPU to be idle. Verified on the phone, power-off screen
included. What the test builds said: the wait alone cures it; a CCU flush and UCHE invalidate without
it do not; neither do sysmem, noubwc, nolrz or inorder; glfetch=0 cures the icons but makes the
power-off slider's text flash. A new Mesa goes into the RAM image's lib/ and redwood/lib/.
adreno_speed 1, 2, 3, 4 = 295, 345, 500, 600 MHz. 4 is what is used now: reported faster, without
artifacts (a short test; not watched under long load). See Thermals. Kernel also needs CONFIG_HAKIOS_ADRENO=y.

The RAM image needs, besides the above:
- fw/ (stored plain): a710_sqe.fw, a710_zap.mdt, .b00 to .b02, gmu_gen70000.bin from the phone's own
  /vendor/firmware. Motorola's files: never commit them or pass on an image that holds them.
- lib/libOSMesa.8.dylib: tools/mk-mesa.sh (needs meson; its venv in xnu-deps/build/mesa/venv).
- lib/libhakios.gl.dylib: userland/build.sh GL=1 builds it; where theos's ld cannot read the SDK,
  add `--ld-path=/usr/bin/ld64.lld -mlinker-version=705` to its compile line (as mk-mesa.sh does).
  The one used here was compiled by hand that way, after mkeagl.py made eagl_gen.h.

## Metal (experimental, 2026-11-27: not in the everyday image)
iOS draws through Metal on the Adreno 710 with ios-garnet's driver (hakios-metal, MIT) and its CoreAnimation
plug-in (userland/src/HakiosMetalCA.m there), both from rafsanbasunia/ios-garnet at 3fa923f; neither is copied
into this repository. What was seen on the phone:
- a test program: the device is made, a buffer reads back, copy and fill are done by the GPU;
- the window server alone given Metal, from an image that saves nothing: lock screen, home screen, page swipe
  and Control Center draw, with short one-pixel black dashes in places (not yet cured). That the window server
  really drew these frames through Metal, and not through OpenGL ES, has not been confirmed from a log.
How it was built, on Linux:
- a Mesa tree of its own with garnet's tools/mk-mesa.sh and tools/mesa (the ld64.lld linker lines added), then in
  hakios-metal `make ios HAKIOS=<that tree> MESA=1`, and the plug-in linked as garnet's userland/build.sh does it
  with METAL_PLUGIN=ca;
- CoreAnimation's shader library: garnet's tools/ca-metallib.py on iOS 16.5's QuartzCore default.pipelinelib, with
  CA_QC_METALLIB= a later QuartzCore default.metallib (here iPadOS 17.0.2's). Apple's file: never commit it.
  It goes to /redwood/share/ca-share/ in the RAM image.
Here: boot-arg metal=2 (the system volume has no link to the plug-in in /System/Library/Extensions, so the name
published leads back to /redwood/lib/HakiosMetal.bundle) and ca_metallib=PATH (userland/src/inject.c).
Boot arguments used: metal=2 ca_metallib=/redwood/share/ca-share/QuartzCore-default.metallib. The daily command line
has no room left for them.

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

## Battery and charging (the audio DSP)
    adsp_step=11 adsp_keep=1 adsp_battery=1 adsp_new=12 usb_ma=0
The audio DSP is started during boot and runs the charger and fuel gauge: a true percentage, and the phone
charges while iOS runs (seen: 55 to 61 % on a wall charger; on a computer's port it still drains slowly).
Kernel: CONFIG_HAKIOS_AVATRN_ADSP=y and CONFIG_HAKIOS_BATTERY=y (drivers/audio/hakios.avatrn_adsp.c,
from ios-garnet). The RAM image needs fw/adsp.mdt and fw/adsp.bNN from the phone's own firmware (its
modem partition's image/ directory), stored plain: the vendor's, never commit them. usb_ma=0 leaves a
computer port's 400 mA limit alone (without it the driver asks for 500). Speaker steps (12-15) are not run.
Not right yet: the temperature reads 2 (hundredths of a degree) and the cycle count repeats the design
capacity: this firmware's property numbers differ. Do not rely on either.

The older way, without the DSP: battery=1 battery_by_voltage=1 battery_no_charger=1 -- the loader asks the
firmware's charger drivers, whose percentage stayed at 8 % here; an estimate from voltage, no charging.

## Sound (both speakers, through the same audio DSP)
    adsp_step=13 adsp_keep=1 adsp_battery=1 adsp_new=14 usb_ma=0 sound_below=96
Kernel: CONFIG_HAKIOS_AVATRN_ADSP, _SPEAKER and _SOUND (drivers/audio/hakios.avatrn_*.c, from ios-garnet).
AudioReach graph into the island's primary I2S (48 kHz, 32 bits, 2 channels; pins 6-8), two AW88261 at I2C
0x35 and 0x34 on GPIO 24/25 with no reset lines, set up from the phone's own aw882xx_acf.bin ("Music"),
which goes into the RAM image's fw/ (stored plain; the vendor's, never commit it). The speakers' guard
limits what they get; sound_below= is eighths of a dB under Android's level (96: 12 dB; the first try).
Steps to look with, without iOS: speaker_probe=2, adsp_step=14 (tone, amplifiers off), =15 (audible, 2 s).
Not done: microphones, the earpiece as its own route, headphones, Bluetooth, calls.

## Wi-Fi (works: Safari browses, YouTube plays)
Inside iOS: wifi=2 (avatrn's settings, no interrupts, a made-up address unless wifi_mac=). Wait for the tick in
Settings before browsing. The RAM image needs:
- the radio's files in fw/ (wpss.mdt, wpss.bNN, regdb.bin, bdwlan.elf.avatrn.epa);
- in LaunchDaemons/, com.hakios.wifi.plist and Apple's com.apple.wifid.plist with _LimitLoadToDeviceTree removed
  (init binds this directory over the system's, so a job not in it does not run);
- for Safari not to freeze on the second page: com.apple.Safari.History and webbookmarksd, which are in Apple's
  App cryptex (the 10 MB image in the IPSW), not in the system volume. Here they go in the RAM image's libexec/
  and their jobs' Program is changed to /redwood/libexec/NAME. Apple's files: never commit them.
Appending the whole App cryptex to the RAM image made a boot image the loader did not start (about 67 MB
unpacked). Not done: the radio's interrupts (its rings are polled every 100 ms).

iOS's own auto-join does not work here (wifid knows the network and its password, but CoreWiFi's auto-join scan is
refused before it reaches the driver). userland/src/autojoin.c asks wifid to join a saved network the way a tap in
Settings does; build and sign it as its first comment says (tools/adhoc_sign.py: ldid's job, for hosts without it).

With Wi-Fi and sound in one kernel the 8 MB XNU keeps for device register maps is too small (the speakers went
silent): apply avatrn/io_map-64mb.patch to the XNU tree (xnu-src/xnu-8796.121.2, patch -p1) before building.

## Brightness and sleep (work: iOS's slider; the power button darkens and restores)
The panel (video mode, refreshing at 144 Hz while iOS makes 60 frames a second) takes DCS 0x51 over DSI0; the
driver is in MiniIOMobileFramebuffer.cpp, after iOSPlus10's (MIT, see NOTICE). On with gpu_soc=sm7435; backlight=0
turns it off, backlight_max=N sets the slider's top (2047 of the panel's 4095). The slider never turns the screen off. Display off
(RequestPowerChange 0) sends level 0 and on, or the power button going down, restores it; the panel and the SoC are
not put to sleep, so it saves the panel's power only. fb_measure=1 also says the panel link's mode and its refresh rate.

## Apps without a graphics chip
Settings > Notifications crashed in CoreImage (CI::GLContext::init, from MobileIcons scaling the apps' icons): apps
have no GL driver here, and kCIContextUseSoftwareRenderer is not honoured on that road. userland/src/appicons.m draws
those icons with Core Graphics instead; it is linked into libnodictation.dylib together with nodictation.c:
`clang ... -fobjc-arc -dynamiclib -install_name /redwood/lib/libnodictation.dylib -framework Foundation -framework
UIKit -framework CoreGraphics userland/src/nodictation.c userland/src/appicons.m`.
## Files
Files stopped itself when its view was switched: fileproviderd turned it away ("denying connection because the
device is not set up") until Apple's setup assistant has run. userland/src/pastbuddy.c, inserted into fileproviderd
(avatrn/kern_exec-inject-default.patch), sets FPDServer's _isPastBuddy byte before it is used:
`clang ... -dynamiclib -install_name /redwood/lib/libpastbuddy.dylib userland/src/pastbuddy.c`.
Seen on the phone: Files stays open. Still open: "On My iPhone" is offered in Browse > Edit but never appears; the
LocalStorage extension is found and never started.

## Low battery
With the battery read through the audio DSP (adsp_battery=1), a battery at boot-arg battery_off percent or less (3; 0:
never) that is still draining (three readings over 20 s) has the file systems written out and the phone switched off
(PSCI SYSTEM_OFF). Seen on the phone with battery_off=100. iOS gives no warning first.

## Date
Time zones: /usr/share/zoneinfo points into /private/var, which nothing had filled, and only Cupertino
could be picked. The RAM image's etc/rc.jobs now runs /usr/libexec/tzinit (Apple's own, in the system
image); Chicago was then found and set.
The firmware's clock does not keep time here (no boot-epoch: Darwin starts in 1969), and the PMK8350's
clock is a bare counter that runs while the phone is off. Two ways, used together or alone:
- rtc_offset=N (hakios.avatrn_buttons.c, needs buttons=1): N = the computer's `date +%s` minus
  /sys/class/rtc/rtc0/since_epoch read by adb in the recovery system. If the battery has run completely
  flat the counter may have restarted: measure again.
- avatrn/boot-now.py IMAGE: boots with this computer's time in epoch=.

## The command line has a limit
607 characters reach the kernel, and the loader puts its own in front; the END of ours is cut off and
the kernel never hears of it (this is how rtc_offset= went missing). repack.py now blanks the
kernel.ver entry (59 characters of hakios_kver=) and refuses a line over 506.

## Keyboard (libnodictation)
A program has the fix only if the kernel's list names it: Settings' keyboard froze until Preferences and
the other applications with keyboards were added (the list is past 512 characters: the buffer is 1024).
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
