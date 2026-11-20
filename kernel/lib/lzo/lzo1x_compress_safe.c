// SPDX-License-Identifier: GPL-2.0-only
/*
 * Checked LZO1X compressor instantiation.  The implementation is shared with
 * lzo1x_compress.c, with HAVE_OP() enforcing the caller's output limit.
 */

#define LZO_SAFE(name) name ## _safe
#define HAVE_OP(x) ((size_t)(op_end - op) >= (size_t)(x))

#include "lzo1x_compress.c"
