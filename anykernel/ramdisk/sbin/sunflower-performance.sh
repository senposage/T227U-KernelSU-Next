#!/system/bin/sh
setgov() {
    for f in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; do
        [ -f "$f" ] || continue
        avail="$(dirname "$f")/scaling_available_governors"
        [ -f "$avail" ] || continue
        grep -qw performance "$avail" 2>/dev/null && echo performance > "$f" 2>/dev/null
    done
}
setgpu() {
    for f in \
        /sys/class/kgsl/kgsl-3d0/devfreq/governor \
        /sys/class/devfreq/*/governor \
        /sys/devices/platform/*gpu*/devfreq/*/governor; do
        [ -f "$f" ] || continue
        grep -qw performance "$f" 2>/dev/null && echo performance > "$f" 2>/dev/null
    done
}
setgov
setgpu
exit 0
