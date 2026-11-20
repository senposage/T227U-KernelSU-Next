/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Compat shim for kernels older than 5.5, where linux/pgtable.h did not
 * yet exist (it was introduced by renaming asm-generic/pgtable.h).
 * This lets code written against the newer API (e.g. KernelSU-Next)
 * build against this pre-5.5 kernel tree.
 */
#ifndef _LINUX_PGTABLE_H_COMPAT
#define _LINUX_PGTABLE_H_COMPAT

#include <asm/pgtable.h>

#endif /* _LINUX_PGTABLE_H_COMPAT */
