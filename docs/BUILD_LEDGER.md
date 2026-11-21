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

- Branch: `codex/t227u-f2fs-bringup`
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
- Device result: **untested**. Expected build stamp `#20`. Confirm boot,
  Wi-Fi, configured LZ4 swap and I/O error counters. No dictionary or
  stream-ownership change is included in this image.

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
