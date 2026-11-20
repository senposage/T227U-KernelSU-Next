#!/usr/bin/env python3
"""
Apply the T225 vendor-module CRC overrides to a freshly built vmlinux, by SYMBOL NAME
(robust against export-order changes). No vendor .ko files needed.

  apply_crc_overrides.py <vmlinux> <crc_overrides.json> [--gen <patched_vmlinux>]

Normal use:   apply_crc_overrides.py out/vmlinux crc_overrides.json
   -> patches out/vmlinux in place (kcrctab / kcrctab_gpl). Then regenerate Image with llvm-objcopy.
Generate use: --gen <already_patched_vmlinux>  writes the overrides json from an
   unpatched/patched vmlinux pair (only needed if you ever get new .ko-derived CRCs).
"""
import subprocess, re, struct, sys, json

def sections(vm):
    out = subprocess.run(['readelf','-S','-W',vm],capture_output=True,text=True).stdout
    s = {}
    for l in out.split('\n'):
        m = re.match(r'\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)', l)
        if m: s[m.group(1)] = (int(m.group(2),16), int(m.group(3),16), int(m.group(4),16))
    return s

def table(vm, data):
    """return list of (name, file_offset_of_crc) for __ksymtab and __ksymtab_gpl"""
    s = sections(vm)
    sa, so, _ = s['__ksymtab_strings']
    res = []
    for kind in ('', '_gpl'):
        ka, ko, kz = s['__ksymtab'+kind]
        ca, co, cz = s['__kcrctab'+kind]
        n = kz // 8
        assert cz // 4 == n, f'ksymtab{kind}/kcrctab{kind} size mismatch'
        for i in range(n):
            fa = ka + i*8 + 4                       # address of name_offset field
            noff = struct.unpack_from('<i', data, ko + i*8 + 4)[0]
            naddr = fa + noff
            p = so + (naddr - sa)
            e = data.index(b'\0', p)
            res.append((data[p:e].decode(), co + i*4))
    return res

def main():
    if len(sys.argv) < 3: print(__doc__); sys.exit(1)
    vm, js = sys.argv[1], sys.argv[2]
    data = bytearray(open(vm,'rb').read())
    tab = table(vm, data)
    if len(sys.argv) >= 5 and sys.argv[3] == '--gen':
        pat = bytearray(open(sys.argv[4],'rb').read())
        ov = {}
        for name, off in tab:
            a = struct.unpack_from('<I', data, off)[0]; b = struct.unpack_from('<I', pat, off)[0]
            if a != b: ov[name] = {'crc': '0x%08x' % b, 'orig': '0x%08x' % a}
        json.dump(ov, open(js,'w'), indent=0, sort_keys=True)
        print('wrote', len(ov), 'overrides'); return
    ov = json.load(open(js))
    idx = {}
    for name, off in tab: idx.setdefault(name, []).append(off)
    done = missing = changed = 0
    for name, v in ov.items():
        if name not in idx: missing += 1; print('  not exported by this kernel (skipped):', name); continue
        for off in idx[name]:
            cur = struct.unpack_from('<I', data, off)[0]
            if cur != int(v['orig'],16) and cur != int(v['crc'],16):
                print(f'  NOTE {name}: unpatched CRC is {cur:#010x}, expected {v["orig"]} -- kernel ABI changed here, review!')
            struct.pack_into('<I', data, off, int(v['crc'],16)); done += 1
    open(vm,'wb').write(data)
    print(f'patched {done} CRC entries, {missing} symbols not present in this kernel')
if __name__ == '__main__': main()
