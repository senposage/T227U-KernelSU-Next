#!/system/bin/sh
# The vendor init.ramplus.rc action is triggered at sys.boot_completed.  Wait
# for it to finish, then leave zram uninitialized by default.  Kernel
# Auditor can subsequently configure zram explicitly when the user requests
# it.
sleep 2
swapoff /dev/block/zram0 2>/dev/null || true
echo 1 > /sys/block/zram0/reset 2>/dev/null
exit 0
