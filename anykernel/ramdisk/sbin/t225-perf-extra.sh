#!/system/bin/sh
# T225 extra performance tuning - I/O scheduler, readahead, swappiness.
# Runs alongside sunflower-performance.sh (CPU/GPU governor); these
# tunables are independent and don't conflict with it.

# I/O scheduler: deadline gives more predictable low-latency I/O than
# cfq on eMMC storage (bfq isn't built into this kernel).
for q in /sys/block/mmcblk0/queue /sys/block/mmcblk0rpmb/queue; do
    [ -d "$q" ] || continue
    echo deadline > "$q/scheduler" 2>/dev/null
done

# Readahead: small bump helps app/asset loading feel smoother.
for ra in /sys/block/mmcblk0/queue/read_ahead_kb; do
    [ -f "$ra" ] || continue
    echo 512 > "$ra" 2>/dev/null
done

# Swappiness: lower value favors keeping apps in RAM (fewer reloads)
# over aggressively swapping to zram - good for a 3GB-RAM device.
if [ -f /proc/sys/vm/swappiness ]; then
    echo 60 > /proc/sys/vm/swappiness 2>/dev/null
fi

exit 0
