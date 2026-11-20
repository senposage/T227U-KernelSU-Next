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
| Binder | `eeef72d8cae5` | Synchronizes access to a transaction target process while freeing the transaction. |
| F2FS | `75b7b736d4b5` | Validates a malformed xattr entry before looking up its handler. `/data` is F2FS on T227U. |
| CPUFreq | `4f590db06867` | Holds the CPU read lock across governor suspend to avoid a hotplug/suspend race. |
| MM/DMA | `c9cd4e15d9a5`, `f396b137cd9d`, `4182c78187b2` | Fatal-signal compaction exit, DMAengine allocation leak fix, and architecture page copy for zsmalloc migration. |
| ZRAM sysfs | `8c6ce8aef6b4` | Replaces unsafe fixed-buffer command parsing for `idle` and `writeback` with `sysfs_streq()`. |
| F2FS completion and corruption | `d6d8cd14b84c`, `f3e38c536b2a`, `ca3d63252472` | Keeps superblock state live through writeback completion and turns corrupt-node/inode cases into an fsck-required error instead of an out-of-bounds access or kernel panic. |
| LZ4 | `f75893e27f63` | Stops partial decompression before reading a match offset beyond corrupt compressed input. |

Each affected object compiled with the preserved T227U r7 configuration in a
separate audit output directory. These commits are not yet in `main` and have
not produced a release.

## Explicitly rejected for T227U/GSI

- In-tree Gen4M WLAN and AXI patches (`f95aeae0c24d`, `3b4c912ef106`): T227U
  boots Samsung's immutable Gen4M vendor modules. Replacing that radio stack
  would require matching WMT/conninfra, firmware, transport, HAL, and vendor
  integration. Revisit only for a full ROM/vendor project.
- MT6768 OPP mapping (`c3fb3ee65509`) and its uPower coupling: hardwires a
  different SoC topology into scheduler core and conflicts with T227U's custom
  PPM/user-policy work.
- MT6768/Mali GPU, Xiaomi device-tree, camera, display, audio, touch,
  fingerprint, charging, and vendor connectivity changes: hardware-specific.
- The donor's full modern CPUFreq QoS/policy series: too invasive for the MTK
  4.19 driver and current PPM guard. Individual race fixes are reviewed
  separately.
- F2FS discard-thread UAF (`0a72eeb86ef5`): not applicable. This T227U F2FS
  fork lacks the discard-thread call that creates the vulnerable lifetime.

## Deferred pending a dedicated test plan

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

## Release rule

No audit-branch change may be released until it has a full T227U build, the
vendor CRC gate passes and is applied, and on-device boot/radio/governor/storage
smoke tests are complete.
