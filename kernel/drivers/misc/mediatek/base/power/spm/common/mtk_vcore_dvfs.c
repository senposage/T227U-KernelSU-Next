// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <mtk_spm_internal.h>

static void spm_dvfsfw_init(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_0, 0, 0, 0);

	spin_unlock_irqrestore(&__spm_lock, flags);
}

void spm_go_to_vcorefs(int spm_flags)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_1, spm_flags, 0, 0);

	spin_unlock_irqrestore(&__spm_lock, flags);
}

void mtk_spmfw_init(int dvfsrc_en, int skip_check)
{
	if (is_spm_enabled() != 0x0 && !skip_check)
		return;

	spm_dvfsfw_init();

	spm_go_to_vcorefs(spm_dvfs_flag_init(dvfsrc_en));
}

static long spm_dvfs_pwrap_call(int pwrap_cmd, int pwrap_vcore)
{
	unsigned long flags;
	long ret;

	spin_lock_irqsave(&__spm_lock, flags);

	ret = mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			     VCOREFS_SMC_CMD_3, pwrap_cmd, pwrap_vcore, 0);

	spin_unlock_irqrestore(&__spm_lock, flags);

	return ret;
}

void spm_dvfs_pwrap_cmd(int pwrap_cmd, int pwrap_vcore)
{
	spm_dvfs_pwrap_call(pwrap_cmd, pwrap_vcore);
}

#ifdef CONFIG_MACH_MT6765
long mt6765_spm_write_pwrap_cmd(int pwrap_cmd, int pwrap_vcore)
{
	return spm_dvfs_pwrap_call(pwrap_cmd, pwrap_vcore);
}
#endif
