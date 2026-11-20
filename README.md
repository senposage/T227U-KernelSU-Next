# T227U KernelSU-Next kernel workspace

This repository tracks the editable T225-derived kernel source being adapted
for the Samsung SM-T227U, its AnyKernel3 flash packaging, and the known-good
release inputs.

## Current known-good baseline

- Device: `gta7lite` / SM-T227U
- Kernel release: `4.19.191-28847731-abT227USQSCEYE2`
- Flash package: `releases/T227U-KernelSU-Next-gta7lite-20261119-r3-radiofix.zip`
- KernelSU Manager: `manager/KernelSU_Next_v3.4.2-102-gb2c1ad00_33457-release.apk`
  (UAPI 6; signed with the certificate accepted by the in-tree driver)

## Layout

- `kernel/` — editable kernel source and KernelSU-Next integration.
- `anykernel/` — AnyKernel3 staging tree used to create flashable packages.
- `releases/` — verified flash packages.
- `manager/` — matching KernelSU-Next Manager APK.

## r4 requirements

1. Apply and validate the requested OPP/PTP frequency-table work.
2. Preserve the T227U module CRC fix and T227U UTS release.
3. Include Manager build `33457` (UAPI 6).
4. Remove Magisk ramdisk injection before repacking the boot image; only after
   a successful boot-image write, remove Magisk-owned `/data/adb` state while
   preserving KernelSU and `/data/adb/modules`.
