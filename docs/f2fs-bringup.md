# T227U F2FS bring-up matrix

This branch ports supportable F2FS fixes from the Android/Linux 4.19 lineage.
A small prerequisite is a porting task, not a rejection reason. Each item is
kept here until it is applied or shown to require a larger incompatible change.

| Candidate | Required work | Status |
| --- | --- | --- |
| `0a72eeb86ef5` discard-worker UAF | None beyond moving the existing worker stop before node-manager teardown. | Applied as `9a4acdd02`; full build and vendor CRC gate pass. |
| `72c6b13f468e` checkpoint-frequency reduction | Adds one inode-list type, one checkpoint reason, and parent-xattr tracking. No external API dependency. | Applied; full build and vendor CRC gate pass. Pending on-device encrypted-directory/fsync and power-loss stress. |
| `df304b987283` encrypted casefold lookup fallback | Requires the matching Unicode/casefold filename-handling chain, then directory-lookup regression coverage. | Queue as a dependency series, not rejected. |
| `218cf245fc6a` duplicate encrypted filenames | No prerequisite: `fscrypt_is_nokey_name()` and the F2FS `-ENOKEY` guard are already carried. | Already present; no duplicate port. |
| LZO/LZO-RLE safety series | Ordered compressor fixes plus malformed-stream and ZRAM stress tests. | Queue as a complete series. |
| ZRAM writeback race/CMA work | Map exact allocator and writeback dependencies; preserve MT6765 multimedia-CMA constraints. | Queue for dependency map and memory-pressure testing. |

The F2FS baseline is Android/Linux 4.19-compatible backports through the
donor's CIP `v4.19.325` line. This is not an on-disk format upgrade.
