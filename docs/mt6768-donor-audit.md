# MT6768 donor audit

Donor: `mt6768-S/android_kernel_xiaomi_mt6768`, branch `lineage-24.0`.

This is a donor review, not a merge. The donor graph contains 835,520
reachable commits, including complete Linux/Android ancestry. Review therefore
targets the donor's recent overlay/backport window and inspects each portable
candidate before it is staged on `codex/mt6768-donor-audit`.

## Staged and object-compiled

| Area | Donor commits | Reason |
| --- | --- | --- |
| HID | `0c05d983e57a`, `4f9f722a64fe` | Adds a missing ratelimited helper and rejects empty numbered HID reports before the report-ID read. |
| Schedutil | `0ab005ccb85a` | Allows a queued deferred update to use the newest demand sample. Does not alter PPM, governor limits, or OC ceilings. |
| F2FS xattrs | `75b7b736d4b5`, `f9dfa44be0fb` | Validates malformed xattr entries both before handler lookup and while finding the last entry, preventing corrupt `/data` metadata from becoming an out-of-bounds access. |
| CPUFreq | `4f590db06867` | Holds the CPU read lock across governor suspend to avoid a hotplug/suspend race. |
| MM/DMA | `c9cd4e15d9a5`, `f396b137cd9d`, `4182c78187b2` | Fatal-signal compaction exit, DMAengine allocation leak fix, and architecture page copy for zsmalloc migration. |
| Zsmalloc migration | `645996efc2ae`, `b19675b6a046` | Synchronizes zspage locking with asynchronous free/page migration and closes its missed-wakeup race during pool teardown. |
| ZRAM sysfs | `8c6ce8aef6b4` | Replaces unsafe fixed-buffer command parsing for `idle` and `writeback` with `sysfs_streq()`. |
| ZRAM backing device | `a12a03051ef9` | Rejects a zero-sized, including self-referential, writeback backing device before setup. |
| ZRAM state reader | `26df52b3e816` | Stops the debug state reader before `snprintf()` can consume the final byte of its userspace buffer. |
| ZRAM compaction stats | `638c954653d4` | Reads the already-atomic total through `atomic_long_read()`, keeping the exported compaction count correct under concurrent reclaim and manual compaction. |
| F2FS completion and corruption | `d6d8cd14b84c`, `f3e38c536b2a`, `ca3d63252472` | Keeps superblock state live through writeback completion and turns corrupt-node/inode cases into an fsck-required error instead of an out-of-bounds access or kernel panic. |
| F2FS inode lifetime | `e408ed9f36b0` | Removes an evicted inode from dirty-meta tracking before checkpoint code can dereference it. |
| F2FS GC and truncation | `45c9da086dde`, `6735aa9e3303`, `37f6006362a2` | Rejects special-file inodes during GC, initializes the physical-block extent-map field, and releases a node page on truncation failure. |
| F2FS device path | `8d6d68ddee44` | Reserves a path terminator after copying the maximum on-disk device-path length. |
| LZ4 | `f75893e27f63` | Stops partial decompression before reading a match offset beyond corrupt compressed input. |
| MTK MSDC | `1fc9b7e48231`, `ab75a549f9fa` | Makes DMA preparation data-scoped and refuses a request when DMA mapping fails, preventing commands from reaching an invalid DMA mapping. |
| Scheduler logging | Local correction | Demotes the expected pre-domain EAS initialization retry from `pr_info` and avoids emitting the paired initializer banner before a domain exists. |

Each affected object compiled with the preserved T227U r7 configuration in a
separate audit output directory. These commits are not yet in `main` and have
not produced a release.

## Re-reviewed applicability

- Gen4M WLAN (`f95aeae0c24d`, `3b4c912ef106`): each is a reasonable small
  driver fix in isolation, but this source tree deliberately leaves
  `vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m` external and
  builds the matching Samsung vendor module. The files the commits modify are
  absent here. Porting them requires the matching vendor WLAN source/module
  package and a radio regression suite; this is a source-availability and ABI
  boundary, not a generic “MediaTek” rejection.
- MT6768 OPP mapping (`c3fb3ee65509`): the earlier hardware-specific
  classification was incorrect. The donor reads active uPower tables at
  runtime; it does not hard-code MT6768 frequencies. It is not ready to port
  unchanged, however: it snapshots stock capacities once, while this tree has
  runtime-selectable MT6765 OC bins. A T227U port must derive capacities from
  the live CPUFreq/OC table and refresh them when the exposed bin set changes.
  This is a concrete local prerequisite, not a rejection.
- MT6768/Mali GPU, Xiaomi device-tree, camera, display, audio, touch,
  fingerprint, charging, and vendor connectivity changes: hardware-specific.
- The donor's full modern CPUFreq QoS/policy series: it cannot be taken as one
  atomic import into the MTK 4.19/PPM path, but each correctness fix and each
  policy change is being reviewed as a port candidate. A dependency or a
  benchmark requirement is not itself a rejection.
- F2FS discard-thread UAF (`0a72eeb86ef5`): prior applicability assessment was
  incorrect. The T227U fork has the discard worker; the corrected teardown
  ordering is now carried on `codex/t227u-f2fs-bringup`.

## Deferred pending a dedicated test plan

- **P1 — Binder backport validation:** `eeef72d8cae5`, `229670361c29`, and
  `544b1575a0e1` are temporarily reverted for an A/B test after Asphalt
  showed repeated `FAILED BINDER TRANSACTION` messages followed by a reboot.
  The reader-availability change alters Binder delivery eligibility and is the
  primary candidate. Do not restore any of the three without a captured
  Binder-driver failure signature and a targeted test plan.
- **P2 — FPSGO/XGF vendor-module parity:** `fpsgo.ko` fails to load because
  the source-built kernel does not export its required XGF interfaces. The
  baseline CPUFreq governor and PPM paths remain functional, but frame-aware
  CPU/GPU boost and frame pacing are absent. Restore the matching in-kernel
  FPSGO/XGF implementation (or a compatible replacement), then verify module
  load, `/sys/kernel/fpsgo`, game frame pacing, and the normal CPU/GPU thermal
  limits. This is the first compatibility item for the next round.
- LZO/LZO-RLE safety series: relevant because T227U enables ZRAM and LZO, but
  it is an ordered multi-commit compressor rewrite and must be imported and
  stress-tested as a complete series.
- ZRAM writeback race and CMA-allocation changes: the donor implementation
  differs substantially. The CMA change could starve MediaTek multimedia
  allocations; do not import by default.
- CMA, page allocator, memcg reclaim, Binder freezer, BPF, RCU, and broad
  upstream security series: evaluate as dependency-aware batches rather than
  cherry-picking isolated commits from a newer kernel.
- Policy/tuning changes such as linear scheduler scaling and vmstat cadence:
  behavior changes, not correctness fixes; require benchmarks before adoption.

## Performance candidates (not staged)

- F2FS checkpoint reduction (`72c6b13f468e`): may reduce metadata/fsync
  overhead after directory-xattr updates. It changes checkpoint triggering and
  must pass power-loss and filesystem stress testing before it is considered.
- F2FS linear dentry search (`df304b987283`): a Unicode casefold correctness
  fallback, not a general directory-lookup optimization; it can increase work
  on the fallback path.
- CPUFreq QoS/policy conversion and scheduler scaling: potentially useful
  latency-policy work, but incompatible as isolated imports with this MT6765
  CPUFreq/PPM implementation. Evaluate only as a dedicated, benchmarked port.

## Release rule

No audit-branch change may be released until it has a full T227U build, the
vendor CRC gate passes and is applied, and on-device boot/radio/governor/storage
smoke tests are complete.
