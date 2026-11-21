#!/usr/bin/env python3
"""Train raw LZ4 dictionaries from page files or explicitly selected running apps."""

import argparse
import base64
import hashlib
import os
import random
import re
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

PAGE = 4096
WINDOW_PAGES = 256
PACKAGE = re.compile(r"[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+")


def adb_root(adb, serial, script, timeout=60):
    command = [adb]
    if serial:
        command.extend(("-s", serial))
    encoded = base64.b64encode(script.encode("ascii")).decode("ascii")
    command.extend(("exec-out", f"echo {encoded} | base64 -d | su -c sh"))
    result = subprocess.run(command, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=timeout)
    if result.returncode:
        error = result.stderr.decode(errors="replace").strip()
        raise ValueError(error or "ADB capture failed; check the device and root grant")
    return result.stdout


def capture_windows(maps, page_budget):
    """Sample windows spread across Java, native allocator, and unnamed memory."""
    groups = {"java": [], "native": [], "anonymous": []}
    for line in maps.splitlines():
        fields = line.split(maxsplit=5)
        if len(fields) < 5 or fields[1] != "rw-p" or fields[4] != "0":
            continue
        label = fields[5] if len(fields) == 6 else ""
        if label.startswith(("[anon:dalvik-main", "[anon:dalvik-non moving",
                             "[anon:dalvik-large", "[anon:dalvik-free list")):
            group = "java"
        elif label == "[heap]" or label.startswith(("[anon:libc_malloc",
                                                    "[anon:scudo",
                                                    "[anon:partition_alloc")):
            group = "native"
        elif not label:
            group = "anonymous"
        else:
            continue
        lo, hi = (int(value, 16) // PAGE for value in fields[0].split("-"))
        if lo < hi:
            groups[group].append((lo, hi))

    present = [ranges for ranges in groups.values() if ranges]
    windows = set()
    if not present:
        return windows
    for group_index, ranges in enumerate(present):
        budget = page_budget // len(present) + (group_index < page_budget % len(present))
        total = sum(hi - lo for lo, hi in ranges)
        budget = min(budget, total)
        chunks = (budget + WINDOW_PAGES - 1) // WINDOW_PAGES
        for index in range(chunks):
            target = total * (2 * index + 1) // (2 * chunks)
            requested = min(WINDOW_PAGES, budget - index * WINDOW_PAGES)
            for lo, hi in ranges:
                span = hi - lo
                if target < span:
                    count = min(requested, span)
                    start = min(max(lo, lo + target - count // 2), hi - count)
                    windows.add((start, count))
                    break
                target -= span
    return windows


def capture_app(adb, serial, package, mib, output):
    if not PACKAGE.fullmatch(package):
        raise ValueError(f"invalid Android package: {package}")
    # Package names have been validated and no process-memory bytes enter the shell text.
    identity = (f"pid=$(pidof -s '{package}') || exit 2\n"
                '[ -n "$pid" ] || exit 2\n')
    mapping = adb_root(adb, serial, identity
                       + 'printf "%s\\n" "$pid"\ncat "/proc/$pid/maps"\n')
    pid, separ, maps = mapping.partition(b"\n")
    if not separ or not pid.isdigit():
        raise ValueError(f"{package}: no running process or root-readable mappings")
    budget = mib * 1024 * 1024 // PAGE
    windows = capture_windows(maps.decode(errors="replace"), budget)
    if not windows:
        raise ValueError(f"{package}: no eligible writable anonymous memory")
    commands = [identity, f'[ "$pid" = {int(pid)} ] || exit 3\n']
    for start, count in sorted(windows):
        commands.append(f'dd if="/proc/$pid/mem" bs={PAGE} skip={start} '
                        f'count={count} 2>/dev/null || true\n')
    data = adb_root(adb, serial, "".join(commands))
    if not data or len(data) % PAGE or len(data) > budget * PAGE:
        raise ValueError(f"{package}: incomplete or invalid page capture")
    write_private(output, data)
    print(f"{package}: captured {len(data) // PAGE} pages")


def write_private(path, data):
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "wb") as output:
        output.write(data)


def sample_pages(path, limit, rng):
    """Sample across the capture without overweighting duplicate or same-fill pages."""
    picked = []
    seen = set()
    with path.open("rb") as source:
        while True:
            page = source.read(PAGE)
            if not page:
                break
            if len(page) != PAGE:
                raise ValueError(f"{path.name}: capture is not a multiple of {PAGE} bytes")
            if page == page[:8] * (PAGE // 8):
                continue
            digest = hashlib.blake2s(page, digest_size=16).digest()
            if digest in seen:
                continue
            seen.add(digest)
            if len(picked) < limit:
                picked.append(page)
            else:
                index = rng.randrange(len(seen))
                if index < limit:
                    picked[index] = page
    return picked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", type=Path, nargs="*",
                        help="raw page files from separate workload sessions")
    parser.add_argument("--device-package", action="append", default=[], metavar="PACKAGE",
                        help="sample a running app over rooted ADB (repeat to select more apps)")
    parser.add_argument("--serial", help="ADB device serial, needed with multiple devices")
    parser.add_argument("--adb", default="adb", help="ADB executable (default: adb)")
    parser.add_argument("--capture-mib", type=int, default=32,
                        help="maximum captured MiB per selected app (default: 32)")
    parser.add_argument("--repeat", type=int, default=1,
                        help="number of capture rounds (default: 1)")
    parser.add_argument("--interval", type=int, default=60,
                        help="seconds between capture rounds (default: 60)")
    parser.add_argument("--save-captures", type=Path, metavar="DIRECTORY",
                        help="also keep private raw captures in a new directory")
    parser.add_argument("--capture-only", action="store_true",
                        help="save raw captures without training")
    parser.add_argument("-o", "--output", type=Path)
    parser.add_argument("--size", type=int, default=16384,
                        help="maximum dictionary size in bytes (default: 16384)")
    parser.add_argument("--pages-per-capture", type=int, default=512)
    parser.add_argument("--seed", type=int, default=22714)
    args = parser.parse_args()
    if not args.captures and not args.device_package:
        parser.error("provide a capture file or --device-package")
    if len(set(args.device_package)) != len(args.device_package):
        parser.error("select each running app only once")
    if any(not PACKAGE.fullmatch(package) for package in args.device_package):
        parser.error("invalid Android package name")
    if not 1 <= args.capture_mib <= 64:
        parser.error("--capture-mib must be between 1 and 64")
    if not 1 <= args.repeat <= 24 or not 1 <= args.interval <= 3600:
        parser.error("--repeat must be 1–24 and --interval must be 1–3600 seconds")
    if args.repeat > 1 and not args.device_package:
        parser.error("--repeat requires --device-package")
    if len(args.device_package) * args.repeat * args.capture_mib > 512:
        parser.error("device captures are limited to 512 MiB per invocation")
    if args.capture_only and (not args.device_package or not args.save_captures or args.output):
        parser.error("--capture-only requires --device-package and --save-captures, without -o")
    if not args.capture_only and not args.output:
        parser.error("-o is required when training a dictionary")
    if args.save_captures and (not args.device_package or args.save_captures.exists()):
        parser.error("--save-captures requires a new directory and --device-package")
    if not 256 <= args.size <= 65536:
        parser.error("--size must be between 256 and 65536")
    if args.pages_per_capture < 1:
        parser.error("--pages-per-capture must be positive")
    if args.output and args.output.exists():
        parser.error("output already exists")
    zstd = shutil.which("zstd")
    if not args.capture_only and not zstd:
        parser.error("zstd CLI is required")
    if args.device_package and not shutil.which(args.adb):
        parser.error("ADB is required for device capture")

    rng = random.Random(args.seed)
    count = 0
    emitted = set()
    with tempfile.TemporaryDirectory(prefix="lz4-dict-training-") as tmp:
        workspace = Path(tmp)
        captures = list(args.captures)
        raw_dir = workspace
        if args.save_captures:
            args.save_captures.mkdir(mode=0o700)
            raw_dir = args.save_captures
        for round_number in range(1, args.repeat + 1):
            for package in args.device_package:
                raw = raw_dir / f"{package}.{round_number:03d}.bin"
                try:
                    capture_app(args.adb, args.serial, package, args.capture_mib, raw)
                except (ValueError, OSError, subprocess.TimeoutExpired) as error:
                    parser.error(f"{package}: {error}")
                captures.append(raw)
            if round_number < args.repeat:
                print(f"Next capture in {args.interval} seconds", flush=True)
                time.sleep(args.interval)
        if args.capture_only:
            return
        training = workspace / "pages"
        training.mkdir()
        for capture in captures:
            start = count
            for page in sample_pages(capture, args.pages_per_capture, rng):
                digest = hashlib.blake2s(page, digest_size=16).digest()
                if digest in emitted:
                    continue
                emitted.add(digest)
                (training / f"{count:07}.page").write_bytes(page)
                count += 1
            print(f"{capture.name}: {count - start} training pages")
        if count < 128:
            parser.error("fewer than 128 usable pages; add a capture")
        candidate = workspace / "dictionary"
        subprocess.run([zstd, "--train", "-T1", "-r", f"--maxdict={args.size}",
                        "-o", str(candidate), str(training)], check=True)
        fd = os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "wb") as output, candidate.open("rb") as source:
            shutil.copyfileobj(source, output)
    print(f"Saved {args.output.stat().st_size} bytes from {count} pages")


if __name__ == "__main__":
    main()
