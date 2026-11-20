# SM-T227U KernelSU-Next Kernel

Custom Android 4.19 kernel source for the Samsung Galaxy Tab A7 Lite LTE
(`SM-T227U`, `gta7lite`). This tree is derived from the related T225 source
and is maintained for the T227U vendor ABI and `gta7lite_00_defconfig` build.

Prebuilt artifacts are deliberately not committed here. Flashable packages are
published on the [Releases](../../releases) page.

## Included work

- KernelSU-Next integration.
- CPU frequency statistics for frequency residency/uptime reporting.
- CPU OC OPPs, opt-in through Kernel Adiutor:
  - performance cluster: 2550 and 2600 MHz;
  - efficiency cluster: 1900, 2000, 2100, 2200, and 2300 MHz.
- GPU OC OPPs: 700, 725, and 750 MHz. The OC ceiling defaults to the stock
  Segment-3 OPP; userspace must explicitly raise it.
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

The gate requires all 197 mapped exports, with zero missing symbols and zero
stale CRCs after patching. A build must not be released unless this passes,
unless explicitly overridden.

## Build configuration

`configs/t227u-r7.config` is the preserved working r7 configuration. Build
from that configuration rather than regenerating a bare defconfig, so the
working KernelSU and vendor compatibility selections are retained.

## Kernel Adiutor

The matching userspace controls live in the
[KernelAdiutor-T227U](https://github.com/senposage/KernelAdiutor-T227U) fork.
The flash ZIP contains KernelSU Manager, not Kernel Adiutor itself.
