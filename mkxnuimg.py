#!/usr/bin/env python3
"""
Pack a phone's UEFI (with the hook that starts the XNU loader) and an optional payload
into an Android boot image for `fastboot boot`.

  kernel = gzip(BootShim + UEFI FD + ["XNUP" container]) + DTB

The boot shim copies the FD to its base and the payload to the UEFI's payload base.

The UEFI pieces are taken from a Mu-Silicium tree built here (Mu-Silicium/ or $MU_SILICIUM,
see uefi/README.md) or, when there is none, from devices/<device>/prebuilt/.

usage: mkxnuimg.py [-d DEVICE] [-p payload.bin] [-o out.img] [--mode MODE]
"""
import argparse, gzip, os, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))

# Per device: the EDK2 package, where the FD is linked, how big it is, and what the
# BootManagerLib in it reads the XNUP payload from (XnuLoader.h, XNU_PAYLOAD_BASE).
DEVICES = {
    'redwood': dict(pkg='redwoodPkg', fd_base=0x9FD00000, fd_size=0x200000,
                    payload_base=0xF0000000, modes=('RELEASE', 'DEBUG'), header_version=1),
    'avatrn': dict(pkg='avatrnPkg', fd_base=0xA7100000, fd_size=0x300000,
                   payload_base=0xF4000000, modes=('RELEASE', 'DEBUG'), header_version=4),
    'borneo': dict(pkg='borneoPkg', fd_base=0x5FC00000, fd_size=0x300000,
                   payload_base=0xF4000000, modes=('RELEASE', 'DEBUG'), header_version=1),
}

ap = argparse.ArgumentParser()
ap.add_argument('-d', '--device', default='redwood', choices=sorted(DEVICES))
ap.add_argument('-p', '--payload')
ap.add_argument('-o', '--output')
ap.add_argument('--mode', help='RELEASE or DEBUG (default: the device\'s usual build)')
ap.add_argument('--mu', help='the Mu-Silicium tree (default: $MU_SILICIUM or Mu-Silicium/)')
ap.add_argument('--fd', help='the UEFI image, when it is not in the tree')
ap.add_argument('--header-version', type=int, choices=(1, 4),
                help='the Android boot image header (default: the device\'s: 1 for redwood, 4 for avatrn)')
args = ap.parse_args()

dev = DEVICES[args.device]
MU = args.mu or os.environ.get('MU_SILICIUM') or os.path.join(HERE, 'Mu-Silicium')
PREBUILT = os.path.join(HERE, 'devices', args.device, 'prebuilt')


def first(*paths):
    for path in paths:
        if path and os.path.isfile(path):
            return path
    return paths[0]


fd_path = args.fd
if not fd_path:
    for mode in (args.mode,) if args.mode else dev['modes']:
        candidate = os.path.join(MU, 'Build', dev['pkg'], f'{mode}_CLANGPDB', 'FV', 'SILICIUM_UEFI.fd')
        if os.path.isfile(candidate):
            fd_path = candidate
            break
if fd_path and os.path.isfile(fd_path):
    shim_dir = os.path.join(MU, 'BootShim')
    for f in ('BootShim.elf', 'BootShim.bin'):
        try:
            os.unlink(os.path.join(shim_dir, f))
        except FileNotFoundError:
            pass
    # The shim copies the FD to FD_BASE and the XNUP payload that follows it to XNUP_PAYLOAD_BASE.
    subprocess.check_call(['make', 'REQUIRES_KERNEL_HEADER=1', f'FD_BASE={dev["fd_base"]:#x}',
                           f'FD_SIZE={dev["fd_size"]:#x}',
                           f'CFLAGS=--target=aarch64-linux-gnu -DPAYLOAD_BASE={dev["payload_base"]:#x}'],
                          cwd=shim_dir, stdout=subprocess.DEVNULL)
    parts = {'shim': os.path.join(shim_dir, 'BootShim.bin'), 'fd': fd_path,
             'dtb': first(os.path.join(MU, 'Resources', 'DTBs', f'{args.device}.dtb'),
                          os.path.join(PREBUILT, 'device-tree.dtb')),
             'ramdisk': first(os.path.join(MU, 'Resources', 'ramdisk'), os.path.join(PREBUILT, 'ramdisk')),
             'mkbootimg': first(os.path.join(MU, 'Resources', 'Scripts', 'mkbootimg.py'),
                                os.path.join(PREBUILT, 'mkbootimg.py'))}
else:
    parts = {'shim': os.path.join(PREBUILT, 'BootShim.bin'), 'fd': os.path.join(PREBUILT, 'SILICIUM_UEFI.fd'),
             'dtb': os.path.join(PREBUILT, 'device-tree.dtb'), 'ramdisk': os.path.join(PREBUILT, 'ramdisk'),
             'mkbootimg': os.path.join(PREBUILT, 'mkbootimg.py')}
for name, path in parts.items():
    if not os.path.isfile(path):
        hint = ''
        if name == 'fd':
            hint = ("\n  The UEFI image is not in the repository: it carries the phone vendor's own drivers, which are not\n"
                    "  redistributed here. Build it from your own phone's firmware: uefi/README.md")
        sys.exit(f'mkxnuimg.py: {path} not found{hint}')
shim = open(parts['shim'], 'rb').read()
fd = open(parts['fd'], 'rb').read()
assert len(fd) == dev['fd_size'], f'{parts["fd"]}: {len(fd):#x} bytes, expected {dev["fd_size"]:#x}'

blob = shim + fd
if args.payload:
    data = open(args.payload, 'rb').read()
    # the payload file is a complete "XNUP" container (see xnuloader/mkpayload.py)
    assert data[:4] == b'XNUP' and len(data) <= 0x04000000 - 0x1000, 'bad or oversized payload'
    blob += data
    blob += b'\0' * (-len(blob) % 16)
    print(f'payload: {len(data)} bytes, bytesum={sum(data) & 0xffffffff:08x}')
# The arm64 header's image_size (the shim's _StackSize) tells ABL how much of the kernel to load. The
# payload is appended after the FD, so it must cover the whole blob, not only the FD.
blob = bytearray(blob)
struct.pack_into('<Q', blob, 16, len(blob))

dtb = open(parts['dtb'], 'rb').read()
out = args.output or os.path.join(HERE, f'xnu-{args.device}.img')
header_version = args.header_version or dev['header_version']
kernel = out + '.kernel~'
# Header 1 keeps the device tree after the gzip stream. Header 4 has no field for it, and
# avatrn's ABL reboots the phone about 8 s after `fastboot boot` when one is appended.
open(kernel, 'wb').write(gzip.compress(blob, 6) + (dtb if header_version == 1 else b''))
try:
    subprocess.check_call([sys.executable, parts['mkbootimg'], '--kernel', kernel, '--ramdisk', parts['ramdisk'],
                           '--header_version', str(header_version), '-o', out])
finally:
    os.unlink(kernel)
print(f'{out}: {os.path.getsize(out)} bytes, header v{header_version} ({args.device}: shim {len(shim)}, '
      f'fd {len(fd)} at {dev["fd_base"]:#x}, payload at {dev["payload_base"]:#x}, uncompressed {len(blob)})')
