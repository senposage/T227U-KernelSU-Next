# MT6768 donor audit

Donor: `mt6768-S/android_kernel_xiaomi_mt6768`, branch `lineage-24.0`,
pinned at `31c653fb625385710583e329cdbf56e56f17a4ae`.

The donor graph includes complete Linux/Android ancestry. The audit
targets its recent overlay and backports. On 2026-11-21, the first 100
commits were screened by subject, the next 300 were queried for matching
subsystems, and the candidates below were checked against their diffs and
the T227U call paths. The working branch is `codex/t227u-f2fs-bringup`.

## Carried in the T227U r8 source

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

The r8 prerelease was built from `e952c8902`. These earlier fixes were
object-compiled during the audit and carried forward into that source.

## 2026-11-21 candidate batch

| Area | Donor commits | Disposition and verification |
| --- | --- | --- |
| CPUFreq sysfs | `0eceef464ddb`, `7a85d5bf3749` | `46aa41a65` initializes masks, locks, work, and release completion before publishing the policy kobject. A sysfs callback can take the policy lock, and an add failure can complete the initialized release wait. |
| CMA activation | `9f0f50701bd3` | `3725c88a1` rejects a reserved area with no initialized bitmap. An earlier activation failure can leave a later area in that state. |
| MMC card add failure | `1a82eb59ae1b` | `213d65daf` drops the OF node acquired before `device_add()` even when card registration fails. |
| Scatterlist allocation | `30d9ba69c13f` | `3a399c2b1` uses the slab allocator for every standard `sg_alloc_table()` chunk, paired with `kfree()`. Custom allocator callbacks keep their existing behavior. |
| Video buffer wakeup | `669d186355c8` | `eddb9b267` gives the scheduler a synchronous handoff hint on completion. Measure camera/video dequeue latency and drops on-device. |
| Alarm timer noise | `c5a7a1ec158e` | `5cfbd2725` removes a routine enqueue notice from the locked timer path while retaining alarm error logs. |

All six affected objects compiled in the established T227U output
directory with the r8 configuration unchanged. The public r8 artifact
remains based on `e952c8902`. This candidate batch still needs a full
link, the vendor CRC gate, and device testing before packaging.

## Re-reviewed applicability

- Gen4M WLAN (`f95aeae0c24d`, `3b4c912ef106`): each is a reasonable small
  driver fix in isolation, but this source tree deliberately leaves
  `vendor/mediatek/kernel_modules/connectivity/wlan/core/gen4m` external and
  builds the matching Samsung vendor module. The files the commits modify are
  absent here. Porting them requires the matching vendor WLAN source/module
  package and a radio regression suite; this is a source-availability and ABI
  boundary, not a generic “MediaTek” rejection.
- OPP mapping (`c3fb3ee65509`): the active T227U port `2fbbaed0f` maps
  utilization clamps through the CPUFreq table and accounts for exposed
  OC bins. The tested r8 config enables `CONFIG_UCLAMP_MAP_OPP`.
- Page allocator watermark accounting (`78cd7af83b38`): already carried.
  Both the slow path and the order-zero fast path subtract unavailable
  high-atomic and CMA reserves.
- MT6768/Mali GPU, Xiaomi device-tree, camera, display, audio, touch,
  fingerprint, charging, and vendor connectivity changes: hardware-specific.
- The donor's full modern CPUFreq QoS/policy series: it cannot be taken as one
  atomic import into the MTK 4.19/PPM path, but each correctness fix and each
  policy change is being reviewed as a port candidate. A dependency or a
  benchmark requirement is not itself a rejection.
- F2FS discard-thread UAF (`0a72eeb86ef5`): prior applicability assessment was
  incorrect. The T227U fork has the discard worker; the corrected teardown
  ordering is now carried on `codex/t227u-f2fs-bringup`.

## Follow-up and prerequisites

| Area | Donor commits or source | Next work |
| --- | --- | --- |
| Binder | `eeef72d8cae5`, `229670361c29`, `544b1575a0e1` | The temporary A/B revert ended with restoration in `a0c343bad` and the context-name lifetime correction in `ab5b59df0`. The game failure also occurred on the earlier kernel and cleared after resetting app data. Keep Binder tests separate from game data state. |
| FPSGO/XGF | `9c1e50771` | Build #25 boots with in-tree FPSGO v3 enabled. The stock module still needs eight implementation functions, including `xgf_ko_init`; the callback pointer globals and ready hook are already exported. Find the matching stock provider or port the XGF implementation against the current structures and callback lifecycle. Verify frame activity and XGF estimation under a workload. See the runtime log for the exact imports. |
| F2FS | `72c6b13f468e`, `df304b987283` | Checkpoint reduction and the encrypted-casefold fallback are included in r8. Validate encrypted directory lookup, fsync, and shutdown recovery on F2FS before claiming a performance gain. |
| DMA-BUF fences | `a726be2dfe3b` | Ported as `c3c875221`. A zeroed stack buffer serves single-fence detail queries and an array serves multiple fences. A host harness matched the baseline reply bytes, including all names used by Android and sync diagnostics, and verified removal of one heap allocation on the single-fence path. Device timing remains open. |
| ZRAM and CMA | `821e2b0937da`, `9cd286e1c961` | The writeback access-race fix is already in `3485fb54b`. Allowing ZRAM to use CMA needs per-area camera/video allocation and swap-pressure measurements. The donor uses the existing `__GFP_CMA` flag and strips it from unmovable zsmalloc metadata. An r8 tablet snapshot had 40 KiB of free CMA out of 400 MiB total. |
| CMA telemetry | `aea2e5ab344d`, `6d2a68e93757`, `923a261cb379` | Consider read-only counters before granting ZRAM access to CMA. Keep existing VM event numbers stable and isolate the bitmap-lock conversion from telemetry so its interrupt and latency consequences can be tested separately. |
| Proactive reclaim | `31c653fb6253` | The tablet mounts the memory controller at `/dev/memcg` with cgroup v1. The donor exposes this interface only through cgroup v2, so it has no current Android memory-controller path. A cgroup-v2 memory migration and an intended userspace caller would make it applicable. |
| LZO | `888466be8c6b`, `f65ee17a14b9`, `4a40ed11eb3d` | The bounded compressor is already wired into crypto, regcache, and F2FS by `9e4f655f9`. LZO-RLE still needs its ordered codec and crypto registration series with empty/short input, alignment, and legacy stream tests. Branch hints can be measured separately. Keep the tested LZ4 default for any codec comparison. |
| MMC diagnostics | `c6ff70f2b100` | The donor edits `mtk-sd`; T227U builds `CONFIG_MMC_MTK_PRO` and uses ComboA. On msdc0, normal commands, responses, and completions feed a 4,000-entry history ring. The existing `/proc/msdc_debug` zone updates `sd_debug_zone[]`; the ring writer does not consult it. A dedicated gate should preserve the register/host-state dump path and handle command/response pairs interrupted by a toggle. Storage latency gains remain unmeasured. |
| Power-supply uevents | `827999220f8e` | Replace per-property allocation only with bounded name handling that preserves the full uppercase uevent key. The donor's fixed buffer has no length check. Benchmark charging notification traffic before staging. |
| Scheduler and counters | `99fe5134769f`, `e99d90895fd4`, `0d1ad9ba8013` | Linear scaling and a two-second vmstat interval are policy changes. Measure wakeup latency and LMKD/reclaim behavior with the existing Samsung defaults as the control. The vmstat branch hints can be measured independently. |
| GPU wake hint | `dbc154a64d7d` | The donor targets a specific Mali Valhall driver path. Identify the matching completion producer in the active T227U graphics driver and confirm its sleep behavior before adapting the hint. |

## Swap and block follow-up

The donor's swap and block paths were checked against the T227U source
because these affect low-memory workloads and KA's explicit resize path.

| Priority | Donor commit | T227U disposition |
| --- | --- | --- |
| High, stability | `a55f268abdb7` | Ported as `3b8275910`. Swapoff removes the device from the available list with the swap-info lock held. The three removal paths were traced, and the shared helper asserts that lock. Full link and vendor CRC gate passed. KA resize testing on this build remains pending. |
| High, data integrity | `f86d55cf6161` | Ported as `d5a666c06`. Both swap-in paths retain the compressed slot until normal swap-cache release. Full link and vendor CRC verification passed. Carry it as a correctness fix and cover page-in and slot release in the usual tablet smoke test. |
| Low on r8 | `387217b97e99` | Adds `cond_resched()` after a failed swap-map scan. The shipped config uses full preemption, where `_cond_resched()` is a no-op. |
| Already present | `6ceea9a764b4`, `a6088845c2bf`, `ba989a01469d`, `fcf38cdf332a` | The swapfile offset fix and Kyber merge, requeue-token, and domain wait-queue work are already carried. |
| Outside the current eMMC path | `dbb7339cfddf` | mq-deadline restart handling in this patch applies to zoned block devices. The T227U eMMC path is non-zoned. |
| Compatibility check | `3a1fe75c4db9` | The filesystem-encrypted swapfile guard changes `swapon()` eligibility. Validate the intended disk-swap backing and encryption setup before applying it. |

## XGF provider search

- The original kernel tar contains the two FPSGO variants with XGF glue.
  The T225/T225N/T227 source overlay contains DTS and config files. The
  device-side module scan found `xgf_ko_init` only as an import in
  `/vendor/lib/modules/fpsgo.ko`.
- Extracted `boot.img.lz4` from the local Canadian
  `T227UVLSCFYE2` AP archive and checked its embedded configuration.
  It has FPSGO v3 disabled and has none of the eight XGF provider
  names in its kernel image. The matching Verizon `T227USQSCEYE2`
  AP was listed publicly but its boot image was unavailable locally.
  Samsung's source search returned HTTP 403 from this host.
- The [MediaTek Labs 4.19 XGF source](https://github.com/MediaTek-Labs/common-kernel-4.19/blob/e440f21fa5dca203a027f31c2ed30af50e6b05c7/drivers/misc/mediatek/performance/fpsgo_v3/fbt/src/xgf.c)
  calls an externally supplied runtime callback. The inspected Oppo,
  Xiaomi, Teracube, and OnePlus trees also yielded callback glue and
  declarations for the missing functions.
- A native replacement can start at `XGF_QUEUE_END` in the existing
  FPSGO path. Estimate render-thread CPU time from scheduler accounting
  over successive frames, reuse the existing `xgf_ema_cal()` smoothing,
  and pass the result into the existing FBT policy. Validate timestamps,
  task lifetime, missed frames, and cleanup before adding dependency
  accounting or a target-FPS policy. Vendor-module compatibility and
  EMA2 parity are separate from that first working estimator.

## Release rule

No audit-branch change may be released until it has a full T227U build, the
vendor CRC gate passes and is applied, and on-device boot/radio/governor/storage
smoke tests are complete.
