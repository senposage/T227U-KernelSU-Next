# T227U runtime test log

This is the durable companion to `BUILD_LEDGER.md`: it records device-side
experiments, their exact temporary settings, raw-capture locations, and the
observed result. A setting is never considered release policy merely because
it was tested here.

## R-20261120-ZRAM-MAP-LOAD

### Basis

- Device kernel: `4.19.191-28847731-abT227USQSCEYE2`.
- Baseline swap: one manual LZ4 ZRAM device, `/dev/block/zram0`, 2 GiB logical
  capacity, priority `-2`, seven compression streams, `page-cluster=0`.
- No disk-backed swap was created or enabled. The vendor automatic RamPlus
  fstab entry was already disabled manually; B-20261120-VR-INSTALLER packages
  the same persistent change in `E:\\T227U-VR.zip`
  (`59f689a28162ed012c1ad87b42f6a525e32aa58016edeefb8d2d62e68457fcd7`).
- All VM settings below are runtime-only. Nothing has been written to a
  ramdisk, vendor fstab beyond the RamPlus disablement, or Kernel Adiutor's
  apply-on-boot state.

### Capture A: aggressive temporary profile

- Temporary values: `swappiness=180`, `vfs_cache_pressure=125`,
  `min_free_kbytes=16384`, `watermark_scale_factor=64`; LZ4 and
  `page-cluster=0` unchanged.
- Duration: 112 samples / 258 seconds through map load and two races.
- Peak ZRAM: 1,499 MiB logical pages using 569 MiB physical ZRAM memory.
- Delta swap traffic: 1,866 MiB in and 1,963 MiB out.
- Peak memory PSI (`avg10`): `some=22.75%`, `full=9.12%`.
- Kernel `oom_kill=0`; Android lowmemorykiller killed background processes to
  protect the foreground game when free swap crossed its 417,788 KiB guard.
- Raw files: `E:\\T227U-race-20261120-live.log` and
  `E:\\T227U-race-post-logcat.txt`.

### Capture B: conservative temporary profile

- Temporary values: `swappiness=100`, `vfs_cache_pressure=100`,
  `min_free_kbytes=6484`, `watermark_scale_factor=10`; LZ4 and
  `page-cluster=0` unchanged.
- Short post-map-load race segment: 41 samples. Peak full PSI fell to 4.54%,
  with 189 MiB in / 446 MiB out. This was encouraging but not a clean full
  map-load comparison.
- Raw files: `E:\\T227U-race-conservative.log` and
  `E:\\T227U-race-conservative-post-logcat.txt`.

### Capture C: conservative profile, marker-separated

- Capture began at the launcher. User markers identify map load, race start,
  race end, and return to menu in the raw log.
- Launcher (136 seconds): peak full PSI `0.58%`.
- Map load (23 seconds): 626 MiB swap-out, 133 MiB swap-in, 58,407 direct
  reclaim pages, peak `some=17.56%` / `full=8.99%` PSI. This is the dominant
  pressure event.
- Race (44 seconds): no direct reclaim, 246 MiB swap-out / 100 MiB swap-in,
  peak full PSI `2.74%`.
- Post-race demo (39 seconds): 139 MiB swap-out / 103 MiB swap-in, peak full
  PSI `4.38%`.
- Kernel `oom_kill=0`. At map load, lowmemorykiller observed a transient
  13,512 KiB free-swap trough and 212% thrashing, then killed lower-priority
  GMS/YouTube/keyboard/background processes. This trough is shorter than the
  two-second sampler interval and must not be inferred from its recovered
  minimum alone.
- Raw files: `E:\\T227U-race-conservative-clean.log` and
  `E:\\T227U-race-conservative-clean-post-logcat.txt`.

### Current conclusion and pending A/B

- Keep the conservative profile for testing; it improves steady-race pressure
  versus the aggressive profile.
- Do **not** add disk-backed swap: it would defer the lowmemorykiller while
  converting the map-load burst into slow-storage stalls.
- A reboot was approved for a 3 GiB *logical* ZRAM capacity A/B. This does
  not reserve 3 GiB of physical RAM; physical use will be judged by
  `/sys/block/zram0/mm_stat`, PSI, and lowmemorykiller records.
- Post-boot setup succeeded on 2026-11-20: `/dev/block/zram0` is 3,145,724
  KiB at priority `-2`; `disksize=3221225472`; LZ4 is selected; and the
  conservative VM profile was restored. The kernel readback clamps
  `max_comp_streams` to `3` (not the requested seven), so three is the actual
  tested value.
- The first reinitialization attempt was rejected safely because this Toybox
  `swapon` rejects an explicit negative `-p` priority. Its rollback left ZRAM
  offline rather than pretending success; the immediate repair used Toybox's
  default `swapon` priority, which read back as the original `-2`.
- Pending: repeat the marker-separated launcher → map-load → race capture on
  this 3 GiB logical-ZRAM configuration before drawing any conclusion.

### R-20261120-ZRAM-3G result and rollback

- Marker-separated 3 GiB run: map load (15 seconds) produced 227 MiB
  swap-out, 46 MiB swap-in, 144,418 direct-reclaim pages, and `full` PSI
  `3.95%`; the 108-second race produced 554 MiB out / 173 MiB in and peak
  full PSI `1.77%`. The larger device reduced full-system stalls but did not
  solve the allocation burst.
- Lowmemorykiller's guard grew with swap capacity from 417,788 KiB to
  629,144 KiB. During the run it still observed 62,840 KiB free swap with
  200% thrashing, then killed background work repeatedly. At 16:52:35 it
  killed `com.gameloft.android.ANMP.GloftA9HM` itself. This is a failed
  configuration for this latency target.
- Raw files: `E:\\T227U-race-zram3g-clean.log`,
  `E:\\T227U-race-zram3g-markers.log`, and
  `E:\\T227U-race-zram3g-post-logcat.txt`.
- Restored state: after a safe reboot, vendor RamPlus correctly left ZRAM
  unconfigured; manual 2 GiB ZRAM was recreated and verified with LZ4,
  priority `-2`, `page-cluster=0`, and the conservative VM profile.
  The active-kernel readback clamps compression streams to `3`.
- Decision: retain 2 GiB logical ZRAM as the tested ceiling. Kernel Adiutor
  follow-up must expose `watermark_scale_factor`, represent ZRAM readback,
  select LZ4 when the kernel advertises it, and keep this profile opt-in;
  it must not expose the failed 3 GiB setting as a default.

## R-20261120-DIRECT-LZ4-MAP-LOAD

- Test kernel: T227U-LZ4 build #19, source `cbddad6a1`.
  Kernel Adiutor applied 2000 MiB LZ4 ZRAM, priority `-2`, without
  disk swap. VM profile was `100/100/6484/10/0` for swappiness, cache
  pressure, min-free KiB, watermark scale and page-cluster respectively.
- Map-load marker: device epoch `1795227754`. LMKD killed game PID
  11539 at `1795227764.825`, its foreground score was `0`, and the
  kill record listed 1,371,760 KiB RSS and 132,280 KiB of swap. LMKD
  cited a breached minimum watermark and 24,568 KiB of usable swap
  against its 409,596 KiB threshold. Device property
  `ro.lmk.swap_free_low_percentage` read back as `20`.
- Samples bracketing the marker and kill show about 139 MiB swap-out,
  7 MiB swap-in, 115,293 direct-scan pages and peak sampled full memory
  PSI of 3.57% through the kill. Kernel OOM kills and first three ZRAM
  I/O error counters remained zero in this window.
- Raw `SwapFree` was about 1.5 GiB in the samples around the kill.
  AOSP LMKD caps reported usable ZRAM swap at an estimate based on
  free RAM and inactive file cache. See
  [AOSP LMKD get_free_swap](https://android.googlesource.com/platform/system/memory/lmkd/+/refs/heads/main/lmkd.cpp#1999).
  This explains why LMKD can report much less headroom than raw
  `SwapFree`. Earlier LMKD-reported troughs above should also be read
  as LMKD's estimate rather than proof that ZRAM filled.
- Result: map-load run terminated, so no race measurements exist. The
  earlier run and this run cannot establish a codec regression. Repeat
  against the original path at exactly 2000 MiB, using the same map,
  post-boot settling time and background workload.
- Local logs: `E:\T227U-LZ4-AB-20261120-211926.log` and its
  `-markers.log`, `-lmk.log` and `-kernel.log` companions.

### Worst-case retry on the same boot

- The second map load started at device epoch `1795227941`, followed by
  the user-reported race start at `1795227965`. The game reached the race
  and PID 16181 survived through the capture's last sample at
  `1795228065`. The user confirmed the race finished after the sampler
  ended. The report was marked at `1795228144`, with the same PID alive,
  no ZRAM I/O errors and no kernel OOM kills. This is a finish
  confirmation, so the report timestamp is excluded from the race timings.
- The samples bracketing map load showed 345 MiB of swap-out, 92 MiB
  of swap-in, 165,836 direct-scan pages and a peak memory full PSI
  `avg10` of 9.07%. Sampled I/O full PSI reached 17.43%.
- From the sample just before race start through capture end, the
  system reported 459 MiB of swap-out, 82 MiB of swap-in and 66,149
  additional direct-scan pages. Direct scanning stopped increasing
  late in the run. Peak physical ZRAM use was about 495 MiB.
- LMKD killed background processes. No foreground game kill, kernel
  OOM kill or ZRAM I/O failure was observed during the retry.
  The first attempt and this retry demonstrate variable app survival
  under a worst-case workload. Reusing the same boot after the first
  LMKD cycle changes the starting state and prevents a matched A/B.
- The retry is in the same capture and marker log, with filtered
  LMKD events in `E:\T227U-LZ4-AB-20261120-211926-retry-lmk.log`.

## R-20261120-LZ4M-PAGE-IN

- Modular LZ4 build #20, 2000 MiB LZ4 ZRAM, conservative VM profile.
  Map load marked at device epoch `1795229124`. The race started at
  `1795229175` and the capture stopped there as requested.
- From the last sample before map load to the race-start readback,
  `pswpin` rose by 16,659 pages (65 MiB) and `pswpout` rose by
  135,599 pages (530 MiB). ZRAM was the only active swap device.
  Game PID 11010 survived. ZRAM I/O errors and the kernel OOM count
  stayed at zero.
- Result: the modular backend passed the live page-in check through
  race start. This run did not collect full-race timing.
- Local logs: `E:\T227U-LZ4M-20261120-214256.log` and
  `E:\T227U-LZ4M-20261120-214256-markers.log`.

## R-20261120-LZ4D-ASPHALT

- Kernel #22 with the original 16 KiB Asphalt dictionary loaded by KA
  and 2040 MiB of logical ZRAM. Map load was marked at device epoch
  `1795232047` and race start at `1795232097`. The same game PID
  remained alive through the later training capture. LMKD killed
  background processes. Kernel OOM and ZRAM I/O error counters
  stayed at zero.
- Between the nearest samples before the map and race markers, the
  tablet swapped in about 158 MiB, swapped out 673 MiB and added
  236,305 direct-scan pages. Near race start, 1,031 MiB of logical
  pages occupied about 341 MiB of physical ZRAM. Earlier plain-LZ4
  runs reported about 34 to 38% physical/logical at comparable
  footprints. The runs use different starting states and cannot
  establish a causal speed or memory improvement.
- Fresh training capture at epoch `1795232289`: 32 MiB from the
  same four offsets in the live game Java heap, with 844 nonzero
  pages. Memory capture activity is excluded from performance
  interpretation.
- Cross-check using LZ4 CLI compressed size, relative to plain LZ4:

  | Held-out capture | Pages | Original 16 KiB | Retry 16 KiB |
  | --- | ---: | ---: | ---: |
  | Original | 40 | 30.29% smaller | 22.01% smaller |
  | Retry | 85 | 13.33% smaller | 25.46% smaller |

  The diagonal compares held-out pages from the candidate's own
  session. The off-diagonal compares separate runs of the same
  workload. None of these are kernel throughput measurements.
- Local logs: `E:\T227U-LZ4D-20261120-223204.log`,
  `E:\T227U-LZ4D-20261120-223204-markers.log`, and
  `E:\T227U-LZ4D-20261120-223204-lmk.log`.
  The cross-check JSON and retry dictionary are local under
  `E:\zram-bench-20261120`. The retry dictionary was not applied.

### Retry dictionary after reboot

- The retry candidate was installed later on request. Its SHA-256 was
  verified before and after reboot:
  `7a9dd6d89f4a9f52e69f51877efcf3d57256506cfeb7662ff6e7c03493b9acea`.
  ADB confirmed a new boot ID and 16,384 dictionary bytes active on
  2040 MiB ZRAM at priority `-2`, with zero ZRAM I/O errors.
  The original dictionary remains backed up locally.
- The extended trainer captured two timed samples of a synthetic ARM64
  process over rooted ADB on the tablet. It collected 162 pages each
  time, retained 131 usable unique pages overall, and produced a
  16 KiB dictionary. The original file-input training workflow also
  passed. Synthetic process and dictionary artifacts were temporary.

## 2026-11-21 build #25 boot and FPSGO

- ADB verified kernel stamp `#25 SMP PREEMPT Sat Nov 21 01:43:22 EST 2026`
  and `sys.boot_completed=1`. The WLAN interface had an IPv4 address.
- ADB identified the stock vendor partition as `SM-T227U`,
  incremental build `T227USQSCEYE2`, with Verizon sales code `VZW`
  and bootloader `T227UVLSCFYE2`. These values identify the XGF
  compatibility target independently of the T225-derived source tree.
- ZRAM activated at 2040 MiB and priority `-2` with the existing
  16,235-byte test dictionary. The kernel reported 101 swap-ins and
  4,597 swap-outs with all four ZRAM I/O counters at zero.
- `/sys/kernel/fpsgo/common/fpsgo_enable` returned `1`. The in-tree
  FPSGO v3 and XGF sysfs nodes are present. Frame activity during a
  game and XGF runtime estimation still need verification.
- The stock `fpsgo.ko` SHA-256 matched the archived module. Eight of
  its 16 XGF imports are exported by #25, including the callback
  pointer globals and `notify_xgf_ko_ready`. Boot logs report the eight
  implementation functions as unresolved:
  `xgf_ko_init`, `xgf_est_runtime`, `xgff_est_runtime`,
  `xgff_update_start_prev_index`, `xgf_ema2_init`,
  `xgf_ema2_predict`, `fpsgo_xgf2ko_calculate_target_fps`,
  and `fpsgo_xgf2ko_do_recycle`.
- The module initializer calls `xgf_ko_init`, fills the callback pointer
  globals, and then calls `notify_xgf_ko_ready`. The in-tree
  `init_xgf_ko()` performs tracepoint setup at kernel boot. Restoring
  vendor parity requires a provider for the missing functions with the
  matching ABI and lifecycle.
- The ten archived T227U vendor modules were checked for a definition of
  `xgf_ko_init`. None supplied it.

## 2026-11-21 build #30 ComboA command-history A/B

- ADB confirmed booted build #30 from `T227U-IO.zip`. FreeKiosk
  (`com.freekiosk`) and tinyCam Pro (`com.alexvas.dvr.pro`) were
  force-stopped before testing and remained stopped through the run.
  Kyber was active on `mmcblk0`, and CPUs 0, 5, and 6 remained
  online in the before and after snapshots.
- The harness created and synced a 192 MiB scratch file on F2FS.
  It validated the file's allocated extents with FIEMAP and used
  direct reads of those extents from the userdata block device.
  Each trial used the same 4,096 random 4 KiB reads and 1,024
  sequential 128 KiB reads at queue depth one. Test data was removed.
- One warm-up was followed by eight trials, with history
  on/off/off/on/off/on/on/off. The earlier zero-delta readings were
  traced to disabled block I/O accounting. Both modes then ran with
  accounting enabled, and each completed trial advanced
  `mmcblk0` by 294,912–294,960 read sectors.

| Direct reads | History on, MiB/s | History off, MiB/s | On p95 / p99, µs | Off p95 / p99, µs |
| --- | ---: | ---: | ---: | ---: |
| Random 4 KiB | 25.26 | 25.07 | 189 / 213 | 196 / 221 |
| Sequential 128 KiB | 203.59 | 204.60 | 724 / 759 | 717 / 756 |

- Entries are medians of four trial summaries per mode. Sequential
  throughput was 0.5% higher with logging off. Paired throughput
  and tail-latency differences changed direction across trials.
  This run showed no consistent read performance gain from disabling
  command history.
- All reads completed. The kernel log showed no new MMC or F2FS
  messages during the measured run. The eMMC reported life-time
  estimates `0x01 0x01` and pre-EOL status `0x01` before and after.
  Restored command history to on and block I/O accounting to off.
  The scripts, trial data, and health snapshots are archived locally at
  `/home/legit-su/t227u-io-ab-20261121.0RMvoZ`.

## 2026-11-21 CPU hotplug and MicroG install on build #30

- The initial online mask was `0,5-6`. CPUFreq listed the performance
  policy as CPUs 0–3 and the efficiency policy as CPUs 4–7. Both
  online little cores ran an affinity-pinned workload at full load.
  An unpinned six-worker run saturated the three available cores.
- The final PPM policy requested four cores per cluster and its thermal
  limit was inactive. The KA settings contained 18 legacy per-core
  online or chmod records. Those records offlined CPUs 1–4 and 7
  and set the node permissions to 0444. The user settings were backed
  up locally before removing those records. All other records were
  preserved. The affected node permissions were restored and the
  tablet reported `0-7`.
- A three-second unpinned eight-worker run advanced the busy
  counters for little CPUs 4–7 by 300, 302, 301, and 299 ticks.
  The mask remained `0-7` after the load. Both CPUFreq policies
  retained their governor and frequency limits.
- The ReVanced MicroG install used ART's `speed` compiler filter
  for ARM64 and ARM. The two passes completed successfully in 70.6
  and 71.7 seconds. A live sample showed the compiler using about
  4.5 cores with low I/O wait. Android reported the app installed.
- Settings backup and the list of removed entries are private at
  `/home/legit-su/t227u-hotplug-20261121.caON3A`. The updated KA
  APK was installed. Android returned the expected APK checksum
  and CPU online mask `0-7`.

### Internal eMMC speed with all eight CPUs online

- After the CPU recovery and MicroG installation, #30 reported
  CPUs 0–7 online and the `mq-deadline` scheduler. The benchmark
  used three queue-depth-one direct read trials on the physical
  userdata extents of a 192 MiB scratch file.
- Median sequential 128 KiB read was 214.20 MiB/s. The three runs
  reported 217.56, 214.20, and 203.89 MiB/s. Median random 4 KiB
  read was 29.78 MiB/s (about 7,623 IOPS). The three runs reported
  29.90, 29.78, and 21.42 MiB/s. Median run p99 was 777 µs
  sequential and 199 µs random.
- The 192 MiB F2FS scratch-file write including fsync was 103.19
  MiB/s. The raw partition was opened read-only and the scratch file
  was removed. Each read trial advanced the eMMC sector counter
  by at least the expected 425,984 sectors.
- Command history was on during the measurement. It was turned off
  afterward per the user's preference. The original block I/O
  accounting state was restored. Local log and harness
  `/home/legit-su/t227u-raw-io-20261121.UwKFSV`.

## 2026-11-21 eMMC scheduler and writeback tuning

- The SM-T227U ran build #30 with all eight CPUs online and ComboA
  command history off. The starting queue settings were
  `mq-deadline`, a writeback throttling target of 2,000 µs,
  128 requests, and 128 KiB readahead.
- Nine interleaved scheduler trials used a 192 MiB F2FS scratch
  file. Each ran 4,096 direct 4 KiB random reads and 1,024 direct
  128 KiB sequential reads through allocated userdata extents.
  The mixed test repeated the random reads while a background
  scratch-file writer flushed each 1 MiB of data. eMMC sector
  counters confirmed the I/O, and the raw partition was read-only.

| Scheduler | Random 4 KiB alone, MiB/s | Sequential alone, MiB/s | Mixed random p99, ms |
| --- | ---: | ---: | ---: |
| mq-deadline | 29.30 | 196.70 | 7.66 |
| Kyber | 28.19 | 204.17 | 7.69 |
| none | 31.20 | 215.43 | 12.02 |

- The latency-focused comparison brought `mq-deadline` forward
  into nine interleaved writeback-target trials.

| Writeback target | Mixed read throughput, MiB/s | Read p95, ms | Read p99, ms | Writer, MiB/s |
| --- | ---: | ---: | ---: | ---: |
| 1,000 µs | 3.67 | 7.14 | 7.35 | 61.99 |
| 2,000 µs | 3.42 | 6.67 | 8.16 | 63.23 |
| 4,000 µs | 3.46 | 8.06 | 8.29 | 61.30 |

- Values are medians of per-trial summaries. Setup, reads, and
  write contention used disposable files. Local harness and
  per-trial data are at `/home/legit-su/t227u-io-tune.qJYDWZ`.
- A repeated 2,000/1,000/1,000/2,000 µs comparison did not
  reproduce the first-pass p99 gain. The two-trial median
  p99 was 8.82 ms at 2,000 µs and 11.97 ms at 1,000 µs.
  The 2,000 µs setting also had higher read and write throughput
  on the repeat, so it remained the writeback target.
- Tested `mq-deadline` batch sizes 0 and 4 in six interleaved
  mixed trials using 2,048 direct reads each. At batch 4, reads
  reached 3.84 MiB/s and p99 was 7.63 ms. At batch 0, reads
  reached 3.06 MiB/s and p99 was 8.31 ms. The three batch-4
  p99 values were 7.63, 7.66, and 7.63 ms, below all three
  batch-0 results. Writer throughput rose from 59.81 to 64.19
  MiB/s.
- Applied `mq-deadline` with `fifo_batch=4` and
  `wbt_lat_usec=2000` for the current boot. ADB readback after
  a dwell confirmed both values. The tablet kept 128 requests,
  128 KiB readahead, eMMC command history off, and CPUs 0–7
  online. The kernel log showed no matching MMC or F2FS errors.

### Buffered readahead and the tablet's saved I/O profile

- The earlier scheduler trials used direct reads. The readahead
  follow-up held `mq-deadline`, `fifo_batch=4`, and
  `wbt_lat_usec=2000` fixed. Each setting ran three trials of
  a 96 MiB buffered sequential read and a 16 MiB buffered read
  made up of shuffled groups of four adjacent 4 KiB pages.
  The test evicted only its scratch file's cache between runs
  and verified eMMC sector counts.
- The Samsung queue accepts only the `temporary N` form of a
  write to `read_ahead_kb`. The former KA entry used a plain
  numeric write, and the active value had remained 128 KiB.
  Every measured setting was read back before the trial.

| Readahead | Sequential, MiB/s | Sequential p99, µs | Clustered, MiB/s | Clustered p99, µs | Clustered physical reads, MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| 32 KiB | 158.49 | 264.6 | 22.68 | 351.1 | 60.00 |
| 64 KiB | 207.11 | 326.0 | 22.59 | 340.2 | 60.00 |
| 128 KiB, small-window pass | 250.88 | 461.5 | 24.86 | 393.5 | 52.00 |
| 128 KiB, large-window pass | 255.46 | 411.4 | 24.15 | 407.5 | 52.00 |
| 256 KiB | 283.00 | 678.1 | 24.35 | 402.5 | 51.95 |
| 512 KiB | 298.64 | 1220.8 | 19.91 | 549.7 | 84.00 |
| 1024 KiB | 296.99 | 2363.4 | 19.89 | 551.5 | 84.00 |

- Kept 128 KiB as the balanced setting. Against 64 KiB in the same
  pass, it read about 21% faster sequentially and 10% faster in
  clusters, with 8 MiB less physical cluster traffic. The 64 KiB
  setting reduced clustered p99 by about 53 µs. The larger windows
  traded away tail latency or clustered throughput.
- Applied the chosen settings live and stored the four commands in
  the device's enabled KA I/O Apply on Boot category. The sequence
  selects `mq-deadline`, writes `fifo_batch=4`, writes
  `wbt_lat_usec=2000`, then writes `temporary 128` to
  `read_ahead_kb`. Verified the live values and read back the
  persisted settings after reopening KA. A reboot replay is pending.
- Local benchmark harness and results
  `/home/legit-su/t227u-readahead.bFIMn7`. Private backup of
  the tablet's KA settings
  `/home/legit-su/t227u-io-default.3VOF6d`.

## 2026-11-21 T227U VCORE gear audit

- Asked the running T227USQSCEYE2 build #30 for
  /sys/devices/platform/10012000.dvfsrc/10012000.dvfsrc:dvfsrc-up/dvfsrc_opp_table.
  It reported firmware type 2, four VCORE gears, three DDR rates,
  and sixteen combined levels. Matching the MT6765 firmware table
  indices, VCORE OPPs 0 through 3 are 800, 700, 675, and 650 mV.
  The independent dvfsrc_dump read the idle PMIC rail at 650 mV.
  The OC voltage printed by the GPU OPP table is nominal. All GPU
  requests above 700 mV select VCORE OPP 0, whose firmware target
  on this tablet is 800 mV, including the 825 mV nominal request
  in GPU test build #33.
- The active MT6765 driver in kernel/drivers/soc/mediatek/mtk-dvfsrc.c
  uses the mt6761_set_vcore_level handler. Its two-bit request field
  already encodes all four gears. The kernel reads their voltages
  through the secure VCOREFS service. Extending the GPU or Linux
  DVFSRC table alone cannot address a fifth gear.
- The linked spm_dvfs_pwrap_cmd helper submits a secure PMIC-wrapper
  command. The MT6768 driver uses it to change an existing gear, but
  its driver and slot mapping are separate from the MT6765 build.
  T227U firmware support and the command slot for its top gear have
  not been confirmed. If supported, reprogramming an existing gear
  changes shared VCORE for every requester of that gear. The legacy
  GPU fixed-voltage path writes the shared regulator directly and
  does not check the regulator return code.
- Read-only audit. There was no firmware, voltage or clock write,
  and no reading of the top gear under a sustained workload.

### Build #33 at 825 MHz

- The user reported that the tablet held the 825 MHz setting. An
  ADB read showed build #33 on Segment 3 with the ceiling and
  operating point at 825 MHz and measured clock at 824.992 MHz.
  VCORE OPP 0 read back as 800 mV in both GPU and DVFSRC dumps,
  versus the nominal GPU table entry of 825 mV. The GPU driver
  reported VSRAM at 875 mV. No limit was active. This was a
  point-in-time read with zero instantaneous GPU utilization.

## 2026-11-22 GPU 1 GHz and VCORE command identification

- On build #34 the user observed immediate visual corruption at
  1,000 MHz and reported that the tablet remained responsive.
  Later ADB reads showed the ceiling back at 900 MHz with a
  399.952 MHz idle clock. The GED log counted 29,055 ms at its
  nominal 1 GHz OPP and 328 ms at 975 MHz. These are software
  residency counters. The sample did not confirm an exact 1 GHz
  hardware clock or sustained rendering stability.
- The filtered post-event kernel and logcat snapshots contained
  no matching GPU faults, hangs, resets, or timeouts. Clock and
  rail were read only. The observation does not isolate voltage
  margin from the experimental divider or other clock behavior.
- Confirmed the live SPM command words on this firmware were
  0x152a002d, 0x152a0025, 0x152a001d, 0x152a0019, 0x152a0015.
  MT6357 register 0x152a is the VCORE voltage selector. Using the
  regulator driver's 518,750 µV base and 6,250 µV step, these
  commands decode to 800, 750, 700, 675, and 650 mV. The stock
  DVFSRC table exposes four requestable gears. The existing
  800 mV top gear matched command slot zero and regulator readback.

### Build #36 first device test

- Build #36 booted. The user tried the 806250 µV write and saw
  a zero GPU voltage reading. ADB confirmed the new sysfs file
  returned EOPNOTSUPP on read, the same preflight that guards
  writes. The SPM command still read 0x152a002d (800 mV).
  Independent DVFSRC readback showed 700 mV at active gear 1
  while the GPU-specific sample showed zero. No evidence of a
  successful SPM write appeared in this run.
- Build #37 validates the top gear through the power driver's
  normalized table, which numbers the highest voltage as OPP 0.
  Its readout keeps showing the independent rail voltage and
  active gear even when the command layout cannot be verified.

### Build #37 VCORE write confirmed under load

- Build #37 booted and the user applied 812500 µV. The first
  host check read programmed_uv=812500, live_uv=700000 and
  active_gear=1. A later check with Asphalt in the foreground
  read programmed_uv=812500, live_uv=812500 and active_gear=0.
  This confirms both the firmware slot write and the PMIC
  readback at top gear. No frequency stability claim follows
  from this voltage sample.

### MT6357 SRAM rail readback

- On running build #37, the MT6357 regulator sysfs reported
  vsram_others at 900000 µV while VCORE was in gear 1 at
  700000 µV. The GPU's separate 875 mV VSRAM readout comes from a
  constant in __mt_gpufreq_get_cur_vsram_volt. The MT6765 GPU
  header defines a nominal 100 mV buck variation minimum, but this
  driver does not use it to check the GPU VCORE transition or the
  shared-rail SPM command. The tested 812.5 mV VCORE value was
  read in an earlier, separate sample. A simultaneous high-load
  VCORE/SRAM margin and the board's actual minimum differential
  have not been established. If vsram_others stays at 900 mV and
  is the rail used by GPU SRAM, 900 mV VCORE would leave zero
  nominal headroom.

### Build #37 at a 950 MHz ceiling

- The user reported near stability at a 950 MHz ceiling and an
  825 mV top VCORE target. Two of five paired regulator samples
  measured 825 mV VCORE in gear 0 and 900 mV on vsram_others.
  The other three measured 700 mV VCORE in gear 1 while
  vsram_others stayed at 900 mV. The gear 0 samples establish a
  75 mV measured difference between these two rails at that
  operating point. They do not establish the GPU SRAM's minimum
  required difference or verify that 950 MHz was active at the
  instant of either voltage read.

### SM-T227U GPU logic and SRAM rail trace

- The live build #37 device tree maps /gpufreq/vcore-supply to
  phandle 0x37, the MT6357 buck_vcore. The modem driver's
  _vcore-supply also points at 0x37. GPU OPPs request a gear on
  this shared buck through MTK PM QoS. The experimental control
  changes SPM slot zero for the MT6357 VCORE selector at 0x152a.
  The GPU node has no software SRAM regulator binding and the
  override does not write an SRAM selector.
- The distinct MT6357 ldo_vsram_others uses phandle 0x90.
  The MT6765 reference drawing and an FCC-filed MT6765 phone
  schematic route VSRAM_OTHERS_PMU to the DVDD_MFG_SRAM pin.
  This is strong corroboration of the GPU SRAM rail. The
  T227U FCC exhibit lists its board schematic as confidential
  and exposes metadata only. The tablet-specific physical
  connection remains inferred.
- The PMIC driver reads each live voltage selector from its
  DA_VOSEL register. The sysfs number is the selected setting
  in microvolts. It is not an analog waveform or a measurement
  of droop or transient overshoot. The 875 mV GPU debug VSRAM
  value is hard-coded. The regulator readings for vsram_others
  stayed at 900 mV in the sampled VCORE gears, including the
  earlier 825 mV gear 0 test.
- The GPU source defines a 100 mV minimum variation but never
  uses that constant to check a transition. The springboard
  helper references only the 250 mV maximum, and the override
  does not enforce a VCORE-to-SRAM differential. With a 900 mV
  SRAM setting, 800 mV VCORE gives a nominal 100 mV gap, 825 mV
  gives 75 mV, and 900 mV gives zero. The #38 software maximum
  is not a qualified operating point. Hold the upper end of
  that experiment pending validation of SRAM headroom. No
  voltage or clock change was made during this trace.
- References: [MT6765 reference schematic, sheet 4](https://raw.githubusercontent.com/bizoonydb/digital-board/main/RDM6-Schematic.pdf#page=4),
  [FCC-filed MT6765 power schematic](https://fccid.io/m/0e8258fe1a39385ee4d997f156589bd8e3373a6579d5f0d2d233125ce3231059.pdf#page=1),
  and [SM-T227U FCC exhibit list](https://fccid.io/ZCASMT227U).

### VSRAM_OTHERS adjustment feasibility

- The MT6357 regulator driver exposes a voltage selector for
  ldo_vsram_others with 6.25 mV increments. Active and sleep
  selector fields are distinct. The source also names a
  tracking-status field. The presence of that field does not
  demonstrate an active VCORE-to-VSRAM tracking mode on the T227U.
  The established PMIC measurements showed 900 mV on this rail
  while VCORE changed across gears. VSRAM_PROC is a separate LDO.
- With VCORE at 875 mV, an SRAM rail of 975 mV would give a 100 mV
  gap. At the lowest published awake VCORE gear of 650 mV, the
  same fixed SRAM setting yields a 325 mV gap. That is below
  the proposed 350 mV comparison limit but above the 250 mV
  spread listed in this GPU driver. Voltage during suspend has
  not been established. Any coupled implementation needs to
  verify ordering, both rail readbacks, and every gear and
  suspend transition. No VSRAM change was made.

### Build #41 first boot

- ADB confirmed build #41 reached Android boot completion.
  The top VCORE ceiling reported 875000 µV. Before any
  test write, the DVS readback reported programmed and live
  VCORE at 800000 µV in gear 0, and the new SRAM control
  independently reported vsram_uv=900000, vcore_uv=800000
  and floor_active=0.

### Build #41 first live VSRAM write

- The user requested 925000 µV via the rail-test control. It
  reported vsram_uv=925000, vcore_uv=700000, active_gear=1
  and floor_active=1. The PMIC regulator class independently
  read 925000 µV. Top VCORE remained programmed at 800000 µV
  and the limit file advertised 825000 µV while the test was
  active. The sampled gap was 225 mV. No matching rail-test
  failure or VCORE checker line appeared in the kernel log.

### GPU voltage readout returning zero on build #41

- A 36-sample capture at the user's 843.75 mV top VCORE and
  950 mV VSRAM settings found 19 zero values in the legacy
  GPU dump. The independently read MT6357 VCORE selectors
  were 700 or 843.75 mV and VSRAM remained 950 mV. Neither
  regulator read zero. The GPU routine returns zero when its
  GPU power flag is clear, independent of shared VCORE.
  The flag printed later in the dump can change between
  those two reads during GPU gating.
- The legacy GPU dump also kept reporting a fixed 875 mV
  VSRAM during the capture. Logs are at
  /home/legit-su/t227u-20261122-rail-zero-samples.log.
  No voltage or clock change was made during capture.
