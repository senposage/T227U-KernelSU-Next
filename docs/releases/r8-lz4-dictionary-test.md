# SM-T227U r8 prerelease · LZ4 ZRAM dictionaries

For the Samsung Galaxy Tab A7 Lite LTE (SM-T227U, `gta7lite`).
Build #22 uses Samsung Linux 4.19.191 and the T227USQSCEYE2 vendor ABI.

## Kernel changes

- Modular LZ4 ZRAM backend with decompression independent of compression
  streams, opt-in dictionaries, and a reusable compression template.
- Retains the T227U CPU/GPU controls, CPU frequency residency, PPM and
  hotplug policy fixes, and vendor radio compatibility. All 197 mapped
  vendor CRCs passed the release gate.
- The AnyKernel installer backs up the vendor RamPlus fstab where
  applicable and disables automatic vendor ZRAM setup so KA can configure
  ZRAM explicitly.

## Install and use

1. Back up Boot in recovery. Flash `T227U-LZ4D.zip` and reboot.
2. Use KernelSU Next Manager build 33457. The ZIP contains one copy and
   the matching APK is also attached below.
3. Install the [updated Kernel Adiutor companion](https://github.com/senposage/KernelAdiutor/releases/tag/t227u-ka-lz4d).
   To load a dictionary, put one or more raw `.dict` files in
   `Download/KernelAdiutor`, choose one from the **LZ4 dictionary** card,
   save a nonzero ZRAM size, and enable Virtual Memory **Apply on Boot**.
   The **Plain LZ4** option keeps dictionary use optional.

The companion also trains 16 KiB dictionaries on-device. Select workloads
manually or use Auto with process exclusions. Use **Build when idle** after
sampling. For desktop or rooted ADB training, the attached
`train_lz4_dictionary.py` accepts multiple workloads and repeated
device captures. Run `--help` for the input options. Keep app-memory
captures and workload dictionaries private.

## Verification

Build #22 booted on the SM-T227U, associated with Wi-Fi, and reached the
start of an Asphalt race with dictionary-backed swap and zero ZRAM I/O
errors. A 16,235-byte test dictionary also loaded on reboot and remained
active through the latest KA update. The workload runs used different
starting states, so the effect on compression and latency remains open.

Kernel source build commit `e952c890224f81755de77f6f9db13f7e6ab43e90`.

ZIP SHA-256 `f6f36c9be674c8d1c41dd7380f81ac49d983689e73253bd9c2debec3e73f341e`.
