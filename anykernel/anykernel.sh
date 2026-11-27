properties() { '
kernel.string=grabbag | SM-T225 test 1
do.devicecheck=1
do.cleanup=1
do.cleanuponabort=0
device.name1=gta7lite
supported.versions=
'; }

BLOCK=/dev/block/by-name/boot
IS_SLOT_DEVICE=0
RAMDISK_COMPRESSION=auto
PATCH_VBMETA_FLAG=auto
NO_MAGISK_CHECK=1
. tools/ak3-core.sh

# gta7lite also identifies the SM-T227U. Require a hardware or vendor model.
model_confirmed=
for model in $(getprop ro.boot.em.model) $(getprop ro.product.vendor.model) \
  $(sed -n 's/^ro.product.vendor.model=//p' /vendor/build.prop /vendor/etc/build.prop 2>/dev/null); do
  case "$model" in
    SM-T225) model_confirmed=1 ;;
    SM-T*) abort "This build is for SM-T225. Detected $model." ;;
  esac
done
[ "$model_confirmed" ] || abort "SM-T225 model could not be verified. Mount vendor and retry."

# Repack with the installed boot image's DTB and original ramdisk.
split_boot
flash_boot
ui_print "grabbag kernel installed for SM-T225."
