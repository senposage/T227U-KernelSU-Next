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
committed or packaged, trace the changed behavior from its triggering event to
the modified code and record the result in the commit body. The trace must
explicitly check:

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

## Device-side vendor change

Performed in TWRP recovery on 2026-11-20, independently of any boot ZIP:

- Backup: `/vendor/etc/fstab.ramplus.t227u.bak`
- Changed only `/vendor/etc/fstab.ramplus`: commented the
  `/dev/block/zram0 ... zramsize=2147483648,auto_configure` line.
- Verification: vendor remounted read-only; subsequent kernel log reported
  `ZramDevice:0kB` and `Total swap = 0kB`.

This vendor change prevents automatic 2 GiB RamPlus activation. It does not
add an initramfs hook and leaves manual ZRAM configuration available.
