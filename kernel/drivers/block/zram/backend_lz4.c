// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/lz4.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>

#include "backend_lz4.h"

static void *lz4_create_ctx(const char *name)
{
	void *workspace = vmalloc(LZ4_MEM_COMPRESS);

	return workspace ? workspace : ERR_PTR(-ENOMEM);
}

static void lz4_destroy_ctx(void *workspace)
{
	vfree(workspace);
}

static int lz4_compress(void *workspace, const void *src, void *dst,
			unsigned int *dst_len)
{
	int len = LZ4_compress_default(src, dst, PAGE_SIZE, *dst_len, workspace);

	if (!len)
		return -EINVAL;

	*dst_len = len;
	return 0;
}

static int lz4_decompress(void *workspace, const void *src,
			  unsigned int src_len, void *dst)
{
	int len = LZ4_decompress_safe(src, dst, src_len, PAGE_SIZE);

	return len == PAGE_SIZE ? 0 : -EINVAL;
}

const struct zcomp_ops backend_lz4 = {
	.create_ctx = lz4_create_ctx,
	.destroy_ctx = lz4_destroy_ctx,
	.compress = lz4_compress,
	.decompress = lz4_decompress,
};
