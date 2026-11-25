#!/usr/bin/env python3
"""Copy a self-contained xnu-boot image, changing only the XNUP 'cmdline' entry."""
import gzip, struct, subprocess, sys, zlib, tempfile, os
src, out, cmdline = sys.argv[1:4]
newkernel = sys.argv[4] if len(sys.argv) > 4 else None
newrd = sys.argv[5] if len(sys.argv) > 5 else None
newloader = sys.argv[6] if len(sys.argv) > 6 else None
d = open(src, 'rb').read()
assert d[:8] == b'ANDROID!'
ksize, rsize = struct.unpack_from('<II', d, 8)
assert struct.unpack_from('<I', d, 40)[0] == 4
kern = d[4096:4096 + ksize]
ramdisk = d[4096 + (ksize + 4095) // 4096 * 4096:][:rsize]
z = zlib.decompressobj(31); body = z.decompress(kern); tail = z.unused_data  # tail: appended DTB
i = 0
while True:
    i = body.index(b'XNUP', i)
    size = struct.unpack_from('<I', body, i + 4)[0]
    if body[i + 8:i + 14] == b'loader': break
    i += 4
p, end, entries, found = i + 8, i + 8 + size, b'', 0
swapped = False
while p + 24 <= end:
    name, sz, res = struct.unpack_from('<16sII', body, p)
    if not name.rstrip(b'\0'): break
    data = body[p + 24:p + 24 + sz]
    if newloader and name.rstrip(b'\0') == b'loader.efi':
        data = open(newloader, 'rb').read()
        assert data[:2] == b'MZ', 'not an EFI program'
        swapped = True
    if name.rstrip(b'\0') == b'cmdline':
        data = cmdline.encode() + b'\0'; found += 1
    if newkernel and name.rstrip(b'\0') == b'kernel':
        data = open(newkernel, 'rb').read()
        assert data[:4] == b'\xcf\xfa\xed\xfe', 'not a Mach-O'
    if newrd and name.rstrip(b'\0') == b'ramdisk':
        data = open(newrd, 'rb').read()
        assert data[32:36] == b'NXSB', 'not an APFS container'
    entries += struct.pack('<16sII', name, len(data), res) + data + b'\0' * (-len(data) % 16)
    p += 24 + sz + (-sz % 16)
assert found == 1 and len(cmdline) < 600
assert swapped or not newloader, 'no loader.efi entry replaced'
rest = body[p:end]               # end marker and padding
assert not rest.strip(b'\0') and not body[end:].strip(b'\0'), 'unexpected data after payload'
body_end = body[end:]
new = body[:i] + b'XNUP' + struct.pack('<I', len(entries) + len(rest)) + entries + rest + body_end
new += b'\0' * (-len(new) % 16)
# arm64 Image header image_size: ABL loads only this much, so it must cover the whole blob
assert struct.unpack_from('<Q', body, 16)[0] == len(body)
new = new[:16] + struct.pack('<Q', len(new)) + new[24:]
with tempfile.TemporaryDirectory() as t:
    open(f'{t}/k', 'wb').write(gzip.compress(new, 6) + tail)
    cmd = ['mkbootimg', '--header_version', '4', '--kernel', f'{t}/k', '-o', out]
    if rsize:
        open(f'{t}/r', 'wb').write(ramdisk); cmd += ['--ramdisk', f'{t}/r']
    osv = struct.unpack_from('<I', d, 16)[0]
    if osv:
        v, lv = osv >> 11, osv & 0x7ff
        cmd += ['--os_version', f'{v>>14}.{(v>>7)&127}.{v&127}', '--os_patch_level', f'{2000+(lv>>4)}-{lv&15:02}']
    subprocess.check_call(cmd)
print(os.path.basename(out), os.path.getsize(out), 'dtb tail', len(tail), 'ramdisk', rsize)
