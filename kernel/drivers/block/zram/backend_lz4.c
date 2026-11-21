// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/lz4.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "backend_lz4.h"

static int lz4_setup_params(struct zcomp_params *params)
{
	LZ4_stream_t *dict_stream = vzalloc(sizeof(*dict_stream));

	if (!dict_stream)
		return -ENOMEM;
	if (LZ4_loadDict(dict_stream, params->dict, params->dict_size) !=
	    params->dict_size) {
		vfree(dict_stream);
		return -EINVAL;
	}
	params->dict_stream = dict_stream;
	return 0;
}

static void lz4_release_params(struct zcomp_params *params)
{
	vfree(params->dict_stream);
}

static void *lz4_create_ctx(const char *name)
{
	void *workspace = vmalloc(LZ4_MEM_COMPRESS);

	return workspace ? workspace : ERR_PTR(-ENOMEM);
}

static void lz4_destroy_ctx(void *workspace)
{
	vfree(workspace);
}

static int lz4_compress(const struct zcomp_params *params, void *workspace,
			const void *src, void *dst, unsigned int *dst_len)
{
	int len;

	if (params->dict_stream) {
		/* Each compressed page starts with the same dictionary state. */
		memcpy(workspace, params->dict_stream, sizeof(LZ4_stream_t));
		len = LZ4_compress_fast_continue(workspace, src, dst, PAGE_SIZE,
						 *dst_len, LZ4_ACCELERATION_DEFAULT);
	} else {
		len = LZ4_compress_default(src, dst, PAGE_SIZE, *dst_len, workspace);
	}

	if (!len)
		return -EINVAL;

	*dst_len = len;
	return 0;
}

static int lz4_decompress(const struct zcomp_params *params,
			  const void *src, unsigned int src_len, void *dst)
{
	int len;

	if (params->dict)
		len = LZ4_decompress_safe_usingDict(src, dst, src_len, PAGE_SIZE,
						    params->dict, params->dict_size);
	else
		len = LZ4_decompress_safe(src, dst, src_len, PAGE_SIZE);

	return len == PAGE_SIZE ? 0 : -EINVAL;
}

const struct zcomp_ops backend_lz4 = {
	.setup_params = lz4_setup_params,
	.release_params = lz4_release_params,
	.create_ctx = lz4_create_ctx,
	.destroy_ctx = lz4_destroy_ctx,
	.compress = lz4_compress,
	.decompress_stateless = lz4_decompress,
};
