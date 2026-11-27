#!/usr/bin/env bash
set -euo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
kernel_dir="$repo_root/kernel"
out_dir=${OUT_DIR:-"$kernel_dir/out-t225"}
toolchain_bin=${TOOLCHAIN_BIN:?Set TOOLCHAIN_BIN to the Android Clang 11.0.1 bin directory}
modules_dir=${T225_VENDOR_MODULES:?Set T225_VENDOR_MODULES to the directory containing WMT provider modules}
jobs=${JOBS:-"$(nproc)"}

export PATH="$toolchain_bin:$PATH"
if ! clang --version | grep -q 'clang version 11.0.1'; then
    echo "Expected Android Clang 11.0.1." >&2
    exit 2
fi

mkdir -p "$out_dir"
out_dir=$(cd "$out_dir" && pwd)
modules_dir=$(cd "$modules_dir" && pwd)

common=(
    -C "$kernel_dir"
    O="$out_dir"
    ARCH=arm64
    LLVM=1
    LLVM_IAS=1
    CLANG_TRIPLE=aarch64-linux-gnu-
    CROSS_COMPILE=aarch64-linux-gnu-
    KCFLAGS=-w
    CONFIG_SECTION_MISMATCH_WARN_ONLY=y
    KSU_VERSION_OVERRIDE=33355
    KSU_VERSION_TAG_OVERRIDE=legacy-susfs-manual@465490a2
)

make "${common[@]}" gta7lite_00_defconfig
cp "$repo_root/configs/t225-grabbag.config" "$out_dir/.config"
make "${common[@]}" olddefconfig
make "${common[@]}" -j"$jobs"

expected_release='4.19.191-28847731-abT225XXSBEYE4'
if [ "$(cat "$out_dir/include/config/kernel.release")" != "$expected_release" ]; then
    echo "Unexpected kernel release for SM-T225." >&2
    exit 1
fi

gate="$repo_root/tools/t225/vendor_crc_gate.py"
patched="$out_dir/vmlinux.t225-vendor"
image="$out_dir/arch/arm64/boot/Image.t225-vendor"
python3 "$gate" "$out_dir/vmlinux" "$modules_dir" --write "$patched"
python3 "$gate" "$patched" "$modules_dir"
llvm-objcopy -O binary -R .note -R .note.gnu.build-id -R .comment -S "$patched" "$image"
gzip -nkf "$image"
gzip -t "$image.gz"
printf 'Verified flash image: %s\n' "$image.gz"
