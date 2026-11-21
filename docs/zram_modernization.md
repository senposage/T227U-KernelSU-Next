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
4. Use holdout size to screen candidates. Measure throughput and page-in
   latency separately during repeatable benchmarking before adopting a
   dictionary. Keep results for independent workloads before choosing a
   general-purpose dictionary.

## Gate 1 — upstream dependency map

The starting 4.19 tree used Crypto API `zcomp` and one per-CPU stream shared
by reads and writes. The T227U port now has a direct LZ4 backend with
backend-owned contexts. Device-wide dictionary parameters and a dictionary
upload interface are still pending.

1. Identify the exact final upstream commits for stream separation, shared
   decompression streams, bounce buffers, and idle-stream borrowing.
2. Create a commit-by-commit dependency matrix against this tree. Do not copy
   modern `lib/lz4` or `drivers/block/zram` wholesale.
3. Preserve the 4.19 block layer, Samsung/MediaTek hooks and existing sysfs
   ABI while modernizing the plain LZ4 backend. Audit stream separation
   against the vendor locking before adopting it.

## Gate 2 — direct LZ4 and stream latency

1. Test the direct LZ4 backend with the existing non-sleeping stream
   ownership. It invokes the same codec the Crypto API used on this tree.
   Compare against the previous kernel under the same workload to measure the
   cost of the wrapper and workspace handling.
2. Treat sleepable streams as a separate dependency. The standard read path
   holds a slot bit spinlock and a zsmalloc mapping. The vendor writeback
   read path holds an atomic source mapping. A stream implementation that
   sleeps requires changes to those lifetimes before adopting it.
3. Run the vendor CRC gate, boot, and repeat the marker-separated ZRAM
   pressure capture after each change. Compare full PSI, direct reclaim,
   and LMK events against the baseline.

## Gate 3 — optional dictionary backend

Only after Gates 0–2 pass:

1. Add an explicit, bounded dictionary ABI with reset-time-only mutation and
   correct lifetime teardown. Do not read `/data` from kernel context.
2. Port the Honor LZ4 template-stream optimization only with the backend API it
   requires. Its benefit exists only while a dictionary is active.
3. Expose any successful control through Kernel Adiutor with readback and an
   explicit apply action; no silent boot-time provisioning.
4. Repeat CRC, boot, ZRAM workload, and application stress validation.

### Gathered dictionary dependencies

The upstream comparison is pinned to Linux commit
[`5f22fb20497f`](https://github.com/torvalds/linux/commit/5f22fb20497f4983094131f00a734362300d22d2).
The reference files and patches are stored locally under
`/home/legit-su/zram-upstream-audit/5f22fb20497f4983094131f00a734362300d22d2`.
None of the dictionary patches are included in the plain-LZ4 test build.

| Upstream change | 4.19 adaptation |
| --- | --- |
| [`f2bac7ad187d`](https://github.com/torvalds/linux/commit/f2bac7ad187d77e2a053d3cd04b158a06b683d26), [`b8f03cb703a16`](https://github.com/torvalds/linux/commit/b8f03cb703a160e14f87a467a4443adc5d940087) | Keep dictionary bytes and the prepared stream template in device-wide immutable parameters. Allocate before CPU stream creation and free after the streams and I/O have drained. |
| [`52c7b4e2ba50`](https://github.com/torvalds/linux/commit/52c7b4e2ba508a924c991e681db534e66a851adf), [`6a81bdfeb350`](https://github.com/torvalds/linux/commit/6a81bdfeb35094c3097650306a5fda9a990d8a97) | The callback port already owns an opaque context per CPU. Pass immutable parameters through that context or the request callbacks when adding dictionary support. |
| [`fb4f644ee8da`](https://github.com/torvalds/linux/commit/fb4f644ee8dac4e896269b84f041eb5dd3f6249d), [`f0f6f7871430`](https://github.com/torvalds/linux/commit/f0f6f787143068b23c5808e7a63aef03601f1377) | Prepare a template using `LZ4_loadDict()`, copy it to the private stream per page and call `LZ4_compress_fast_continue()`. Evaluate `LZ4_decompress_safe_usingDict()` for stateless reads, including vendor compressed writeback. The 4.19 codec already exposes these APIs. |
| [`7b0f677c7bd5`](https://github.com/torvalds/linux/commit/7b0f677c7bd539bd5695b14f9a195e257c3b4463) | Validate the selected backend and dictionary size before enabling the disk. Protect upload, initialization and reset with the existing initialization lock. |

Upstream's `algorithm_params` reads a dictionary by filename inside the
kernel. This port needs a bounded userspace upload. In this 4.19 tree,
ordinary kernfs writes are limited to one page per callback. A multi-page
upload therefore needs explicit staging and commit, including validation
for incomplete or conflicting writes. It cannot be a single text sysfs
store for a 16 KiB dictionary.

Before accepting a dictionary for a build, validate independent captures,
check block round trips and reset lifetime, and measure the template-copy
cost against rebuilding the hash table on the tablet. Keep workload memory
and resulting dictionaries local.

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

The 16 KiB candidate is worth further testing. The 40-page holdout comes
from the same game capture as the training pages, so it measures an in-run
compression benefit. A separate capture is needed to test generalization,
and the CLI timings include a process launch per page.

The direct LZ4 test build is recorded in `BUILD_LEDGER.md` under
`B-20261120-LZ4`. It uses the existing in-tree codec and an explicit
per-CPU LZ4 workspace. The modular follow-up `B-20261120-LZ4M` passed
boot and live page-in. In the next image `B-20261120-LZ4-READ`, plain LZ4
decompresses without any mutable compressor context. Compressed vendor
writeback objects spanning pages still need a separate per-CPU bounce
page. Crypto API backends continue borrowing a per-CPU transform for
decoding. Dictionary selection and sleepable shared streams remain
unimplemented.
