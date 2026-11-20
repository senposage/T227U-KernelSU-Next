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
