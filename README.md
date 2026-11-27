# grabbag

grabbag is a custom Android 4.19 kernel for the Samsung Galaxy Tab A7 Lite
LTE (`SM-T227U`, `gta7lite`). It combines upstream backports with fixes
for this device and targets the T227U vendor ABI and
`gta7lite_00_defconfig`.

Prebuilt artifacts are deliberately not committed here. Flashable packages are
published on the [Releases](../../releases) page.

## Included work

- KernelSU-Next integration.
- CPU frequency statistics for frequency residency/uptime reporting.
- CPU OC OPPs, opt-in through Kernel Adiutor:
  - performance cluster: 2550 and 2600 MHz;
  - efficiency cluster: 1900, 2000, 2100, 2200, and 2300 MHz.
- Segment-3 GPU OC OPPs at 700, 725, 750, 775, 800, and 825 MHz.
  The GPU starts at the stock 680 MHz ceiling. Higher bins are selectable
  through Kernel Adiutor and request the highest shared VCORE DVFS step.
  r9 adds 25 MHz steps from 850 MHz through 1 GHz and experimental
  1.1 and 1.2 GHz bins.
- CPU/GPU OC ceiling and voltage-control interfaces, including live rail
  readback for the GPU path.
- PPM/hotplug policy guard: PPM remains a thermal/power safety envelope but
  no longer overwrites the min/max policy selected by the governor or Kernel
  Adiutor.
- Existing stock thermal, power, and regulator protections remain enabled.

## Vendor Wi-Fi and Bluetooth ABI

The stock vendor modules are immutable and are built with Samsung's exported
symbol CRCs. Any kernel build that changes the ABI must patch those CRCs before
release, otherwise Wi-Fi and Bluetooth modules will not load.

`tools/t227u/vendor_crc_gate.sh` is mandatory for releases:

```sh
# Validate an unpatched build against the T227U vendor ABI map.
tools/t227u/vendor_crc_gate.sh verify out-t227u/vmlinux

# Validate, patch, validate again, then regenerate Image.gz.
OBJCOPY=/path/to/llvm-objcopy \
  tools/t227u/vendor_crc_gate.sh apply out-t227u/vmlinux \
  out-t227u/arch/arm64/boot/Image.gz
```

The gate requires all 203 mapped exports, with zero missing symbols and zero
stale CRCs after patching. A build must not be released unless this passes,
unless explicitly overridden.

## Build configuration

`configs/t227u-r9.config` is the configuration used for r9. It includes the
T227U FPSGO and utilization-clamp selections alongside the KernelSU and vendor
compatibility selections.

The complete KernelSU-Next legacy source used by this tree is kept at
`kernel/KernelSU`; it is intentionally source-controlled rather than supplied
by an untracked local checkout.  The reproducible build entry point is:

```sh
TOOLCHAIN_BIN=/path/to/android-clang-r383902/bin \
  tools/t227u/build_t227u.sh
```

This target requires Android Clang r383902 / 11.0.1 and its LLVM binutils.
It creates a disposable `kernel/out-t227u/` directory, initializes it with
`gta7lite_00_defconfig`, applies the r9 configuration, and pins the integrated
KernelSU-Next legacy source to driver version 33355
(`legacy-susfs-manual@465490a2`).  It does not apply the vendor CRC override;
run the mandatory gate above on the resulting `vmlinux` and `Image.gz` before
packaging or testing vendor Wi-Fi/Bluetooth modules.

## Kernel Adiutor

The matching userspace controls live in the
[Kernel Adiutor T227U branch](https://github.com/senposage/KernelAdiutor/tree/codex/t227u-dvfs-policy).
The flash ZIP includes KernelSU Manager. Install Kernel Adiutor from the
separate APK.
