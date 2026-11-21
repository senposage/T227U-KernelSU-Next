#!/usr/bin/env python3
"""Train a raw LZ4 dictionary from separate, locally captured 4 KiB page files."""

import argparse
import hashlib
import os
import random
import shutil
import subprocess
import tempfile
from pathlib import Path

PAGE = 4096


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
    parser.add_argument("captures", type=Path, nargs="+",
                        help="raw page files from separate workload sessions")
    parser.add_argument("-o", "--output", type=Path, required=True)
    parser.add_argument("--size", type=int, default=16384,
                        help="maximum dictionary size in bytes (default: 16384)")
    parser.add_argument("--pages-per-capture", type=int, default=512)
    parser.add_argument("--seed", type=int, default=22714)
    args = parser.parse_args()
    if not 256 <= args.size <= 65536:
        parser.error("--size must be between 256 and 65536")
    if args.pages_per_capture < 1:
        parser.error("--pages-per-capture must be positive")
    if args.output.exists():
        parser.error("output already exists")
    zstd = shutil.which("zstd")
    if not zstd:
        parser.error("zstd CLI is required")

    rng = random.Random(args.seed)
    count = 0
    emitted = set()
    with tempfile.TemporaryDirectory(prefix="lz4-dict-training-") as tmp:
        workspace = Path(tmp)
        training = workspace / "pages"
        training.mkdir()
        for capture in args.captures:
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
