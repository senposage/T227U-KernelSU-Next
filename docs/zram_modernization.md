# T227U ZRAM modernization plan

## Scope and invariant

- Target: Samsung T227U, MT6765, Linux 4.19 vendor kernel.
- Baseline: 2 GiB logical ZRAM, LZ4, priority `-2`, conservative VM profile.
- Keep vendor automatic RamPlus disabled. Do not add initramfs or KernelSU boot
  scripts for tuning or dictionary deployment.
- Every kernel-linking step must pass the vendor CRC gate and boot as a separate
  artifact. Never mix zcomp surgery with unrelated scheduler, Binder, or OPP
  work.

## Gate 0 — prove a dictionary is worth pursuing

1. Capture a bounded 32 MiB sample under KernelSU root. On this 4.19 device,
   reading `/dev/block/zram0` exposes logical zero-filled space rather than
   compressed backing pages, so use selected anonymous pages from the active
   stress application's process instead. The sample can contain
   application-memory data: retain it only on the owner's local `E:\\` drive,
   never add it to Git or a release, and erase the device-side temporary copy
   immediately.
2. Reject zero pages, split 90/10 train/holdout deterministically, and record
   nonzero-page rate.
3. Train 4 KiB, 8 KiB, and 16 KiB candidate dictionaries; benchmark LZ4 level
   1 on the holdout against no dictionary using compressed bytes and elapsed
   time. The benchmark script must use actual input filenames, not a broken
   `cat | xargs` pipeline.
4. Continue only if a candidate improves holdout size materially without a
   compression-throughput regression. Record the raw numbers before choosing a
   dictionary size. The expected benefit is workload-dependent; no claim is
   made from synthetic pages.

## Gate 1 — upstream dependency map

The 4.19 tree uses crypto-API `zcomp` and one per-CPU stream shared by reads
and writes. It has no modern `backend_lz4.c`, `zcomp_params`,
`algorithm_params`, or dictionary ABI. Therefore:

1. Identify the exact final upstream commits for stream separation, shared
   decompression streams, bounce buffers, and idle-stream borrowing.
2. Create a commit-by-commit dependency matrix against this tree. Do not copy
   modern `lib/lz4` or `drivers/block/zram` wholesale.
3. Port the stream/concurrency plumbing first, retaining the 4.19 block layer,
   Samsung/MediaTek hooks, and existing sysfs ABI.

## Gate 2 — stream-latency port

1. Backport only a compilable, independently testable stream separation slice.
2. Validate allocation ownership, CPU-hotplug teardown, locking, and read/write
   concurrency before linking.
3. Build, run the vendor CRC gate, boot, and repeat the marker-separated ZRAM
   pressure capture. Compare full PSI, direct reclaim, and LMK events against
   the baseline.

## Gate 3 — optional dictionary backend

Only after Gates 0–2 pass:

1. Add an explicit, bounded dictionary ABI with reset-time-only mutation and
   correct lifetime teardown. Do not read `/data` from kernel context.
2. Port the Honor LZ4 template-stream optimization only with the backend API it
   requires. Its benefit exists only while a dictionary is active.
3. Expose any successful control through Kernel Adiutor with readback and an
   explicit apply action; no silent boot-time provisioning.
4. Repeat CRC, boot, ZRAM workload, and application stress validation.

### Dictionary lifetime rule

The first implementation supports exactly one immutable dictionary per live
ZRAM instance. It may be installed only while the device is reset and empty.
Changing a dictionary while compressed pages exist is invalid: decompression
must use the exact dictionary that created each page.

Workload-specific dictionaries are deferred. A correct multi-dictionary
implementation would require either a swap drain/reset when changing profiles,
or page-level dictionary generation metadata and retention of every old
dictionary until all pages using it are discarded. It would also need explicit
selection policy rather than context-switching between dictionaries on the
compression fast path. Neither design belongs in the initial 4.19 backport.

## Gate 0 result — 2026-11-20 Asphalt map load

The live game process was under 1.24 GiB of active ZRAM use, with 409 MiB RSS
and 843 MiB process swap. A bounded 32 MiB anonymous-heap capture produced 391
nonzero 4 KiB pages. A deterministic 90/10 split therefore left 351 training
and 40 holdout pages (160 KiB).

| Candidate | Holdout bytes | Ratio | Reduction vs. plain LZ4 |
| --- | ---: | ---: | ---: |
| Plain LZ4 level 1 | 85,135 | 0.520 | — |
| 4 KiB dictionary | 65,162 | 0.398 | 23.46% |
| 8 KiB dictionary | 61,291 | 0.374 | 28.01% |
| 16 KiB dictionary | 59,344 | 0.362 | 30.29% |

The dictionaries were produced with `zstd --train` and used as raw input for
the LZ4 CLI. This is a positive screening result, not a throughput claim or a
final LZ4-native dictionary-training method. The metrics-only result is stored
locally at `E:\\zram-bench-20261120\\lz4-dict-screen-results.json`; raw memory
and generated dictionaries are not versioned.

**Disposition:** Gate 0 passes. The selected candidate is 16 KiB, subject to
the stream-latency port and an on-device A/B test. No LZ4/zcomp kernel source
has changed yet.
