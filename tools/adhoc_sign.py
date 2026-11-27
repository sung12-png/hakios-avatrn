#!/usr/bin/env python3
"""adhoc_sign.py BINARY IDENTIFIER KEY...: give a thin arm64 Mach-O (linked with an ad-hoc signature) a new ad-hoc signature
that carries the boolean entitlements KEY... (XML and DER), as `ldid -S` would."""
import hashlib, struct, sys

def blob(magic, body):
    return struct.pack('>II', magic, 8 + len(body)) + body

def der(tag, body):
    n = len(body)
    length = bytes([n]) if n < 128 else bytes([0x80 | (n.bit_length() + 7) // 8]) + n.to_bytes((n.bit_length() + 7) // 8, 'big')
    return bytes([tag]) + length + body

def sign(path, ident, keys):
    data = bytearray(open(path, 'rb').read())
    magic, cpu, sub, kind, ncmds = struct.unpack_from('<IIIII', data, 0)
    assert magic == 0xfeedfacf, 'not a thin 64-bit Mach-O'
    off, sig, linkedit, text = 32, None, None, None
    for _ in range(ncmds):
        cmd, size = struct.unpack_from('<II', data, off)
        if cmd == 0x1d:
            sig = off
        if cmd == 0x19:
            name = bytes(data[off + 8:off + 24]).rstrip(b'\0')
            if name == b'__LINKEDIT':
                linkedit = off
            if name == b'__TEXT':
                text = struct.unpack_from('<QQ', data, off + 40)   # fileoff, filesize
        off += size
    assert sig and linkedit and text, 'no LC_CODE_SIGNATURE, __LINKEDIT or __TEXT'
    dataoff = struct.unpack_from('<I', data, sig + 8)[0]
    xml = ('<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" '
           '"http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n<plist version="1.0">\n<dict>\n'
           + ''.join('\t<key>%s</key>\n\t<true/>\n' % k for k in keys) + '</dict>\n</plist>\n').encode()
    pairs = b''.join(der(0x30, der(0x0c, k.encode()) + der(0x01, b'\xff')) for k in sorted(keys))
    others = {2: blob(0xfade0c01, struct.pack('>I', 0)), 5: blob(0xfade7171, xml),
              7: blob(0xfade7172, der(0x70, der(0x02, b'\x01') + der(0xb0, pairs)))}
    page, hlen, special = 4096, 32, 7
    ncode = (dataoff + page - 1) // page
    name = ident.encode() + b'\0'
    ident_off = 88
    hash_off = ident_off + len(name) + special * hlen
    cd_len = hash_off + ncode * hlen
    # sizes are known before the hashes: the load commands, which the first page holds, name the signature's size
    count = 1 + len(others)
    total = 12 + 8 * count + cd_len + sum(map(len, others.values()))
    total_padded = (total + 15) & ~15
    struct.pack_into('<I', data, sig + 12, total_padded)
    vmaddr, vmsize, fileoff, filesize = struct.unpack_from('<QQQQ', data, linkedit + 24)
    new_size = dataoff + total_padded - fileoff
    struct.pack_into('<Q', data, linkedit + 48, new_size)
    struct.pack_into('<Q', data, linkedit + 32, (new_size + 0x3fff) & ~0x3fff)
    del data[dataoff:]
    assert len(data) == dataoff
    cd = bytearray(struct.pack('>IIIIIIIIIBBBBIIIIQQQQ', 0xfade0c02, cd_len, 0x20400, 0x2, hash_off, ident_off, special, ncode,
                               dataoff, hlen, 2, 0, 12, 0, 0, 0, 0, 0, text[0], text[1], 1))
    assert len(cd) == ident_off
    cd += name
    for slot in range(special, 0, -1):
        cd += hashlib.sha256(others[slot]).digest() if slot in others else bytes(hlen)
    for i in range(ncode):
        cd += hashlib.sha256(bytes(data[i * page:min((i + 1) * page, dataoff)])).digest()
    assert len(cd) == cd_len
    slots = [(0, bytes(cd))] + sorted(others.items())
    index, body, at = b'', b'', 12 + 8 * len(slots)
    for kind_, b in slots:
        index += struct.pack('>II', kind_, at)
        body += b
        at += len(b)
    super_ = struct.pack('>III', 0xfade0cc0, at, len(slots)) + index + body
    assert len(super_) == total
    data += super_ + bytes(total_padded - total)
    open(path, 'wb').write(data)
    print('signed %s: %d code pages, %d entitlements' % (path.rsplit('/', 1)[-1], ncode, len(keys)))

sign(sys.argv[1], sys.argv[2], sys.argv[3:])
