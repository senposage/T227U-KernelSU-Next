#!/usr/bin/env bash
# Reproducible T227U source build.  This script never changes release outputs.
set -euo pipefail

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
kernel_dir="$repo_root/kernel"
config="$repo_root/configs/t227u-r7.config"
out_dir=${OUT_DIR:-"$kernel_dir/out-t227u"}
toolchain_bin=${TOOLCHAIN_BIN:?Set TOOLCHAIN_BIN to the Android Clang r383902 bin directory}
jobs=${JOBS:-"$(nproc)"}

for tool in clang ld.lld llvm-ar llvm-nm llvm-objcopy; do
	if [ ! -x "$toolchain_bin/$tool" ]; then
		echo "missing required tool: $toolchain_bin/$tool" >&2
		exit 2
	fi
done

if ! "$toolchain_bin/clang" --version | grep -q 'clang version 11.0.1'; then
	echo "expected Android Clang r383902 / 11.0.1" >&2
	"$toolchain_bin/clang" --version >&2
	exit 2
fi

mkdir -p "$out_dir"
export PATH="$toolchain_bin:$PATH"

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

# Establish generated output metadata, then restore the preserved known-good
# T227U configuration instead of accepting a newly generated bare defconfig.
make "${common[@]}" gta7lite_00_defconfig
cp "$config" "$out_dir/.config"
make "${common[@]}" olddefconfig
make "${common[@]}" -j"$jobs"

test -s "$out_dir/vmlinux"
test -s "$out_dir/arch/arm64/boot/Image.gz"
echo "built: $out_dir/arch/arm64/boot/Image.gz"
