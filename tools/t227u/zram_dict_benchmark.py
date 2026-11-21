#!/usr/bin/env python3
"""Screen LZ4 dictionaries against a bounded logical ZRAM-page sample.

The input is private device-memory material.  Keep the output directory local,
never add it to Git, and remove it when the experiment is finished.
"""
import argparse
import json
import random
import shutil
import subprocess
import time
from pathlib import Path

PAGE = 4096
DICT_SIZES = (4096, 8192, 16384)


def run(command):
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.PIPE, text=True)


def compress_pages(lz4, pages, output, dictionary=None):
    output.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    total = 0
    for page in pages:
        packed = output / (page.name + ".lz4")
        command = [lz4, "-1", "--no-frame-crc", "-q", "-f"]
        if dictionary:
            command += ["-D", str(dictionary)]
        command += [str(page), str(packed)]
        run(command)
        total += packed.stat().st_size
    return total, time.perf_counter() - started


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("sample", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--seed", type=int, default=22714)
    args = parser.parse_args()

    lz4 = shutil.which("lz4")
    zstd = shutil.which("zstd")
    if not lz4 or not zstd:
        raise SystemExit("lz4 and zstd CLIs are required")
    if args.output.exists():
        raise SystemExit("output directory already exists: " + str(args.output))

    raw = args.sample.read_bytes()
    pages = [raw[offset:offset + PAGE] for offset in range(0, len(raw), PAGE)
             if len(raw[offset:offset + PAGE]) == PAGE and any(raw[offset:offset + PAGE])]
    if len(pages) < 128:
        raise SystemExit("fewer than 128 nonzero pages; capture a better workload sample")

    random.Random(args.seed).shuffle(pages)
    split = int(len(pages) * 0.9)
    train, test = pages[:split], pages[split:]
    train_dir, test_dir = args.output / "train", args.output / "test"
    for directory, source in ((train_dir, train), (test_dir, test)):
        directory.mkdir(parents=True)
        for number, page in enumerate(source):
            (directory / ("page-%05d.bin" % number)).write_bytes(page)

    test_pages = sorted(test_dir.iterdir())
    original = len(test_pages) * PAGE
    results = {"sample_bytes": len(raw), "nonzero_pages": len(pages),
               "train_pages": len(train), "test_pages": len(test),
               "test_input_bytes": original,
               "dictionary_trainer": "zstd --train (raw dictionary used by lz4)",
               "cases": {}}

    baseline, elapsed = compress_pages(lz4, test_pages, args.output / "packed-baseline")
    results["cases"]["baseline"] = {"bytes": baseline, "elapsed_seconds": elapsed,
                                    "ratio": baseline / original}
    for size in DICT_SIZES:
        dictionary = args.output / ("lz4-%dk.dict" % (size // 1024))
        run([zstd, "--train", "--maxdict=%d" % size, "-q", "-f",
             *[str(page) for page in sorted(train_dir.iterdir())],
             "-o", str(dictionary)])
        packed, elapsed = compress_pages(lz4, test_pages,
                                         args.output / ("packed-%dk" % (size // 1024)),
                                         dictionary)
        results["cases"]["dict_%dk" % (size // 1024)] = {
            "bytes": packed, "elapsed_seconds": elapsed, "ratio": packed / original,
            "size_reduction_percent": (baseline - packed) * 100.0 / baseline,
        }

    (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results, indent=2))
    print("Note: elapsed time includes CLI process startup per page; use it only as a\n"
          "relative screening signal. A zstd-trained raw dictionary is a heuristic\n"
          "screen for LZ4; kernel throughput still requires on-device measurement.")


if __name__ == "__main__":
    main()
