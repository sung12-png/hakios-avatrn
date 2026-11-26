#!/usr/bin/env python3
"""boot-now.py IMAGE [--no-boot]: `fastboot boot` IMAGE with this computer's time in epoch=.

The phone's firmware clock does not keep time (the loader's boot-epoch is then absent and Darwin starts
in 1969), and the PMIC's clock is a bare counter. The image is repacked into a temporary directory with
epoch= added; the original is not touched. Needs mkbootimg and fastboot."""
import os, struct, subprocess, sys, tempfile, time, zlib

ARRIVAL = 25  # seconds from here to the kernel's first look at the clock (an estimate)


def cmdline_of(image):
    d = open(image, 'rb').read()
    assert d[:8] == b'ANDROID!', 'not a boot image'
    size = struct.unpack_from('<I', d, 8)[0]
    body = zlib.decompressobj(31).decompress(d[4096:4096 + size])
    i = 0
    while (i := body.find(b'cmdline\0', i)) >= 0:
        n = struct.unpack_from('<I', body, i + 16)[0]
        if 0 < n < 700:
            return body[i + 24:i + 24 + n].rstrip(b'\0').decode()
        i += 8
    sys.exit('no command line found in ' + image)


def main():
    args = [a for a in sys.argv[1:] if a != '--no-boot']
    if len(args) != 1:
        sys.exit(__doc__)
    # (rtc_offset= goes too: epoch= wins over it, and the line has little room)
    words = [w for w in cmdline_of(args[0]).split() if not w.startswith(('epoch=', 'rtc_offset='))]
    here = os.path.dirname(os.path.abspath(__file__))
    with tempfile.TemporaryDirectory() as t:
        out = os.path.join(t, 'now.img')
        words.append('epoch=%d' % (time.time() + ARRIVAL))
        subprocess.check_call([sys.executable, '-I', os.path.join(here, 'repack.py'), args[0], out, ' '.join(words)])
        print(words[-1])
        if '--no-boot' in sys.argv:
            assert cmdline_of(out).split() == words
            print('not booted; the repacked image reads back as asked')
        else:
            subprocess.check_call(['fastboot', 'boot', out])


main()
