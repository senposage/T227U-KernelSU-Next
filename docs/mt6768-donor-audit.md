# MT6768 donor audit

Donor: `mt6768-S/android_kernel_xiaomi_mt6768`, branch `lineage-24.0`,
pinned at `31c653fb625385710583e329cdbf56e56f17a4ae`.

The donor graph includes complete Linux/Android ancestry. The audit
targets its recent overlay and backports. On 2026-11-21, the first 100
commits were screened by subject, the next 300 were queried for matching
subsystems, and the candidates below were checked against their diffs and
the T227U call paths. The working branch is `codex/t227u-kernel-bringup`.

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
  ordering is now carried on `codex/t227u-kernel-bringup`.

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
- The supplied Verizon `T227USQSCEYE2` boot image reports
  `4.19.191-28847731-abT227USQSCEYE2`. Its embedded config disables FPSGO
  v3. The kernel export table has none of the vendor FPSGO module's 16
  XGF imports, and the boot ramdisk contains no kernel modules. The
  vendor module has matching vermagic and is listed in `modules.load`.
  The Canadian `T227UVLSCFYE2` boot image also disables FPSGO v3.
- The [MediaTek Labs 4.19 XGF source](https://github.com/MediaTek-Labs/common-kernel-4.19/blob/e440f21fa5dca203a027f31c2ed30af50e6b05c7/drivers/misc/mediatek/performance/fpsgo_v3/fbt/src/xgf.c)
  calls an externally supplied runtime callback. The inspected Oppo,
  Xiaomi, Teracube, and OnePlus trees also yielded callback glue and
  declarations for the missing functions.
- Build #26 adds native render-thread runtime accounting to the
  MT6765 FPSGO variant. Queue-end captures a scheduler sample before
  dispatching work. XGF keeps a baseline per render thread and buffer,
  rejects stale samples and PID reuse, and bounds CPU time by the frame
  interval. The existing EMA, FBT, FSTB target-FPS policy, and render
  thread clamp handle the result. The code retains the layout of the
  module-facing XGF render structure. Vendor EMA2 and worker dependency
  prediction are outside this implementation. See the build ledger for
  verification and the tablet test.

## Android 14 GSI frame delivery

- The installed Lineage TrebleDroid GSI has the
  [MediaTek GED KPI Surface hooks](https://github.com/xiaoleGun/treble_build_miku/blob/Vampire_v2/patches/trebledroid/platform_frameworks_native/0009-Add-MTK-GED-KPI-support-to-fix-broken-Mediatek-gpufr.patch).
  The device property `persist.sys.phh.mtk_ged_kpi` is already `1`,
  and `/proc/ged` is live. The MT6765 GPU Makefile builds GED through
  its platform selection even with `CONFIG_MTK_GED_SUPPORT` unset.
- A six-second function trace on #26 recorded 58 GED queue, 58 dequeue,
  and 58 acquire calls from Asphalt's `Thread-13`. The thread belonged
  to the Asphalt process. The earlier #26 trace saw zero FPSGO queue
  events while the game's buffers were presenting. The GED trace is at
  `/home/legit-su/t227u-20261121-ged-asphalt.log`.
- Build #27 adds the kernel-side adapter behind a root-only process
  selection. It samples the caller at a successful GED queue and uses
  the window ID for render state, then forwards completion into native
  XGF and FBT. GED keeps its existing GPU policy. Queue-start and
  dequeue timing remain available through the legacy FPSGO path.
- The adapter test build is recorded in the build ledger. The earlier
  #26 GED probe used a private trace instance and changed no Android
  or kernel settings.

## T227U vendor module ABI audit

All ten archived modules matched the SHA-256 of their device copies.
The audit compared strong undefined symbols and `__versions` imports
against the patched build #26 kernel exports and the archived module
exports. On the running #25 tablet, `/proc/modules` listed Wi-Fi,
Bluetooth, and the three connectivity providers. The current kernel
ring had no matching early loader messages, so the table reports
binary compatibility and load state separately.

| Vendor module | Result |
| --- | --- |
| `fpsgo` | Eight missing XGF imports. The native frame path in #26 runs inside the kernel independently of the vendor glue module. |
| `fmradio_drv_mt6631` | Five CRC mismatches: `platform_device_unregister`, `platform_device_put`, `platform_device_alloc`, `platform_device_add`, `gpiod_set_raw_value`. |
| `gps_drv` | One CRC mismatch: `nonseekable_open`. |
| `trace_mmstat` | Four CRC mismatches: `seq_putc`, `init_task`, `contig_page_data`, `bpf_trace_run3`. |
| `met` | Three required imports resolve with matching CRCs. It was absent from `/proc/modules`, so its load status needs independent evidence. |
| `bt_drv_connac1x`, `connfem`, `wlan_drv_gen4m`, `wmt_chrdev_wifi`, `wmt_drv` | Loaded on #25. Their required imports resolve in the #26 kernel or a vendor dependency and the built-in CRCs match. |

### FM and GPS compatibility review

Boot #26 logged the five FM CRC disagreements and the GPS
`nonseekable_open` disagreement during normal vendor module startup.
Both modules have matching vermagic.

- GPS `gps_emi_open` forwards the kernel-supplied inode and file
  pointers directly and returns the callee's integer result.
  `nonseekable_open()` clears three seek-related mode bits on the
  kernel's own file object. The GPS callback-table slots and character
  device size match the build's DWARF layouts.
- FM stores the pointer from `platform_device_alloc()`, adds it,
  releases it with `platform_device_put()` if add fails, and unregisters
  it after registration. Its probe reads the device-tree node from the
  same field offset as build #26. The platform driver and file callback
  tables match the build's layout. The GPIO call sites pass descriptors
  from `gpio_to_desc()` and levels 0 or 1.
- Added the six reviewed CRCs to the map. The gate applied all 203 entries
  on a disposable copy of #26. A second scan of all ten vendor modules
  found zero built-in CRC mismatches for FM and GPS and verified that
  only the six intended export CRCs changed. Device load and hardware
  tests remain pending. The #26 kernel and ZIP retained their hashes.
- `trace_mmstat` is optional diagnostics and is out of scope for this
  follow-up. Its four entries were left out of the map.

The `fpsgo.ko` errors are unresolved imports. CRC overrides do not
create those exports. See the build ledger for the native driver
smoke test and the Asphalt frame-event observation.

## 2026-11-21 resumed donor review

Screened entries 401–800 at the pinned head for additional T227U
storage, memory, CPU-frequency, and scheduler candidates. Also
revisited charging events and the active eMMC command logger.

| Donor | T227U disposition |
| --- | --- |
| `51da83359def` | Ported in `b1e9f9ae7`. Widen the legacy DT-v1 frequency value before the kHz-to-Hz conversion. The stock bins and MTK PPM policy are outside this path. |
| `827999220f8e` | Adapted in `01f40c929`. The bounded uevent helper writes the full name and value, then the driver uppercases only the property key in the appended entry. This removes one allocation per reported property. The host check compared 292 property names and covered both uevent capacity limits. Charging timing remains unmeasured. |
| `7c113506163a`, `cc312deaba30` | Ported together in `c21e15097`. Bound the explicit FQ-CoDel quantum and use one clamped MTU for its defaults. The current tablet uses `fq` by default, so this is hardening for interfaces configured to use FQ-CoDel. |
| `f9f6e27e2d07` | Already present in `f5b5f57da`. Checked both the maximum filename length and the remaining inline dentry slots in the rehash path. |
| `8d734db97256` | Already in the initial import. The F2FS same-inode move check uses the expanded length before entering the block exchange. |
| `c6ff70f2b100` | Direct import targets the generic `mtk-sd` driver. The running config selects ComboA, whose logger timestamps and updates a shared ring for msdc0. A ComboA performance experiment needs a separate history toggle that preserves register/error dumps and accounts for in-flight command/response pairs when toggled. |
| `34ed75ea6749`, `3667698913e5`, `cf9f7abb0ac4` | The first two are for the DaVinci MMC host. The arm64 hibernation fix is outside the compiled T227U configuration. |

The three ports above linked in build #29 with the established r8
configuration and passed the applied vendor CRC gate. See the build
ledger for the build inputs and the remaining device checks.

## Release rule

No audit-branch change may be released until it has a full T227U build, the
vendor CRC gate passes and is applied, and on-device boot/radio/governor/storage
smoke tests are complete.
