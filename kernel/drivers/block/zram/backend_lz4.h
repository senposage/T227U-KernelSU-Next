/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ZRAM_BACKEND_LZ4_H_
#define _ZRAM_BACKEND_LZ4_H_

void *zram_lz4_alloc_workspace(void);
void zram_lz4_free_workspace(void *workspace);
int zram_lz4_compress(const void *src, void *dst, unsigned int *dst_len,
			 void *workspace);
int zram_lz4_decompress(const void *src, unsigned int src_len, void *dst);

#endif /* _ZRAM_BACKEND_LZ4_H_ */
