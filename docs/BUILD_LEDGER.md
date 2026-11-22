# T227U build ledger

This file is the authoritative record for every test and release artifact.
It is committed with the source so the code, its exact change set, the build
inputs, and the resulting ZIP can always be tied together.

## Rules

Before a ZIP is handed to the device, add a record containing all of:

1. A unique build ID and timestamp (America/New_York).
2. The Git branch and exact `HEAD` commit.
3. Every source or ramdisk hunk introduced since the prior tested artifact.
   Record `git show --format= --unified=0 <commit>` hunk ranges, never only a
   prose summary.
4. The exact build command, output directory, config, and CRC-gate result.
5. Artifact pathname, SHA-256, ramdisk basis, and observed device result.

No artifact is a release merely because it builds.  A test ZIP must be marked
`untested`, `booted`, `failed`, or `reverted` here after its device result is
known.

## Functional-trace gate

Compilation is not evidence that a change functions correctly. Before code is
committed or packaged, run a proportional functional preflight and record its
result in the commit body. The normal preflight is deliberately short: verify
the triggering call path, the actual type/ownership/lifetime, execution
context, and a concrete test path. Go deeper only when that check finds a
mismatch or the change crosses a high-risk boundary (boot, Binder, memory
management, ABI, concurrency, power, or storage).

For high-risk changes, explicitly check:

1. The entry point and concrete call path used on this device.
2. The real type, allocation/ownership, and lifetime of every changed object.
3. Execution context and lock/sleeping constraints for every added primitive.
4. ABI assumptions against the vendor modules and the post-link CRC result.
5. A runtime test that exercises the path. Until it exists, label the result
   **not yet tested**, never functional or releasable.

The local Git configuration uses `.gitmessage-t227u` plus verbose staged diffs
at commit time so this trace is visible while each commit is written.

## Common build inputs

- Branch: `codex/t227u-kernel-bringup`
- Source: `/home/legit-su/t227u-public-source`
- Output: `/home/legit-su/t227u-public-repro-build`
- Config: `gta7lite_00_defconfig`; FPSGO V3 enabled by commit `9c1e50771`.
- Toolchain: `/home/legit-su/kernel_src.oc.tar/toolchain_611/bin`
- Required post-link gate:

  ```sh
  OBJCOPY=/home/legit-su/kernel_src.oc.tar/toolchain_611/bin/llvm-objcopy \
    tools/t227u/vendor_crc_gate.sh apply \
    /home/legit-su/t227u-public-repro-build/vmlinux \
    /home/legit-su/t227u-public-repro-build/arch/arm64/boot/Image.gz
  ```

## Code-change provenance

| Commit | Exact touched hunks | Intent / disposition |
| --- | --- | --- |
| `7d5fc0d69` | `kernel/drivers/cpufreq/cpufreq.c`: `+1663..1664`, `+1684`; `kernel/fs/f2fs/xattr.c`: `577`, `+591` | Restored CPUFreq hotplug locking and F2FS xattr correctness. Retained. |
| `9c1e50771` | `kernel/arch/arm64/configs/gta7lite_defconfig:1944` | Enables in-tree FPSGO V3. Known booting. |
| `a0c343bad` | `kernel/drivers/android/binder.c`: `+69`, `+77`, `1067`, `+2156..2160`, `3066` | Restored Binder work. The `3066` context-name copy was later proven invalid for this tree and corrected by `ab5b59df0`. |
| `f4d2a7519` | `anykernel/anykernel.sh:35..46`; removed all lines of `init.sunflower-performance.rc`, `sbin/sunflower-performance.sh`, and `sbin/t225-perf-extra.sh`; added `init.t227u-ramplus.rc:1..12`, `sbin/t227u-ramplus-off.sh:1..9`; `kernel/mm/zsmalloc.c:49,2465,2470..2471,2476,2505,2507,2513,2626,2629,2653..2663` | Removes forced frequency/I/O tuning; adds the ZRAM lifetime protection. The RamPlus init hook was unsafe and removed in `04eb10ecc`. |
| `04eb10ecc` | Removed `anykernel/anykernel.sh:42..47`, all lines of `init.t227u-ramplus.rc`, and all lines of `sbin/t227u-ramplus-off.sh` | Removes the unsafe RamPlus init hook. Retained. |
| `ab5b59df0` | `kernel/drivers/android/binder.c:68`, `75`, `3064` | Restores `e->context_name = proc->context->name;`. This tree defines `context_name` as `const char *`; the previous `strscpy()` wrote through a null pointer on early Binder traffic. Retained. |
| `dc5718e2a` | `kernel/drivers/block/zram/zcomp.c:+42..210`; `kernel/drivers/block/zram/zcomp.h:+13..40`; `kernel/drivers/block/zram/zram_drv.c:2240..2258,2917,3011,3039,3054,3068` | Replaces pointer-per-CPU, preemption-disabled zcomp streams with embedded mutex-protected streams. The writeback decompression path now locks before `kmap_atomic()`. Direct compile of both touched objects passed; no linked image or runtime test exists yet. |

Reproduce an exact change set with, for example:

```sh
git show --format= --unified=0 ab5b59df0
```

## Build records

### B-20261120-FPSGO-GOOD

- Source state: includes `7d5fc0d69` and `9c1e50771`; Binder backports were
  still reverted.
- Artifact: `E:\T227U-KernelSU-Next-gta7lite-test-no-binder-fpsgo-v3.zip`
- SHA-256: `dde5326bca1df1f0f881e57e38c840270ee314a9dc9a154bb6d71da1d2032e60`
- Ramdisk basis: original FPSGO test ramdisk, including the old performance
  scripts.
- Device result: **booted**. This is the emergency known-good rollback.

### B-20261120-ZSFIX-RAMDISK-CLEAN

- Source state: `04eb10ecc` (before the Binder pointer correction).
- Artifact: `E:\T227U-KernelSU-Next-gta7lite-test-zsfix-no-ramplus-hook.zip`
- SHA-256: `48bb7e5de710b049a238093c6922a3f04796d02771137f4fea99dbc748540aac`
- Kernel-image SHA-256: `90b6e1158b89878bc871409a379d4d92329f3331454b905bef4e3488747d8f1e`
- Ramdisk basis: forced performance scripts and the unsafe RamPlus hook
  removed.
- Device result: **failed / not releasable**. It contained the invalid Binder
  context-name write identified in `ab5b59df0`.

### B-20261120-ZSFIX-AB-OLD-BINDER

- Source state: kernel image from `04eb10ecc`; exact installer and ramdisk
  copied from B-20261120-FPSGO-GOOD.
- Artifact: `E:\T227U-KernelSU-Next-gta7lite-test-zsfix-good-ramdisk-ab.zip`
- SHA-256: `2f684093b4b05761df55582a3aa64bd747e45c23f0f98529487a54ede30e1d6b`
- Device result: **failed before LOS splash**. This isolates the failure to
  the changed kernel image, not the AnyKernel ramdisk repack.

### B-20261120-ZSFIX-BINDERPTR-AB

- Timestamp: 2026-11-20 14:24 EST.
- Source state: `ab5b59df0`.
- Changed code relative to B-20261120-ZSFIX-AB-OLD-BINDER: only the Binder
  context-name correction listed above; ZRAM lifetime protection remains.
- Build: canonical LLVM build into
  `/home/legit-su/t227u-public-repro-build`; only `drivers/android/binder.o`
  was recompiled before relinking.
- CRC gate: **PASS** — 197 vendor exports verified, 0 remaining mismatches.
- Kernel-image SHA-256: `7bf365d82af4f5373fad10bad29353388c97e6643586f7b941e1c6a251c5aa2e`
- Artifact: `E:\T227U-ZS.zip`
- Artifact SHA-256: `bc248b870013a4baf6f41b0ca88acdf8effa63cab4d516e6696f8878cc8b1096`
- Ramdisk basis: byte-for-byte B-20261120-FPSGO-GOOD installer/ramdisk;
  therefore it still contains the old performance scripts for A/B isolation.
- Device result: **untested — booting at time of record**. Not releasable.

### B-20261120-VR-INSTALLER

- Timestamp: 2026-11-20 16:15 EST.
- Source state: `c498a526a` on `codex/t227u-f2fs-bringup`.
- Changed installer hunk: `anykernel/anykernel.sh:+13..51,+81,+95`.
  Before `write_boot`, it detects `/vendor/etc/fstab.ramplus`, retains a
  one-time `fstab.ramplus.t227u.bak`, comments only the active
  `/dev/block/zram0 ... zramsize=...,auto_configure` entry, verifies the
  marker, and remounts vendor read-only. An already-marked file is a no-write
  success path. Any failure aborts before boot write.
- Build: repack only; `Image.gz` and DTB were copied byte-for-byte from
  `E:\\T227U-ZS.zip`. No kernel objects or configuration were rebuilt.
- Installer preflight: `bash -n`; fixture proved the exact zram line changes,
  an unrelated vendor fstab line remains unchanged, and a marked entry remains
  single-commented. ZIP integrity test and installer/Image byte comparisons:
  **PASS**.
- CRC gate: inherited **PASS** from B-20261120-ZSFIX-BINDERPTR-AB (197 vendor
  exports verified, 0 mismatches); no linked kernel image changed.
- Artifact: `E:\\T227U-VR.zip`.
- SHA-256: `59f689a28162ed012c1ad87b42f6a525e32aa58016edeefb8d2d62e68457fcd7`.
- Kernel-image SHA-256: `7bf365d82af4f5373fad10bad29353388c97e6643586f7b941e1c6a251c5aa2e`.
- Device result: **untested**. In TWRP, confirm the installer reports either
  `Disabling vendor automatic RamPlus...` or `already disabled`; after boot,
  confirm the marker in vendor fstab and no automatic `/proc/swaps` entry.

### B-20261120-LZ4

- Timestamp: 2026-11-20 21:11 EST.
- Branch and source: `codex/t227u-f2fs-bringup`,
  `cbddad6a1644c9157d8ccd8408d4126a89b05fea`. This includes the preceding
  F2FS and vendor changes on the branch.
- Exact hunk record: run
  `git show --format= --unified=0 cbddad6a1 -- kernel/drivers/block/zram`.
  The new-side hunk starts and lengths (`start,length`) are:
  - `Makefile`: `2,1`.
  - `backend_lz4.c`: `1,36` and `backend_lz4.h`: `1,11`.
  - `zcomp.c`: `20,1; 41,2; 45,1; 50,1; 53,5; 59,10; 75,2; 78,1; 80,1;
    133,1; 136,1; 138,1; 160,4; 174,3; 187,1; 190,2; 195,0; 204,4; 213,1;
    215,1; 218,0`.
  - `zcomp.h`: `12,0; 13,0; 17,1; 23,1; 37,1`.
  - `zram_drv.c`: `2240,0; 2242,1; 2258,1; 2917,1; 3011,1; 3039,1;
    3054,1; 3068,1`. The file is byte-identical to the version before
    `dc5718e2a`.
- Functional trace: compressed ZRAM reads hold the slot bit spinlock
  and a zsmalloc mapping. The vendor writeback reader already holds a
  `kmap_atomic()` source mapping when it borrows a stream. Both require
  non-sleeping acquisition. The restored per-CPU borrow still covers
  compression, decompression and the compressed output buffer. CPU
  hotplug callbacks own workspace allocation and teardown. Direct LZ4
  uses the in-tree codec and the same block format as the Crypto API
  wrapper. The compressed decoder validates the resulting page length.
- Config: extracted from the kernel in the known-booting
  `E:\T227U-ZS.zip` and persisted locally as
  `/home/legit-su/t227u-zs-tested.config`. The linked image and output
  `.config` match that config byte-for-byte, including FPSGO V3.
  Config SHA-256:
  `ae63d8ec8ed72586802644719132ae95f303fbd3137518d9783012a50d39af99`.
  The generic `build_t227u.sh` loads the older r7 config, which disables
  FPSGO V3. Use the established output and the extracted config for this
  branch's A/B test.
- Build command from the source root:

  ```sh
  out=/home/legit-su/t227u-public-repro-build
  export PATH=/home/legit-su/kernel_src.oc.tar/toolchain_611/bin:$PATH
  cp /home/legit-su/t227u-zs-tested.config "$out/.config"
  make -C kernel O="$out" ARCH=arm64 LLVM=1 LLVM_IAS=1 \
    CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-gnu- \
    KCFLAGS=-w CONFIG_SECTION_MISMATCH_WARN_ONLY=y KSU_VERSION_OVERRIDE=33355 \
    KSU_VERSION_TAG_OVERRIDE=legacy-susfs-manual@465490a2 -j8
  ```

- Full link and module build passed. The linker reported 14 section
  mismatches and modpost reported three duplicate vendor exports.
  The linked kernel contains both direct LZ4 entry points. Local build
  log: `/home/legit-su/t227u-lz4-build.log`.
- Vendor CRC gate: **PASS**. All 197 mapped symbols were present. The
  patch corrected 28 mismatches and verification found zero remaining.
  Local gate log: `/home/legit-su/t227u-lz4-crc.log`.
- Kernel-image SHA-256:
  `40f9c17c298e9963cc67724ea452ce2adbd7d4e548b882d2b4798aebb65895ab`.
- Artifact: `E:\T227U-LZ4.zip`.
  SHA-256:
  `a3df3ff8ff722da36e8688469d1024aeddf51818c85f844220f49dbd966d1d0c`.
- Installer basis: `T227U-VR.zip`, with the same installer, DTB and single
  KernelSU Manager package `33457`. Removed the bundled
  `ramdisk/init.sunflower-performance.rc`,
  `ramdisk/sbin/sunflower-performance.sh` and
  `ramdisk/sbin/t225-perf-extra.sh` overlays. Installer syntax, ZIP integrity,
  embedded image identity, DTB identity and manager identity checks passed.
- Device result: **booted**. ADB reported `sys.boot_completed=1` and the
  expected build stamp `#19 SMP PREEMPT Fri Nov 20 21:10:56 EST 2026`.
  Wi-Fi associated successfully and `wlan_drv_gen4m` and
  `bt_drv_connac1x` were loaded. Bluetooth was off during the check.
  The initial read at Android boot completion preceded Kernel Adiutor's
  setup. After its apply completed, ADB verified 2000 MiB of active LZ4
  ZRAM at priority `-2`, with 34,048 KiB in use and no disk-backed swap.
  VM readback was `swappiness=100`, `vfs_cache_pressure=100`,
  `min_free_kbytes=6484`, `watermark_scale_factor=10` and `page-cluster=0`.
  The boot log search found no unknown symbols, version mismatches,
  sleep-in-atomic warnings or ZRAM errors. Bluetooth operation and an
  LZ4 ZRAM map-load A/B on the conservative profile remain to be tested.

### B-20261120-LZ4M

- Timestamp: 2026-11-20 21:32 EST. Source
  `2cd005a722f03aa28945ab78497cfde289f57475` on
  `codex/t227u-f2fs-bringup`.
- Exact preflight diff: `git show --format= --unified=0 2cd005a72`.
  It covers `backend_lz4.c/.h`, `zcomp.c/.h`, `lz4_compress.c`,
  `lz4_decompress.c`, `lz4defs.h`, `lz4hc_compress.c` and
  `tools/t227u/lz4_roundtrip.c`. Codec reference is Linux
  `5f22fb20497f4983094131f00a734362300d22d2`.
- Functional trace: device creation selects LZ4 or the Crypto API ops.
  CPU prepare allocates the private backend context and stream buffers,
  with cleanup on failure. CPU teardown frees the context through the same
  ops. The existing driver borrow and release paths still pin execution to
  one CPU. Compression and decompression callbacks allocate nothing.
  Plain LZ4 still uses the kernel block format. The codec uses compiler
  builtins for fixed-size copies and handles overlapping final literals.
- Build: the same source root, toolchain, output, config and `make`
  invocation as B-20261120-LZ4. The existing output config and image-embedded
  config both matched `t227u-zs-tested.config` byte-for-byte. Full link and
  module build passed with the recorded section-mismatch and duplicate
  vendor-export warnings. Build log `/home/legit-su/t227u-lz4-modern-build.log`.
- Verification: linked the prior and current ARM64 codec objects against
  the round-trip test, ran both binaries as the tablet ADB shell user and
  removed them. Both passed 1,024 full-page decodes, 1,024 partial decodes
  and 650 in-place decodes. Compressed output digest matched (`7e426a55`).
  This validates tested codec blocks. Backend lifecycle requires a kernel
  boot and live swap I/O check.
- Vendor CRC gate: **PASS**, 197 present, 28 corrected, zero remaining
  mismatches. Gate log `/home/legit-su/t227u-lz4-modern-crc.log`.
- Kernel image SHA-256:
  `aef2942fe7c206eab1c3f518f1fbdebc91774784f97f1665572baec64bef2bd0`.
- Artifact: `E:\T227U-LZ4M.zip`. SHA-256:
  `5a12f1be69558e980731106380275f4a6910b9c37d7161dea5f725cbb0c35108`.
  Installer, DTB and sole manager APK match `T227U-LZ4.zip`.
  ZIP integrity and embedded image comparison passed.
- Device result: **booted**. ADB confirmed build `#20`, Android boot
  completion, Wi-Fi association and loaded Wi-Fi/Bluetooth modules.
  After Kernel Adiutor's apply completed, ADB confirmed 2000 MiB LZ4
  ZRAM at priority `-2`, the conservative VM settings and zero
  ZRAM I/O errors. Swap-out was active. Sustained page-in and workload
  timing remain untested on this image.
- The boot log still shows unresolved vendor FPSGO symbols and CRC
  mismatches in FM radio and GPS modules. The same errors are present
  in the pre-LZ4 `E:\T227U-test-after-crash-dmesg.txt` capture. The
  vendor CRC gate covers the mapped symbols and the Wi-Fi/Bluetooth
  drivers loaded in this boot. No dictionary or stream-ownership change
  is included in this image.

### B-20261120-LZ4-READ

- Source `d0bd91a4c157580f508cdf4de0700e2c3ffba3ff` on
  `codex/t227u-f2fs-bringup`. Exact touched hunks:
  `git show --format= --unified=0 d0bd91a4c`.
- Normal ZRAM LZ4 reads dispatch directly to stateless decompression while
  retaining the slot lock and zsmalloc mapping. Non-spanning compressed
  vendor writeback reads follow the same dispatch. A read spanning pages
  uses a separate preallocated one-page per-CPU buffer. Huge span reads
  copy straight to the destination. The source mapping is released before
  mapping the destination on spanning reads, and the bounce buffer stays
  valid through error diagnostics. Stateful Crypto API backends retain
  the existing non-sleeping stream borrow. CPU hotplug still owns
  allocation, rollback and teardown. The codec and block format match #20.
- Build: same source root, toolchain, output and make flags as
  B-20261120-LZ4M. The output and embedded configs matched the tested
  T227U-ZS config. Full link succeeded. Build stamp:
  `#21 SMP PREEMPT Fri Nov 20 21:49:42 EST 2026`.
- Vendor CRC: **PASS**, 197 mapped symbols present, 28 corrected,
  zero remaining. Logs: `/home/legit-su/t227u-lz4-read-build.log`
  and `/home/legit-su/t227u-lz4-read-crc.log`.
- Local image: `/home/legit-su/t227u-public-repro-build/arch/arm64/boot/Image.gz`.
  SHA-256: `156b9c9996f2d8b86234a5c713802be66e9b94426f628e3d19ac434479584824`.
- Local test artifact: `E:\T227U-LZ4R.zip`, SHA-256
  `4f9d42101272884dfb18f1351d7461597810260de982a241b597661ef33b9588`.
  Reused the verified #20 installer, DTB and sole KernelSU manager APK.
  ZIP CRC, embedded kernel identity and unchanged payload checks passed.
- Device result: **booted**. ADB confirmed build #21, Android boot
  completion, Wi-Fi association and loaded Wi-Fi/Bluetooth modules.
  After Kernel Adiutor applied, ADB verified 2000 MiB of active LZ4
  ZRAM at priority `-2`. The initial readback showed three swap-ins,
  201 swap-outs, zero ZRAM I/O errors and zero kernel OOM kills.
  Sustained page-in remains untested. The archive is local. The active
  swap has no disk backing, so it did not exercise the new writeback
  bounce buffer.

### B-20261120-LZ4-DICT

- Source: `e952c890224f81755de77f6f9db13f7e6ab43e90` on
  `codex/t227u-f2fs-bringup`. Exact hunks:
  `git show --format= --unified=0 e952c8902`.
- Added opt-in, per-device raw dictionary upload via
  `comp_dictionary_size` and `comp_dictionary`. Writes are serialized
  under the initialization lock, offsets must be sequential and disk
  initialization requires a complete upload. The dictionary buffer
  transfers to the compressor only after successful setup and CPU stream
  creation. Reset clears staged data, then frees the live parameters
  after stream teardown. LZ4 copies the prepared template into the
  compression workspace and decodes using immutable dictionary bytes.
  The default path starts with an empty dictionary.
- ARM64 codec check on the tablet passed 1,024 plain round trips and
  192 dictionary page round trips at 16 KiB and 64 KiB. The template
  and per-page dictionary reload produced identical blocks. The tablet
  was running #21 for this codec-object test. Live #22 sysfs and
  block-device round trips remain to be verified.
- Build: same source, toolchain, output and flags as B-20261120-LZ4-READ.
  Full link passed and embedded config matched the tested T227U-ZS
  config. Build stamp: `#22 SMP PREEMPT Fri Nov 20 22:05:08 EST 2026`.
  Build log: `/home/legit-su/t227u-lz4-dict-build.log`.
- CRC gate: **PASS**, 197 mapped symbols present, 28 corrected, zero
  remaining. Gate log: `/home/legit-su/t227u-lz4-dict-crc.log`.
- Kernel image SHA-256:
  `9d1533c9e02726e34f7c5f6ab41cf365011277eaea0e60e2fbb208c63543fa4a`.
- Local test ZIP: `E:\T227U-LZ4D.zip`. SHA-256:
  `f6f36c9be674c8d1c41dd7380f81ac49d983689e73253bd9c2debec3e73f341e`.
  Installer, DTB, single KernelSU manager APK, embedded image identity,
  and ZIP integrity checked. No dictionary data is bundled.
- Device result: **booted**. Android completed boot on #22 and Wi-Fi
  associated. Initial automatic and manual KA setup both hit
  `EINVAL` on the reset attribute because Android's `printf` issued
  the trailing newline as a separate sysfs write. Verified on-device
  that a value-only write succeeds. The corrected transaction run as
  KA's app UID loaded 16,384 bytes, initialized 2040 MiB LZ4 ZRAM,
  and activated swap at priority `-2`. ZRAM errors stayed at zero
  through 241 swap-outs. After a second reboot with the corrected APK,
  KA's ApplyOnBoot log confirmed automatic activation with
  `disk=2139095040 bytes, dictionary=16384 bytes`. ADB readback matched:
  2040 MiB of swap at priority `-2`, three swap-ins, 243 swap-outs,
  zero ZRAM I/O errors and zero kernel OOM kills. Sustained dictionary
  page-in and workload performance are pending.
- Fixed companion KA test build: `E:\KA-LZ4D.apk` from
  `senposage/KernelAdiutor` commit `7932d0da50034f2a8ec195794e2dd8368eb3137c`.
  SHA-256:
  `261e80fbd0e6aa06566e92e795502d279efb8dacebc6356bc3ab2717e6e390bb`.
  The fixture passed dictionary staging, missing or failed upload fallback,
  preservation of active swap during boot, and safe abort on swapoff
  failure. ADB verified the installed replacement APK. The root
  transaction was verified separately on #22.
- Test dictionary: reproduced the original 16 KiB Asphalt candidate
  from the same local capture and training split. Held-out compressed
  totals matched the earlier screen. The private file was staged and
  verified on the tablet at
  `/sdcard/Download/KernelAdiutor/zram-lz4.dict`.
  SHA-256: `500c6218e50dce47279425b2067d0fb06a50df3725ad1c1e262cc9666fadb2af`.
  Automatic boot upload passed. Workload performance remains to be tested.

## Device-side vendor change

Performed in TWRP recovery on 2026-11-20, independently of any boot ZIP:

- Backup: `/vendor/etc/fstab.ramplus.t227u.bak`
- Changed only `/vendor/etc/fstab.ramplus`: commented the
  `/dev/block/zram0 ... zramsize=2147483648,auto_configure` line.
- Verification: vendor remounted read-only; subsequent kernel log reported
  `ZramDevice:0kB` and `Total swap = 0kB`.

This vendor change prevents automatic 2 GiB RamPlus activation. It does not
add an initramfs hook and leaves manual ZRAM configuration available.

## Historical source-only record

### Z-20261120-STREAM-OWNERSHIP

- Timestamp: 2026-11-20 EST.
- Commit: `dc5718e2a21f7c464eb1320737d435917dc4e4d8` on
  `codex/t227u-f2fs-bringup`.
- Disposition: **superseded** by `cbddad6a1`. The original trace missed
  the already-held slot bit spinlock and source mapping. A later run
  linked this mutex version using the older r7 config. The linked build
  was discarded and the tested config was recovered from the ZS image.
- Purpose: first, isolated modern-ZRAM dependency slice. It changes stream
  ownership and CPU-hotplug lifetime only; it deliberately retains the 4.19
  Crypto API backend and introduces no dictionary ABI, compressor change,
  sysfs node, initramfs hook, or vendor-file change.
- Functional trace: `zram_handle_comp_page()`, `__zram_bvec_read()`, and
  `__zram_bvec_write()` are the only callers of `zcomp_stream_get()`. Each
  now acquires the mutex before entering `kmap_atomic()`; CPU-offline takes
  the same mutex before freeing buffers, while a migration race retries if
  the selected stream has been torn down.
- Compile preflight: forced build of
  `drivers/block/zram/zcomp.o` and `drivers/block/zram/zram_drv.o` passed
  under Android Clang r383902 / 11.0.1 using the preserved T227U
  configuration and existing `out-t227u` output directory.
- Full-link status: **not completed**. A duplicate full build was detected
  before link completion and both build processes were stopped to protect the
  shared output directory. Therefore `vmlinux`, `Image.gz`, the vendor CRC
  gate, a ZIP, and a device test are all **not produced / not claimed**.

## 2026-11-21 MT6768 candidate compile checks

- Branch `codex/t227u-f2fs-bringup`, based on the r8 release source and
  later documentation. Commits `46aa41a65`, `3725c88a1`, `213d65daf`,
  `3a399c2b1`, `eddb9b267`, and `5cfbd2725` record the exact patches.
- Used the established source, toolchain, make flags, and
  `/home/legit-su/t227u-public-repro-build` output directory. The
  configuration SHA-256 matched before and after both object builds
  (`ae63d8ec8ed72586802644719132ae95f303fbd3137518d9783012a50d39af99`).
- Compiled `lib/scatterlist.o` and
  `drivers/media/common/videobuf2/videobuf2-core.o` first. The video
  object references `__wake_up_sync`. A second object build passed for
  `drivers/cpufreq/cpufreq.o`, `mm/cma.o`, `drivers/mmc/core/bus.o`, and
  `kernel/time/alarmtimer.o`.
- Full link, vendor CRC verification, boot, storage/SD, governor/hotplug,
  and camera/video tests remain pending for this candidate batch.

## 2026-11-21 MT6768 candidate full builds

- Both builds used the existing output directory and the common T227U
  toolchain and flags. The r8 config SHA-256 was preserved
  (`ae63d8ec8ed72586802644719132ae95f303fbd3137518d9783012a50d39af99`).
  Both linked and generated `Image.gz`, and both reported the existing
  14 section mismatches and three duplicate vendor module exports.
- Build #23 includes the previous six candidates and the fence ioctl
  patch later committed as `c3c875221`. A host harness compared the
  previous and new ioctl output bytes for count-only, single-fence,
  multi-fence, and error cases. It also verified that single-fence
  detail queries bypass allocation. Image SHA-256
  `a45cc70f77ea0fef26fc5679c4ce6d3d955c75e87da09770b61a0a73b92e9695`.
  Logs `/home/legit-su/t227u-20261121-candidate-full-build.log` and
  `/home/legit-su/t227u-20261121-candidate-crc.log`.
- Build #24 adds the swapoff lock fix `3b8275910`. The raw vendor CRC
  differences matched released build #22, and the applied gate verified
  all 197 mapped exports with zero remaining mismatches. Image SHA-256
  `27edee2f3687f4cb7bcee1c3d441297d6982625fa92404d7b44c05d8bdd0ce54`.
  Logs `/home/legit-su/t227u-20261121-swapoff-build.log` and
  `/home/legit-su/t227u-20261121-swapoff-crc.log`.
- The candidate image is local. Boot, radio, video, and live ZRAM
  resize on this build remain to be tested. The tablet baseline still
  runs #22, with F2FS `/data`, `mq-deadline` selected on `mmcblk0`,
  and the cgroup-v1 memory controller at `/dev/memcg`.

## 2026-11-21 ZRAM page-in candidate

- Build #25 adds `d5a666c06` on `codex/t227u-f2fs-bringup`. Both
  successful page-in paths now leave the compressed backing in place.
  Verified the ordinary slot-release callback remains in `swapfile.c`.
- Incremental build and full link passed in the existing output directory
  with the r8 configuration and vendor ABI string. The raw CRC differences
  matched #22 and the applied gate verified all 197 vendor exports.
- Image SHA-256
  `ca8bb57a935feb0aa005f1a64918021c1e5d6d00179ef5a80912176280f065d7`.
  Logs `/home/legit-su/t227u-20261121-pagein-build.log` and
  `/home/legit-su/t227u-20261121-pagein-crc.log`.
- Local flashable archive `E:\T227U-25.zip`, SHA-256
  `967a2abecb12168a90ca8263a23d5016b610de9357dcb799f69111bc8b95998e`.
  The package contains the exact CRC-verified #25 image. The r8
  AnyKernel installer, DTB, recovery tools, and single KernelSU Next
  Manager APK matched byte for byte. ZIP integrity passed.
- The tablet booted #25, obtained a Wi-Fi IPv4 address, and activated
  2040 MiB of dictionary-backed ZRAM. ADB recorded swap-ins and
  swap-outs with zero ZRAM I/O errors. See the runtime log for the
  remaining FPSGO/XGF work.

## 2026-11-21 XGF test build

- Build ID `B-20261121-XGF-26`, packaged at 02:18 EST.
  Branch `codex/t227u-f2fs-bringup`. Built from `9b50239d1` with the
  source edits committed verbatim as `8be986c48e4ecb6e86df07d2928d9b4adf927d8b`.
  Boot release `4.19.191-28847731-abT227USQSCEYE2`, build #26.
- Exact source hunks since build #25, relative to
  `kernel/drivers/misc/mediatek/performance/fpsgo_v3/`:
  - `composer/include/fps_composer.h`: `-9,0 +10,2`, `-37 +39,2`
  - `composer/src/fps_composer.c`: `-268 +268`, `-285 +285`, `-330,0 +331,2`, `-345,3 +347,2`, `-444 +445`, `-522 +523,2`
  - `fbt/include/xgf.h`: `-103,0 +104,5`, `-265,0 +271`, `-268 +274`
  - `fbt/src/xgf.c`: `-14,0 +15`, `-28,0 +30,7`, `-822,0 +831`, `-841,3 +850,2`, `-844,0 +853`, `-856 +865`, `-1291,0 +1301,11`, `-1294,2 +1314,2`, `-1299,9 +1319,2`, `-1375 +1388`, `-1379 +1391,0`, `-1381 +1392,0`, `-1405,0 +1417,11`, `-1523,2 +1545`, `-1526 +1547,2`, `-1528 +1550,2`, `-1530,4 +1553,7`, `-1535 +1561,28`, `-1611 +1664`, `-1617,0 +1671`, `-1648,0 +1703`, `-1668 +1723,2`, `-1673,8 +1729,3`
  - `fpsgo_main.c`: `-54,0 +55`, `-153 +154,2`, `-172 +174`, `-246 +248,2`, `-300,0 +304,3`

- The MT6765 queue-end notifier snapshots scheduler runtime before
  queueing work. Composer and XGF operate under their existing mutexes.
  The sample carries a start timestamp to detect PID recycling. It has
  no task pointer. XGF reuses its EMA and outer dependency list for the
  render thread, and FBT consumes the result using its existing policy.
  The host test executed the estimator for initialization, bounded and
  zero deltas, buffer isolation, task reuse, idle, counter rollback, and
  vendor callback precedence. Patch style and full link passed.
- Existing r8 output and configuration retained. The .config SHA-256
  was `ae63d8ec8ed72586802644719132ae95f303fbd3137518d9783012a50d39af99`.
  Build command from the source root:

  ```sh
  export PATH=/home/legit-su/kernel_src.oc.tar/toolchain_611/bin:$PATH
  out=/home/legit-su/t227u-public-repro-build
  make -C kernel O="$out" ARCH=arm64 LLVM=1 LLVM_IAS=1 \
    CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-gnu- \
    KCFLAGS=-w CONFIG_SECTION_MISMATCH_WARN_ONLY=y KSU_VERSION_OVERRIDE=33355 \
    KSU_VERSION_TAG_OVERRIDE=legacy-susfs-manual@465490a2 -j8
  ```

- Applied vendor CRC gate passed all 197 mapped exports with zero
  remaining mismatches. The all-module audit in `mt6768-donor-audit.md`
  records ten additional CRC mismatches outside the gate.
  Logs `/home/legit-su/t227u-20261121-xgf-build.log` and
  `/home/legit-su/t227u-20261121-xgf-crc.log`.
- Patched `Image.gz` SHA-256
  `13dc82d003410438020ea47be40a662d529b57b3d577283d3393686ad6cf337c`.
  Local test ZIP `E:\T227U-XGF.zip`
  SHA-256 `2a925a1ec81cb6950fa26de73b6ca9546273a26d88daf8e170044c7dd31f5f1c`.
  The #25 installer payloads, DTB, recovery tools and single KernelSU
  Manager APK matched byte-for-byte. ZIP integrity and embedded kernel
  identity passed.
- Device result **booted**. ADB confirmed #26, completed boot, and
  `fpsgo_enable=1`. A smoke process submitted 12 queue events through
  the normal `/proc/perfmgr/perf_ioctl` frame path. The first frame
  established a baseline. Eleven frames measured 3.069–3.203 ms of
  CPU time and FBT produced ten positive performance indices. Sampled
  CPU time stayed below the corresponding frame interval.
- During a separate ten-second Asphalt capture, the game's SurfaceFlinger
  buffer timestamps advanced while FPSGO recorded zero XGF runtime
  samples and zero FBT performance indices. This verifies the native
  kernel path with an explicit producer. Automatic frame delivery from
  the GSI remains unverified and was absent in the observed workload.
  Traces are at `/home/legit-su/t227u-20261121-xgf-device-smoke.log` and
  `/home/legit-su/t227u-20261121-xgf-asphalt.log`. Both private trace
  instances and the temporary device test binary were removed.
- A separate ABI check on a disposable copy of #26 passed all 203 mapped
  CRCs after adding the reviewed FM and GPS entries. The existing
  `E:\T227U-XGF.zip` was unchanged. This review produced no flashable
  artifact. Gate log `/home/legit-su/t227u-20261121-module-abi-crc.log`.

## 2026-11-21 GED to FPSGO test build

- Build ID `B-20261121-GED-27`, packaged at 02:40 EST. Branch
  `codex/t227u-f2fs-bringup`. Built from `38f30869e` with source edits
  committed verbatim as `29e07a2f1caf0c8e08ad45a167c8f1d913f5b55a`.
  Boot release `4.19.191-28847731-abT227USQSCEYE2`, build #27.
- Exact kernel hunks since build #26:
  - `kernel/drivers/gpu/mediatek/ged/src/ged_bridge.c`: `-146,0 +147,7`
  - `kernel/drivers/misc/mediatek/include/mt-plat/fpsgo_common.h`: `-12,0 +13,4`
  - `kernel/drivers/misc/mediatek/performance/fpsgo_v3/base/src/fpsgo_base.c`: `-531 +531`
  - `kernel/drivers/misc/mediatek/performance/fpsgo_v3/composer/include/fps_composer.h`: `-40,0 +41,2`
  - `kernel/drivers/misc/mediatek/performance/fpsgo_v3/composer/src/fps_composer.c`: `-282,0 +283,38`, `-288 +325,0`, `-290,2 +326,0`, `-331,34 +366`, `-375,0 +378,35`
  - `kernel/drivers/misc/mediatek/performance/fpsgo_v3/fbt/src/fbt_cpu.c`: `-633,0 +634,2`, `-640,4 +642,4`, `-646 +648`, `-1754 +1756,2`
  - `kernel/drivers/misc/mediatek/performance/fpsgo_v3/fpsgo_main.c`: `-8,0 +9`, `-41,0 +43`, `-48,0 +51`, `-82,0 +86,3`, `-267,0 +274,6`, `-276,0 +289,29`

- While Asphalt reported slow loading on #26, a five-second ADB sample
  showed flat eMMC request counters, near-zero memory and I/O pressure,
  and zero ZRAM failed reads and writes. About 529 MiB of memory was
  available and 1.67 GiB of swap was free in the first snapshot. The
  app was still alive and its most active worker threads were mostly
  sleeping on futexes. The snapshot did not establish the load cause.
- On #27, a successful GED queue from the selected process reaches
  XGF and FBT through the existing FPSGO workqueue. The ioctl caller
  supplies its own task sample before queuing work. The worker confirms
  the target is still selected. Compositor calls, invalid window IDs,
  and known different owners do not enter this path. GED has completion
  timestamps only, so its frames skip enqueue-length reports and the
  rescue timer that depends on enqueue-start timing.
- The selected process control is
  `/sys/module/fpsgo_main/parameters/ged_tgid`, verified in the linked
  object. Root can set the current game process ID, and zero is the
  default and disables forwarding. Worker activity and render expiry
  remain bounded by the existing FPSGO workqueue and idle handling.
- Host checks covered opt-in, caller and owner scope, invalid window,
  allocation failure, disabling queued work, monotonic completion,
  and legacy enqueue timing. Full link passed. The build reused the
  #26 command, r8 config, and established output directory.
  Config SHA-256
  `ae63d8ec8ed72586802644719132ae95f303fbd3137518d9783012a50d39af99`.
- Applied CRC gate verified all 203 mapped exports, including the
  reviewed GPS and FM updates, with zero unexpected CRCs after patch.
  A second import scan confirmed zero built-in CRC mismatches for
  Wi-Fi, Bluetooth, FM, and GPS. Logs
  `/home/legit-su/t227u-20261121-ged-fpsgo-build.log` and
  `/home/legit-su/t227u-20261121-ged-fpsgo-crc.log`.
- `Image.gz` SHA-256
  `3e082a36b1418a207707a5afa4457a328d713198ef2ecb1a29c8a4eda849ee13`.
  Local test archive `E:\T227U-GED.zip` SHA-256
  `d88d1742a77a24cca4b6ab568741ff775ead02c57e2c592e695a5893806f6954`.
  Package integrity and embedded image identity verified. The #26
  installer, DTB, recovery tools, and single manager APK match byte for
  byte.
- Device result **booted**. ADB confirmed #27, completed boot, and
  `ged_tgid=0`. Wi-Fi got IPv4. The Bluetooth, GPS, and FM modules
  loaded. GPS and FM created `/dev/stpgps` and `/dev/fm`, and the GNSS
  services were running. Reception and Bluetooth pairing were outside
  this check.
- With the bridge off, 197 GED buffers produced zero FPSGO frames.
  With Asphalt selected, 53 of 53 GED buffers reached the FPSGO
  composer. XGF returned 51 positive CPU samples, and FBT emitted 50
  positive indices. The game process stayed the same during the
  capture. The repeat off/on/off/on/off trace reproduced forwarding
  only while selected. CPU governor minimums and maximums matched in
  all five samples.
- Frame cadence varied between capture windows. The test verifies
  routing and CPU policy readback. A matched race or load remains
  needed to measure the performance effect. Both private trace
  instances were removed and the bridge was restored to zero.
  Logs `/home/legit-su/t227u-20261121-ged-device-ab.log` and
  `/home/legit-su/t227u-20261121-ged-device-repeat.log`.

## 2026-11-21 Asphalt server-sync storage check

- The device was still running #26. Its running config enables
  `CONFIG_MMC_MTK_PRO`, and the eMMC host uses MediaTek ComboA.
  The donor DMA-map change in `54d4c1aed` modifies the generic
  `mtk-sd` driver, which the running config excludes. The active MMC
  core change in `213d65daf` releases a device-tree reference during
  card removal or failed registration.
- F2FS `/data` resolves to `mmcblk0p52`. The `mmcblk0` counters were
  identical over the four-second sync-stall sample, with zero queued
  requests and 0.00% ten-second I/O pressure. ZRAM reported zero failed
  reads and writes. The available kernel ring covered roughly three
  minutes and contained no matching MMC resets, timeouts, tuning
  failures, or block/F2FS I/O errors.
- Wi-Fi was validated. Asphalt's Firebase telemetry connection failed
  and its hostname resolved to loopback on the tablet. The app log
  did not identify a failed request to the game's synchronization
  backend. The active eMMC scheduler was Kyber during this capture.

## 2026-11-21 automatic GED to FPSGO test build

- Build ID `B-20261121-GED-AUTO-28`, packaged at 03:00 EST. Branch
  `codex/t227u-f2fs-bringup`. Built from `82fb9a4f8` with source edits
  committed verbatim as `1a4908726bc638c1ab3438dce64e0a48502ced4d`.
  Boot release `4.19.191-28847731-abT227USQSCEYE2`, build #28.
- Exact kernel hunks since #27 in `kernel/drivers/misc/mediatek/performance/fpsgo_v3/fpsgo_main.c`: `-9,0 +10`, `-31,0 +33`, `-86 +88`, `-88 +90`, `-232,0 +235,36`, `-275,2 +313`, `-293 +330,6`, `-295,2 +337,4`

- The Android GSI places Asphalt and Launcher in `/top-app`. Automatic
  GED selection matches the process leader's cpuset at the ioctl and
  validates that it is still selected before the worker uses a sample.
  The worker also checks task start time against the sample so a recycled
  TID cannot inherit queued work. The existing GPU policy is kept.
- `ged_tgid=-1` now ships as the default and follows the top app.
  A positive TGID selects a single process, and zero disables forwarding.
  On the running #27 tablet, Asphalt was selected again after the
  earlier capture. FBT reported a positive index and the bridge was
  left active at the user's request.
- Host checks covered top-app selection, demotion, exact and truncated
  paths, overrides, thread reuse, changed process ownership, and exit.
  The linked default was verified as `-1`. The full build reused the
  #26 command, the established output directory, and the r8 config
  SHA-256 `ae63d8ec8ed72586802644719132ae95f303fbd3137518d9783012a50d39af99`.
- Applied CRC gate verified all 203 mapped exports. The independent
  import scan found zero built-in CRC mismatches for Wi-Fi, Bluetooth,
  FM, and GPS. Logs `/home/legit-su/t227u-20261121-ged-auto-build.log`
  and `/home/legit-su/t227u-20261121-ged-auto-crc.log`.
- `Image.gz` SHA-256
  `c60643204781aa1d0280abe9f22725d989b631888687b274f6c4cb5453d7b9e5`.
  Local test archive `E:\T227U-AUTO.zip` SHA-256
  `4afbb57777fdc62035512105636afe5f60a19b0229b610042b0cbc36bdd4eb49`.
  Archive membership, integrity and embedded image identity passed.
  All installer payloads and the single manager APK match #27.
- Device result **booted**. ADB confirmed #28 and read `ged_tgid=-1`
  from its default control. Foreground-app switching and Asphalt
  performance in automatic mode still need a matched test.

## 2026-11-21 MT6631 FM app compatibility

- Build #28 loads `fmradio_drv_mt6631`. The Android 14 GSI exposes
  `/dev/fm`, the vendor has MT6631 firmware and configuration, and
  the audio policy includes FM tuner ports. A temporary probe using
  the MediaTek FM chip-ID ioctl returned `0x6631`. The probe was removed.
  Station reception and audio playback remain untested.
- [RevampedFMRadio's MediaTek branch](https://github.com/iusmac/RevampedFMRadio/tree/mtk)
  uses `/dev/fm` via `libmtkfmjni`.
  [TechyMinati's FMRadio source](https://github.com/techyminati/android_packages_apps_FMRadio)
  explicitly supports MT6631. Both use the system shared UID and
  platform signing. Neither upstream publishes an APK release.
  The current GSI's framework APK signer differs from the AOSP public
  platform test certificate. A direct port would need integration and
  signing with this GSI's platform key, or a separate app design with
  an explicitly authorized FM backend.

## 2026-11-21 RevampedFMRadio build

- Built the unmodified upstream MediaTek source at
  `02a4f9991434e60931c539e67a0b5e24cf0a2c35` in
  `/home/legit-su/t227u-revamped-fmradio`. The work directory is
  `/home/legit-su/t227u-fm-build`.
- The main APK retains `com.android.fmradio`, version `3.1.3-mtk`,
  and the system shared UID. It includes ARM64 `libmtkfmjni.so` with
  its JNI entry point. The separate recordings APK and upstream
  privileged-permission XML are in the same archive.
- Java and resources used Android 14 framework inputs. JNI used NDK
  r27c, a local compatibility include for the legacy Android logging
  header, and the GSI's `libcutils` as a link input. Packaging used
  SDK 35 tools, CardView 1.0.0, and AndroidX Core 1.12.0.
- Local archive `E:\T227U-FMRadio.zip`, SHA-256
  `244caf9dd54ecf535326cf3638ed0be09cc1ef61b0497b373de7fbd86edba1c4`.
  Verified both manifests compile at API 34, APK alignment and
  integrity, ARM64 JNI identity, and ZIP integrity.
- Both APKs are unsigned ROM integration artifacts. The current
  GSI uses its own platform signer, so install and FM playback
  testing still require suitable signing and integration.

- On request, a separate signed APK variant with its own application UID
  and its signed recordings companion were sideloaded. KernelSU Next
  granted root to `com.android.fmradio` using the stock app-profile CLI.
  Launching reached the FM activity and then crashed during
  `FmService` construction. Android's audio service denied
  `MODIFY_AUDIO_ROUTING` in `getMediaAudioProductStrategy()`.
  The two data apps remain installed and the FM app's root profile remains
  granted. The platform integration ZIP and the system partition were
  unchanged.

- The systemless version-code 8 variant retains the signer and
  application UID. KernelSU module `t227u_fmradio_priv` mounts the
  privileged APK and adds the FM permission allowlist. Android granted
  audio routing and tuner capture after reboot.
- The app's initial `/dev/fm` SELinux denial was resolved by assigning
  the node to the app's verified group and allowing only the needed
  tuner ioctls. SELinux remains enforcing. The app opened the tuner,
  scanned, and tuned successfully. Android reported PLAYING, and the
  app was left running.
- The final late service re-applied the device group and policy in a
  manual test. A subsequent reboot with this final service remains
  untested. Local module archive
  `/home/legit-su/t227u-fm-build/root-install/fm-systemless.zip`,
  SHA-256 `b9538d0230fd030037c7287b325eafd01e37f66070334494912f507f1bd03121`.

### FM reception disposition

**Unsupported on SM-T227U hardware.** On build #28, the MT6631 reported
chip ID 0x6631, powered up, and accepted a tune to 98.9 MHz. On the
successful reinsertion, MT6357 ACCDET identified a 4-pole headset.
Android registered the wired route and delivered the plug event to
RevampedFMRadio. The tuner returned RSSI -114 for three consecutive
reads with no stereo lock, and the user heard static.

The headset detection result confirms the jack switch and codec
classification. The RF connection from the jack to the tuner remains
unverified. Samsung's T227U board schematic and BOM are confidential.
FM reception is outside the supported feature list for this device.

## 2026-11-21 donor audit build #29

- Build ID `B-20261121-DONOR-29`. Branch
  `codex/t227u-kernel-bringup`. Compiled from `532b5a473` with
  the changes committed verbatim as `b1e9f9ae7`, `01f40c929`,
  and `c21e15097`. Kernel release
  `4.19.191-28847731-abT227USQSCEYE2`, build #29.
- Exact source changes are in the three commits. Files touched
  `kernel/drivers/opp/of.c`,
  `kernel/drivers/power/supply/power_supply_sysfs.c`,
  and `kernel/net/sched/sch_fq_codel.c`.
- Reused the #26 command, Clang 11.0.1, and
  `/home/legit-su/t227u-public-repro-build`. Config SHA-256 before
  and after was
  `ae63d8ec8ed72586802644719132ae95f303fbd3137518d9783012a50d39af99`.
- Checkpatch reported zero errors or warnings. An extracted host
  harness passed uevent key/value checks for all 292 property names,
  full key-table and buffer handling, 64-bit OPP scaling, and FQ-CoDel
  default MTU bounds.
- Full link and module build passed with the existing 14 section
  mismatches and three duplicate vendor exports. The CRC gate verified
  all 203 mapped exports, corrected 28 values, and verified zero
  remaining mismatches. Logs
  `/home/legit-su/t227u-20261121-donor-resume-build.log` and
  `/home/legit-su/t227u-20261121-donor-resume-crc.log`.
- CRC-patched `Image.gz` SHA-256
  `1fe53ef121ad8e66f0730ae66aaa2bbfe0013488a21904eefec5e69662de2025`.
  Build verification is host-only. The tablet remains on #28.

## 2026-11-21 eMMC and reclaim test build #30

- Build ID `B-20261121-IO-30`, branch
  `codex/t227u-kernel-bringup`. Built from `10fa0ca30` with changes
  committed verbatim as `65f7736ae` (`kernel/block/blk-stat.c`),
  `bd68797df` (`kernel/mm/vmscan.c`), and `a26c71901`
  (`kernel/drivers/mmc/host/mediatek/ComboA/dbg.c`).
  Release `4.19.191-28847731-abT227USQSCEYE2`, build #30.
- Reused the #26 command, Clang 11.0.1, and
  `/home/legit-su/t227u-public-repro-build`. The before and after
  config SHA-256 matched
  `ae63d8ec8ed72586802644719132ae95f303fbd3137518d9783012a50d39af99`.
- Checkpatch passed. An extracted host harness covered the logging
  default, write validation, disable before the timestamp and ring
  writes, enable during a command, ring wrap, and an interleaved
  completion. The link contains the `msdc.cmd_history` parameter.
- Full link passed with the existing 14 section mismatches and
  three duplicate vendor exports. The applied gate verified all 203
  vendor CRCs, patched 28 mismatches, and verified zero remaining.
  Logs `/home/legit-su/t227u-20261121-io-build.log` and
  `/home/legit-su/t227u-20261121-io-crc.log`.
- CRC-patched `Image.gz` SHA-256
  `19097118fe0080b3a253ffc3eb7ffcc0c94e5c55818511369df7ebd3189df8a5`.
  Local test ZIP `E:\\T227U-IO.zip` SHA-256
  `265a72e7cbd10db27375fb7e97048cf64b09a7e467f6cf16a9abdd70d1c86f73`.
  ZIP integrity and embedded-image identity passed. All other entries,
  including the installer, DTB, recovery tools, and single KernelSU
  Manager 33457 APK, match the booted #28 ZIP byte for byte. Device
  boot, radio, and latency testing for #30 remain pending.

### ComboA command history default after build #30

- Changed `kernel/drivers/mmc/host/mediatek/ComboA/dbg.c` so the
  zero-initialized capture generation starts with command logging
  disabled. Root can write `1` to
  `/sys/module/msdc/parameters/cmd_history` to enable capture.
- Recompiled the ComboA `dbg.o` target with Clang 11.0.1 in the
  established output directory. Verified the generation is in
  zero-filled BSS and the module parameter is present. The config hash
  matched build #30. Log
  `/home/legit-su/t227u-20261121-msdc-default-off-object.log`.
  Object verification only. The running #30 tablet already has
  capture disabled through its runtime setting.

## 2026-11-21 r9 build #31

- Build ID `B-20261121-R9-31`. Built kernel source
  `35f338bf4f6d0b8af065aab1407f2c0e65261d53` on
  `codex/t227u-kernel-bringup`. Release
  `4.19.191-28847731-abT227USQSCEYE2`, build #31.
- Reused the #26 command, Clang 11.0.1, and the established
  `/home/legit-su/t227u-public-repro-build` output directory.
  The config hash remained
  `ae63d8ec8ed72586802644719132ae95f303fbd3137518d9783012a50d39af99`.
  The exact build input is tracked as `configs/t227u-r9.config` and
  selected by the public build script.
- Full link passed with the existing 14 section mismatch and three
  duplicate vendor export warnings. The linked history generation is
  zero-filled and the runtime command history control remains present.
  The applied CRC gate verified all 203 mapped exports after applying
  28 overrides. An independent import comparison matched all built-in
  imports for seven vendor Wi-Fi, BT, connectivity, GPS, and FM modules.
  Logs `/home/legit-su/t227u-20261121-r9-build.log`,
  `/home/legit-su/t227u-20261121-r9-crc.log`, and
  `/home/legit-su/t227u-20261121-r9-imports.log`.
- Patched image SHA-256
  `c5f5c189daa3ffffe1ecf11f048df33c352b3ccba56540a53c38ab61c7263ff8`.
  Local release candidate `E:\\T227U-R9.zip` SHA-256
  `946128ef597ea2a1e7f0d3cbd5645ca7705d4e20054e25e9f291cd0291d13ad8`.
  The ZIP replaced the kernel and installer display string from #30.
  The recovery payloads, DTB, and single Manager 33457 APK were
  matched byte-for-byte. The standalone Manager and Kernel Adiutor
  match the hashes of the corresponding published assets. The
  trainer script was copied from the tagged source and its help
  command completed.

## GPU OC test builds #32 and #33

- Build #32 compiled the first 775, 800, and 825 MHz table revision
  on base ae95d6ebb. Link and the 203-entry CRC gate passed. The
  cap condition in that revision would have disabled explicit
  lower caps on other GPU segments. Build #33 superseded it before
  packaging. The #32-only change was the use of
  g_segment_id == MT6765_SEGMENT alone in the target cap condition.
  Logs /home/legit-su/t227u-20261121-gpu825-build.log and
  /home/legit-su/t227u-20261121-gpu825-crc.log.
- Build #33 used source 83b5e4e1df5d87de0fdc829bd51026eb48786da0
  on codex/t227u-kernel-bringup. The complete diff covers
  kernel/drivers/gpu/mediatek/gpufreq/mt6765/mtk_gpufreq_core.c and
  mtk_gpufreq_core.h. The added bins request nominal 825 mV through
  the existing VCORE QoS mapping. Segment 3 starts at its 680 MHz
  stock OPP and returns to 680 MHz when resetting the OC ceiling.
- Both builds used the #26 command, Clang 11.0.1, the established
  /home/legit-su/t227u-public-repro-build output directory, and
  config SHA-256
  ae63d8ec8ed72586802644719132ae95f303fbd3137518d9783012a50d39af99.
  Kernel release 4.19.191-28847731-abT227USQSCEYE2.
- Checkpatch reported zero errors and warnings. A host harness
  compiled the driver policy functions and checked startup at 680,
  all three cap writes, applying a lower cap while active, thermal
  clamp behavior, voltage bounds and reset, and lower caps on other
  segments. Verified the nine linked Segment-3 OPP rows and CRC
  imports for the seven Wi-Fi, BT, connectivity, GPS and FM modules.
- Build #33 linked with the baseline 14 section mismatches and three
  duplicate vendor export warnings. The CRC gate applied the 28
  required overrides and verified all 203 mapped exports with zero
  remaining mismatches. Logs under /home/legit-su use prefix
  t227u-20261121-gpu825 with suffixes -final-build.log,
  -final-crc.log, -policy.log, -linked.log, and -package.log.
- CRC-patched Image.gz SHA-256
  8a1b9c37ac9edcdd8cb4e76fdcffdf9b656c2f7ebf7d4748e152de57396b1661.
  The local test ZIP E:\T227U-GPU.zip SHA-256
  c9d1cccdff3f1604d8ac9ce8d9daa501db633d6c1d14795011dfa4b52fcfc1da
  passed ZIP integrity checks and embedded image comparison. The
  only payload changes from the r9 candidate are the image and
  installer display string. The ZIP contains one Manager 33457 APK.
  Tablet boot, rail readback under load, and bin stability are pending.

## GPU 1 GHz test build #34

- Build #34 used kernel source 914fef7c89b202bc894601d8184c0b398b792761 on
  codex/t227u-kernel-bringup. The exact diff is in
  kernel/drivers/gpu/mediatek/gpufreq/mt6765/mtk_gpufreq_core.c and
  mtk_gpufreq_core.h. The added Segment-3 bins are 850, 875, 900,
  925, 950, 975, and 1000 MHz, with the existing shared VCORE
  request. Startup and cap reset use the stock 680 MHz point.
  The 975 and 1000 MHz bins program the experimental divide-by-two
  path using the existing clock parking sequence. The VCO targets
  are 1.95 and 2.0 GHz. All bins through 950 MHz use the established
  divider. The legacy raw fixed-voltage clock control rejects new
  above-950 MHz frequencies before writing the regulator.
- Reused the #26 command, Clang 11.0.1, and
  /home/legit-su/t227u-public-repro-build. The before and after
  config SHA-256 matched
  ae63d8ec8ed72586802644719132ae95f303fbd3137518d9783012a50d39af99.
  Kernel release 4.19.191-28847731-abT227USQSCEYE2.
- Checkpatch reported zero errors and warnings. Host harnesses
  compiled the current driver functions and verified the stock
  startup, requested caps, active cap reduction, thermal clamp,
  voltage bounds, all seven PLL DDS values, parked clock writes,
  the /4 to /2 transition and return to /4, and rejection of
  invalid frequencies and non-Segment-3 /2 requests. The linked
  kernel contains all 16 expected Segment-3 OPP rows.
- Full link passed with the existing 14 section mismatch and
  three duplicate vendor export warnings. The CRC gate corrected
  28 values and verified all 203 exports with zero remaining
  mismatches. A separate import check passed for seven vendor
  Wi-Fi, BT, connectivity, GPS, and FM modules.
- Logs in /home/legit-su with prefix t227u-20261121-gpu1000 and
  suffixes -build.log, -crc.log, -policy.log, -pll.log, -linked.log,
  and -package.log. CRC-patched image SHA-256
  b1ee743f15d4bb51d45ee5729fdb6d348f368516177738258ef58c1f6a90c8cf.
- Local E:\T227U-G1.zip SHA-256
  19a818185a64181571446d3b6a12230705c47a2d9a1bb05764cd858b67a8eaf5.
  Checked ZIP integrity, embedded image identity, recovery payload
  modes, and the single Manager 33457 APK. The image and installer
  display string are the only changed payloads from the r9 template.
  Build #34 boot and GPU stability remain pending device testing.

## 2026-11-22 runtime top VCORE test builds #35 and #36

- Build #35 used 0d9556a6871cde6b9ac752e92d5a079f883dac1f and
  passed the link and the 203-export CRC gate. It was superseded
  before packaging when the host harness flagged the unsigned
  conversion in the sysfs write return expression. Logs
  /home/legit-su/t227u-20261122-vcore-build.log and
  /home/legit-su/t227u-20261122-vcore-crc.log.
- Build #36 used 1c594628196ec2fed5189c143108f7508ea1d980 on
  codex/t227u-kernel-bringup. Exact kernel changes are in the two
  commits. They cover the MT6765 DVFSRC debug driver, its header,
  the SPM common wrapper, and the secure service header. Build
  #36 preserves signed write errors explicitly.
- The test sysfs file is
  /sys/devices/platform/10012000.dvfsrc/helio-dvfsrc/dvfsrc_top_vcore_uv.
  It accepts 800000, 806250, 812500, 818750, or 825000 in µV.
  Writing 0 restores the stock 800000 µV command. No voltage
  change runs automatically. A read shows the programmed command,
  the PMIC regulator readback, and the active DVFSRC gear. The
  regulator may reflect the new command only on a gear transition.
  This experiment adjusts shared VCORE for all users of the top gear.
- The code verifies the current firmware gear and SPM command
  layout before invoking the locked secure service. It checks the
  return value and command readback and attempts rollback when
  the top command changes unexpectedly. The compiled host
  harness covered all selectors, range rejection, command and
  regulator reporting, reset, firmware errors, mismatched
  command rollback, and rejection of other layouts. Linked
  symbols include the sysfs attribute, its two callbacks, and the
  SPM wrapper. The 16 linked GPU OPP rows match build #34.
- Reused the #26 command, Clang 11.0.1, the established
  /home/legit-su/t227u-public-repro-build output directory and
  config SHA-256
  ae63d8ec8ed72586802644719132ae95f303fbd3137518d9783012a50d39af99.
  Kernel release 4.19.191-28847731-abT227USQSCEYE2.
- Checkpatch passed. The final link reported the existing 14
  section mismatches and three duplicate exports. The CRC gate
  fixed 28 CRCs and validated all 203 mapped exports. All seven
  vendor connectivity module import checks passed. Logs in
  /home/legit-su with prefix t227u-20261122-vcore and suffixes
  -final-build.log, -final-crc.log, -host.log, -symbols.log,
  -imports.log, and -package.log.
- Local E:\T227U-VC.zip SHA-256
  9313b4a803e480303c62ea4ed34afa68b741bb91a3170794c418ed7625b24360.
  CRC-patched Image.gz SHA-256
  4adda16bc59999cf08816202c69ca025258a65f3d3d768f322e572ae4a7f3ae1.
  ZIP integrity, embedded image identity, single Manager APK, and
  installer payload comparison passed. Boot, firmware acceptance,
  and active-gear rail readback remain pending on hardware.
