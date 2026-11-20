properties() { '
kernel.string=T227U KernelSU-Next Magisk-transition dry run
do.devicecheck=1
do.cleanup=0
do.cleanuponabort=0
device.name1=gta7lite
supported.versions=
'; }
boot_attributes() {
set_perm_recursive 0 0 755 644 $RAMDISK/*;
set_perm_recursive 0 0 750 750 $RAMDISK/init* $RAMDISK/sbin;
}
BLOCK=/dev/block/by-name/boot;
IS_SLOT_DEVICE=0;
RAMDISK_COMPRESSION=gz;
PATCH_VBMETA_FLAG=auto;
. tools/ak3-core.sh;
dump_boot;

# Exercise the exact production cleanup without touching the boot partition.
if [ -f "$RAMDISK/.backup/.magisk" ]; then
  ui_print "Dry run: removing Magisk ramdisk injection...";
  magiskboot decompress "$RAMDISK/.backup/init.xz" "$RAMDISK/init" || abort "Could not restore original init. Aborting...";
  for fstab in fstab.mt6765 fstab.mt8768; do
    if [ -f "$RAMDISK/.backup/$fstab.xz" ]; then
      magiskboot decompress "$RAMDISK/.backup/$fstab.xz" "$RAMDISK/$fstab" || abort "Could not restore $fstab. Aborting...";
    fi
  done
  rm -rf "$RAMDISK/overlay.d" "$RAMDISK/.backup";
fi

if [ -f "$RAMDISK/init.rc" ] && ! grep -q "init.sunflower-performance.rc" "$RAMDISK/init.rc"; then
  echo 'import /init.sunflower-performance.rc' >> "$RAMDISK/init.rc"
fi
repack_ramdisk;
cd "$SPLITIMG";
cp "$AKHOME/Image.gz" kernel;
cp "$AKHOME/ramdisk-new.cpio.gz" ramdisk.cpio;
cp "$AKHOME/dtb" dtb;
magiskboot repack "$BOOTIMG" "$AKHOME/boot-dryrun.img";
test -s "$AKHOME/boot-dryrun.img" || abort "Dry-run repack failed.";
ui_print "Dry-run repack complete; boot partition was not written.";
exit 0;
