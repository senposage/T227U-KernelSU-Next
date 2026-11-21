# T227U r8 test · LZ4 ZRAM dictionaries

For the Samsung Galaxy Tab A7 Lite SM-T227U. This flashable ZIP uses the
Samsung 4.19.191 tree and T227USQSCEYE2 base. Build #22 is an
experimental release for testing.

- Modular LZ4 backend with separate compressor-independent reads.
- Opt-in per-device LZ4 dictionary and prebuilt compression template.
- Retains the tested VM and vendor RamPlus behavior. Plain LZ4 is
  available without a dictionary.
- All 197 mapped vendor CRCs passed. Boot, Wi-Fi, automatic dictionary
  activation and live swap I/O were verified. Asphalt reached the race
  with zero ZRAM I/O errors. The performance captures use different
  starting states, so no speedup is claimed.

Flash `T227U-LZ4D.zip` in recovery. The matching KernelSU Next Manager
33457 is bundled once and available separately below. Install the
[matching Kernel Adiutor test APK](https://github.com/senposage/KernelAdiutor/releases/tag/t227u-ka-lz4d)
to load a dictionary automatically. Save a nonzero size and enable
Virtual Memory Apply on Boot, then copy a raw dictionary to
`Download/KernelAdiutor/zram-lz4.dict` on the tablet. KA uploads it
before enabling ZRAM swap.

Use the attached `train_lz4_dictionary.py` with Python 3 and the
`zstd` CLI. For example:

```sh
python3 train_lz4_dictionary.py -o zram-lz4.dict asphalt.bin browsing.bin
```

The inputs are separate raw 4 KiB page captures. Keep your memory
captures and generated dictionaries private. The release contains
the training tool and no sample data.

Kernel source build commit: `e952c890224f81755de77f6f9db13f7e6ab43e90`.

ZIP SHA-256:
`f6f36c9be674c8d1c41dd7380f81ac49d983689e73253bd9c2debec3e73f341e`.
