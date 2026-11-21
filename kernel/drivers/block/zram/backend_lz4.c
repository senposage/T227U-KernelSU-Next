// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/lz4.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>

#include "backend_lz4.h"

void *zram_lz4_alloc_workspace(void)
{
	return vmalloc(LZ4_MEM_COMPRESS);
}

void zram_lz4_free_workspace(void *workspace)
{
	vfree(workspace);
}

int zram_lz4_compress(const void *src, void *dst, unsigned int *dst_len,
			 void *workspace)
{
	int len = LZ4_compress_default(src, dst, PAGE_SIZE, *dst_len, workspace);

	if (!len)
		return -EINVAL;

	*dst_len = len;
	return 0;
}

int zram_lz4_decompress(const void *src, unsigned int src_len, void *dst)
{
	int len = LZ4_decompress_safe(src, dst, src_len, PAGE_SIZE);

	return len == PAGE_SIZE ? 0 : -EINVAL;
}
