properties() { '
kernel.string=T227U KernelSU-Next r7 CPU stats + governor policy guard
do.devicecheck=1
do.cleanup=1
do.cleanuponabort=0
device.name1=gta7lite
supported.versions=
'; }
boot_attributes() {
set_perm_recursive 0 0 755 644 $RAMDISK/*;
set_perm_recursive 0 0 750 750 $RAMDISK/init* $RAMDISK/sbin;
}

disable_vendor_ramplus() {
  local fstab=/vendor/etc/fstab.ramplus
  local backup=/vendor/etc/fstab.ramplus.t227u.bak

  if [ ! -f "$fstab" ]; then
    ui_print "RamPlus fstab absent; leaving vendor unchanged.";
    return 0;
  fi

  # Avoid requiring a writable vendor mount on subsequent flashes.  This also
  # makes the operation safe to repeat after the original one-time patch.
  if grep -q '^# T227U manual-zram: /dev/block/zram0' "$fstab"; then
    ui_print "Vendor automatic RamPlus is already disabled.";
    return 0;
  fi

  ui_print "Disabling vendor automatic RamPlus...";
  mount -o rw,remount -t auto /vendor || abort "Could not remount /vendor rw. Aborting before boot write...";

  if [ ! -f "$backup" ]; then
    cp -p "$fstab" "$backup" || {
      mount -o ro,remount -t auto /vendor;
      abort "Could not back up fstab.ramplus. Aborting before boot write...";
    }
  fi

  sed -i '/^[[:space:]]*\/dev\/block\/zram0[[:space:]].*zramsize=.*auto_configure/ s/^/# T227U manual-zram: /' "$fstab" || {
    mount -o ro,remount -t auto /vendor;
    abort "Could not patch fstab.ramplus. Aborting before boot write...";
  }

  grep -q '^# T227U manual-zram: /dev/block/zram0' "$fstab" || {
    mount -o ro,remount -t auto /vendor;
    abort "RamPlus fstab verification failed. Aborting before boot write...";
  }
  sync;
  mount -o ro,remount -t auto /vendor || abort "Could not remount /vendor ro. Aborting before boot write...";
}
BLOCK=/dev/block/by-name/boot;
IS_SLOT_DEVICE=0;
RAMDISK_COMPRESSION=gz;
PATCH_VBMETA_FLAG=auto;
. tools/ak3-core.sh;
dump_boot;

# The previous Magisk install left an overlay.d ramdisk injection. Restore the
# original init and fstab files from its in-ramdisk backup, then discard the
# Magisk overlay. Do not use `cpio restore`: this ramdisk's backup metadata
# produces an empty CPIO on restore.
if [ -f "$RAMDISK/.backup/.magisk" ]; then
  ui_print "Removing Magisk ramdisk injection...";
  magiskboot decompress "$RAMDISK/.backup/init.xz" "$RAMDISK/init" || abort "Could not restore original init. Aborting...";
  for fstab in fstab.mt6765 fstab.mt8768; do
    if [ -f "$RAMDISK/.backup/$fstab.xz" ]; then
      magiskboot decompress "$RAMDISK/.backup/$fstab.xz" "$RAMDISK/$fstab" || abort "Could not restore $fstab. Aborting...";
    fi
  done
  rm -rf "$RAMDISK/overlay.d" "$RAMDISK/.backup";
fi

# Remove obsolete profile-tuning hooks. Frequency, GPU, I/O, and VM policy
# are user-controlled through Kernel Auditor, never forced from the ramdisk.
rm -f "$RAMDISK/sbin/sunflower-performance.sh" "$RAMDISK/sbin/t225-perf-extra.sh";
if [ -f "$RAMDISK/init.rc" ]; then
  sed -i '/init\.sunflower-performance\.rc/d' "$RAMDISK/init.rc"
fi

disable_vendor_ramplus;
write_boot;

# Only clean Magisk's data after write_boot has completed successfully.  Keep
# KernelSU, its userspace daemon, and all modules intact.
if [ -d /data/adb ]; then
  ui_print "Removing stale Magisk runtime data...";
  rm -rf /data/adb/magisk;
  rm -f /data/adb/magisk.db;
fi

ui_print " ";
ui_print "=== T227U KernelSU-Next installed ===";
ui_print "Flashed kernel image and matching MT6765 DTB only.";
ui_print "Disabled vendor automatic RamPlus; manual ZRAM remains available.";
