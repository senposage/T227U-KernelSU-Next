#!/usr/bin/env python3
"""Patch and verify the SM-T225 kernel and peer-module CRC map."""

import argparse
from collections import defaultdict
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

HERE = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(HERE.parent / "t227u"))
from apply_vendor_crc_overrides import table

USERCOPY = "__check_object_size"
# These exports differ from T227U's target values. Keep every other local
# override within the previously reviewed set shared by both devices.
T225_OVERRIDES = {"init_task", "seq_putc", "bpf_trace_run3", "contig_page_data"}


def read_expectations():
    expected = {}
    for line in (HERE / "vendor_expected_crcs.txt").read_text().splitlines():
        symbol, crc = line.split()
        if symbol in expected or not re.fullmatch(r"0x[0-9a-fA-F]{8}", crc):
            raise ValueError(f"invalid or duplicate CRC entry: {line}")
        expected[symbol] = int(crc, 16)
    if len(expected) != 523 or expected.get(USERCOPY) != 0x88DB9F48:
        raise ValueError("incomplete T225 CRC list or wrong usercopy CRC")
    return expected


def read_module_exports(path):
    output = subprocess.check_output(["nm", "--defined-only", path], text=True)
    exports = set(re.findall(r"\s__ksymtab_(\w+)$", output, re.MULTILINE))
    crcs = re.findall(r"^([0-9a-fA-F]+)\s+A\s+__crc_(\w+)$", output, re.MULTILINE)
    return {name: int(crc, 16) for crc, name in crcs if name in exports}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("vmlinux", type=Path)
    parser.add_argument("modules", type=Path, help="directory containing WMT provider modules")
    parser.add_argument("--write", type=Path, help="write a verified, patched copy of vmlinux")
    args = parser.parse_args()

    expected = read_expectations()
    providers = json.loads((HERE / "vendor_module_providers.json").read_text())
    shared = json.loads((HERE.parent / "t227u/vendor_module_crc_overrides.json").read_text())
    if any(expected.get(name) != int(entry["crc"], 16) for name, entry in shared.items()):
        raise ValueError("shared override map disagrees with the T225 expectations")
    allowed = set(shared) | T225_OVERRIDES

    data = bytearray(args.vmlinux.read_bytes())
    offsets = defaultdict(list)
    for name, offset in table(args.vmlinux, data):
        offsets[name].append(offset)
    module_names = {module for choices in providers.values() for module in choices}
    module_exports = {name: read_module_exports(args.modules / name) for name in sorted(module_names)}

    errors = []
    changes = []
    kernel_count = module_count = 0
    for name, target in expected.items():
        if name in providers:
            matches = [module for module in providers[name] if module_exports[module].get(name) == target]
            if name in offsets:
                errors.append(f"{name}: duplicate kernel and module providers")
            elif not matches:
                errors.append(f"{name}: missing peer module export with CRC {target:#010x}")
            module_count += 1
            continue
        if not offsets[name]:
            errors.append(f"{name}: missing kernel export")
            continue
        kernel_count += 1
        for offset in offsets[name]:
            current, = struct.unpack_from("<I", data, offset)
            if current == target:
                continue
            if not args.write or name not in allowed or name == USERCOPY:
                errors.append(f"{name}: kernel CRC {current:#010x}, expected {target:#010x}")
            else:
                changes.append((name, offset, current, target))

    if errors:
        raise ValueError("\n".join(errors))
    if len(providers) != 42 or kernel_count != 481 or module_count != 42:
        raise ValueError("unexpected distribution of kernel and peer-module exports")
    if args.write:
        if args.write.resolve() == args.vmlinux.resolve():
            raise ValueError("write to a separate vmlinux file")
        for name, offset, current, target in changes:
            print(f"{name}: {current:#010x} -> {target:#010x}")
            struct.pack_into("<I", data, offset, target)
        args.write.write_bytes(data)

    print(f"PASS: {kernel_count} kernel exports and {module_count} peer-module exports "
          f"({len(changes)} CRC entries patched), native {USERCOPY} CRC {expected[USERCOPY]:#010x}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        sys.exit(f"FAIL: {error}")
