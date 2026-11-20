# T227U F2FS bring-up matrix

This branch ports supportable F2FS fixes from the Android/Linux 4.19 lineage.
A small prerequisite is a porting task, not a rejection reason. Each item is
kept here until it is applied or shown to require a larger incompatible change.

| Candidate | Required work | Status |
| --- | --- | --- |
| `0a72eeb86ef5` discard-worker UAF | None beyond moving the existing worker stop before node-manager teardown. | Applied as `9a4acdd02`; full build and vendor CRC gate pass. |
| `72c6b13f468e` checkpoint-frequency reduction | Adds one inode-list type, one checkpoint reason, and parent-xattr tracking. No external API dependency. | Applied; full build and vendor CRC gate pass. Pending on-device encrypted-directory/fsync and power-loss stress. |
| `df304b987283` encrypted casefold lookup fallback | Existing `CONFIG_UNICODE` casefold support and Samsung's shallow strict-mode scan were retained; adds the full-depth fallback after a normal hash miss. | Applied; full build and vendor CRC gate pass. Pending casefolded encrypted-directory regression test. |
| `218cf245fc6a` duplicate encrypted filenames | No prerequisite: `fscrypt_is_nokey_name()` and the F2FS `-ENOKEY` guard are already carried. | Already present; no duplicate port. |
| `888466be8c6b` LZO bounded compression | Adds a checked encoder API. The donor source has later LZO bitstream plumbing absent from this 4.19 tree, so adapt the checks to the older encoder before switching callers. Active callers are F2FS, crypto, and regcache. | Port candidate with a defined small source adaptation and overflow test; not rejected. |
| `821e2b0937da` ZRAM writeback access race | Existing writeback path and slot locking match; make access marking explicit so writeback's internal read retains `ZRAM_IDLE`. | Applied; full build and vendor CRC gate pass. Pending device-side ZRAM writeback/memory-pressure stress. |
| CMA allocation work | Map exact allocator dependencies separately; preserve MT6765 multimedia-CMA constraints. | Queue for dependency map and memory-pressure testing. |

The F2FS baseline is Android/Linux 4.19-compatible backports through the
donor's CIP `v4.19.325` line. This is not an on-disk format upgrade.
