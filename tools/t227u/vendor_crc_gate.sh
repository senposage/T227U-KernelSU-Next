#!/usr/bin/env bash
# T227U vendor-module ABI gate.
#
# The stock Wi-Fi/BT modules are immutable and require the CRCs recorded in
# vendor_module_crc_overrides.json.  Do not release an Image.gz unless this
# gate has passed and the CRC patch has been applied.
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PATCHER="$SCRIPT_DIR/apply_vendor_crc_overrides.py"
OVERRIDES="$SCRIPT_DIR/vendor_module_crc_overrides.json"

usage() {
	printf 'Usage:\n  %s verify <vmlinux>\n  %s apply <vmlinux> <Image.gz>\n' "$0" "$0" >&2
	exit 2
}

verify() {
	local vmlinux="$1" require_clean="$2" tmp log before after expected summary actual missing notes
	[[ -f "$vmlinux" ]] || { printf 'Missing vmlinux: %s\n' "$vmlinux" >&2; exit 2; }
	tmp="$(mktemp "$(dirname -- "$vmlinux")/.vendor-crc-verify.XXXXXX")"
	log="$(mktemp "$(dirname -- "$vmlinux")/.vendor-crc-verify-log.XXXXXX")"
	trap 'rm -f "$tmp" "$log"' RETURN
	before="$(sha256sum "$vmlinux" | awk '{print $1}')"
	cp "$vmlinux" "$tmp"
	python3 "$PATCHER" "$tmp" "$OVERRIDES" | tee "$log"
	after="$(sha256sum "$vmlinux" | awk '{print $1}')"
	[[ "$before" == "$after" ]] || { printf 'FAIL: verify modified the original vmlinux\n' >&2; exit 1; }
	expected="$(python3 -c 'import json,sys; print(len(json.load(open(sys.argv[1]))))' "$OVERRIDES")"
	summary="$(grep -E '^patched [0-9]+ CRC entries, [0-9]+ symbols not present' "$log" | tail -1 || true)"
	if [[ "$summary" =~ ^patched[[:space:]]+([0-9]+)[[:space:]]+CRC[[:space:]]+entries,[[:space:]]+([0-9]+)[[:space:]]+symbols ]]; then
		actual="${BASH_REMATCH[1]}"
		missing="${BASH_REMATCH[2]}"
	else
		printf 'FAIL: CRC gate did not produce a patch summary\n' >&2
		exit 1
	fi
	notes="$(grep -c '^  NOTE ' "$log" || true)"
	[[ "$actual" == "$expected" && "$missing" == 0 ]] || {
		printf 'FAIL: expected %s mapped exports; got %s patched and %s missing\n' "$expected" "$actual" "$missing" >&2
		exit 1
	}
	if [[ "$require_clean" == 1 && "$notes" != 0 ]]; then
		printf 'FAIL: patched vmlinux still has %s stale CRC entries\n' "$notes" >&2
		exit 1
	fi
	printf 'PASS: %s vendor CRC exports verified (%s pre-patch mismatches)\n' "$actual" "$notes"
}

case "${1:-}" in
	verify)
		[[ $# == 2 ]] || usage
		verify "$2" 0
		;;
	apply)
		[[ $# == 3 ]] || usage
		verify "$2" 0
		python3 "$PATCHER" "$2" "$OVERRIDES"
		verify "$2" 1
		objcopy="${OBJCOPY:-llvm-objcopy}"
		image="${3%.gz}"
		"$objcopy" -O binary -R .note -R .note.gnu.build-id -R .comment -S "$2" "$image"
		gzip -kf "$image"
		printf 'PASS: regenerated %s\n' "${image}.gz"
		;;
	*) usage ;;
esac
