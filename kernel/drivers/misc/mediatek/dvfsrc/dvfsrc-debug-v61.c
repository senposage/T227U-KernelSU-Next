// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mfd/mt6357/registers.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/suspend.h>

#include "dvfsrc-debug.h"
#include "dvfsrc-common.h"
#if IS_ENABLED(CONFIG_MTK_DRAMC_LEGACY)
#include <mtk_dramc.h>
#endif

enum dvfsrc_regs {
	DVFSRC_BASIC_CONTROL,
	DVFSRC_SW_REQ1,
	DVFSRC_INT,
	DVFSRC_INT_EN,
	DVFSRC_SW_BW_0,
	DVFSRC_VCORE_REQUEST,
	DVFSRC_LEVEL,
	DVFSRC_LAST,
	DVFSRC_MD_SCENARIO,
	DVFSRC_RECORD_0,
	DVFSRC_RSRV_0,
};

static const int mt6761_regs[] = {
	[DVFSRC_BASIC_CONTROL] = 0x0,
	[DVFSRC_SW_REQ1] = 0x4,
	[DVFSRC_INT] = 0x98,
	[DVFSRC_INT_EN] = 0x9C,
	[DVFSRC_SW_BW_0] = 0x160,
	[DVFSRC_VCORE_REQUEST] = 0x48,
	[DVFSRC_LEVEL] = 0xDC,
	[DVFSRC_LAST] = 0x308,
	[DVFSRC_MD_SCENARIO] = 0x310,
	[DVFSRC_RECORD_0] = 0x400,
	[DVFSRC_RSRV_0] = 0x600,
};

enum dvfsrc_spm_regs {
	SPM_POWERON_CONFIG_EN,
	SPM_PCM_IM_PTR,
	SPM_MD2SPM_DVFS_CON,
	SPM_SW_FLAG,
	SPM_SW_RSV_9,
	SPM_DVFS_EVENT_STA,
	SPM_DVFS_LEVEL,
	SPM_DFS_LEVEL,
	SPM_DVS_LEVEL,
	SPM_DVFS_CMD0,
	SPM_DVFS_CMD1,
	SPM_DVFS_CMD2,
	SPM_DVFS_CMD3,
	SPM_DVFS_CMD4,
};

static const int mt6761_spm_regs[] = {
	[SPM_POWERON_CONFIG_EN] = 0x0,
	[SPM_PCM_IM_PTR] = 0x020,
	[SPM_MD2SPM_DVFS_CON] = 0x43C,
	[SPM_SW_FLAG] = 0x600,
	[SPM_SW_RSV_9] = 0x658,
	[SPM_DVFS_EVENT_STA] = 0x69C,
	[SPM_DVFS_LEVEL] = 0x6A4,
	[SPM_DFS_LEVEL] = 0x6B0,
	[SPM_DVS_LEVEL] = 0x6B4,
	[SPM_DVFS_CMD0] = 0x710,
	[SPM_DVFS_CMD1] = 0x714,
	[SPM_DVFS_CMD2] = 0x718,
	[SPM_DVFS_CMD3] = 0x71C,
	[SPM_DVFS_CMD4] = 0x720,
};

#define DVFSRC_TARGET_LEVEL(x)	(((x) >> 0) & 0x0000ffff)
#define DVFSRC_CURRENT_LEVEL(x)	(((x) >> 16) & 0x0000ffff)

static u32 dvfsrc_read(struct mtk_dvfsrc *dvfs, u32 reg, u32 offset)
{
	return readl(dvfs->regs + dvfs->dvd->config->regs[reg] + offset);
}

static u32 spm_read(struct mtk_dvfsrc *dvfs, u32 reg)
{
	return readl(dvfs->spm_regs + dvfs->dvd->config->spm_regs[reg]);
}

#ifdef CONFIG_MACH_MT6765
#define TOP_VCORE_BASE_UV	518750
#define TOP_VCORE_STEP_UV	6250
#define TOP_VCORE_STOCK_UV	800000
#define TOP_VCORE_NO_FLOOR_MAX_UV	900000
#define TOP_VCORE_MAX_UV	925000
#define TOP_VCORE_CMD		0
#define TOP_VCORE_PMIC_REG	((u32)MT6357_RG_BUCK_VCORE_VOSEL_ADDR << 16)
#define TOP_VCORE_SELECTOR(uv)	(((uv) - TOP_VCORE_BASE_UV) / TOP_VCORE_STEP_UV)

#define VSRAM_TEST_STOCK_UV	900000
#define VSRAM_TEST_MAX_UV	1025000
#define VSRAM_TEST_HEADROOM_UV	75000
#define VSRAM_HIGH_HEADROOM_UV	100000
#define VSRAM_TEST_MAX_GAP_UV	350000
#define VCORE_LOWEST_FW_UV	650000
#define VCORE_SRAM_FLOOR_UV	675000
#define VSRAM_NO_FLOOR_MAX_UV	(VCORE_LOWEST_FW_UV + VSRAM_TEST_MAX_GAP_UV)

static DEFINE_MUTEX(top_vcore_lock);
static bool vsram_override_active;
static struct mtk_pm_qos_request vsram_high_floor;

static int dvfsrc_vcore_cmd_uv(u32 cmd)
{
	return TOP_VCORE_BASE_UV + (cmd & 0x7f) * TOP_VCORE_STEP_UV;
}

static int dvfsrc_top_vcore_read_cmd(struct mtk_dvfsrc *dvfsrc, u32 *cmd)
{
	if (!dvfsrc->spm_regs || !dvfsrc->vcore_power ||
	    mtk_dvfsrc_vcore_opp_count() != 4 ||
	    mtk_dvfsrc_vcore_uv_table(0) != TOP_VCORE_STOCK_UV)
		return -EOPNOTSUPP;

	/* Confirm the firmware's VCORE command layout before using slot 0. */
	if (spm_read(dvfsrc, SPM_DVFS_CMD1) != (TOP_VCORE_PMIC_REG | 0x25) ||
	    spm_read(dvfsrc, SPM_DVFS_CMD2) != (TOP_VCORE_PMIC_REG | 0x1d) ||
	    spm_read(dvfsrc, SPM_DVFS_CMD3) !=
	    (TOP_VCORE_PMIC_REG | TOP_VCORE_SELECTOR(VCORE_SRAM_FLOOR_UV)) ||
	    spm_read(dvfsrc, SPM_DVFS_CMD4) !=
	    (TOP_VCORE_PMIC_REG | TOP_VCORE_SELECTOR(VCORE_LOWEST_FW_UV)))
		return -EOPNOTSUPP;

	*cmd = spm_read(dvfsrc, SPM_DVFS_CMD0);
	if (*cmd < (TOP_VCORE_PMIC_REG | TOP_VCORE_SELECTOR(TOP_VCORE_STOCK_UV)) ||
	    *cmd > (TOP_VCORE_PMIC_REG | TOP_VCORE_SELECTOR(TOP_VCORE_MAX_UV)))
		return -EOPNOTSUPP;

	return 0;
}

static ssize_t dvfsrc_top_vcore_uv_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	u32 cmd;
	int ret;
	int live_uv;
	int gear;

	if (!dvfsrc->vcore_power)
		return -ENODEV;

	mutex_lock(&top_vcore_lock);
	live_uv = regulator_get_voltage(dvfsrc->vcore_power);
	gear = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_OPP);
	ret = dvfsrc_top_vcore_read_cmd(dvfsrc, &cmd);
	if (ret)
		ret = scnprintf(buf, PAGE_SIZE,
				"programmed_uv=unavailable live_uv=%d active_gear=%d\n",
				live_uv, gear);
	else
		ret = scnprintf(buf, PAGE_SIZE,
				"programmed_uv=%u live_uv=%d active_gear=%d\n",
				dvfsrc_vcore_cmd_uv(cmd),
				live_uv, gear);
	mutex_unlock(&top_vcore_lock);

	return ret;
}

static int dvfsrc_vsram_test_get_power(struct mtk_dvfsrc *dvfsrc)
{
	struct regulator *power;

	if (dvfsrc->vsram_test_power)
		return 0;

	power = devm_regulator_get_optional(dvfsrc->dev, "vsram_others");
	if (IS_ERR(power))
		return PTR_ERR(power);

	dvfsrc->vsram_test_power = power;
	return 0;
}

/* Called with top_vcore_lock held. Always allow restoring the stock target. */
static int dvfsrc_top_vcore_limit(struct mtk_dvfsrc *dvfsrc)
{
	int vsram_uv;

	if (dvfsrc_vsram_test_get_power(dvfsrc))
		return TOP_VCORE_STOCK_UV;

	vsram_uv = regulator_get_voltage(dvfsrc->vsram_test_power);
	if (vsram_uv < TOP_VCORE_STOCK_UV + VSRAM_TEST_HEADROOM_UV)
		return TOP_VCORE_STOCK_UV;
	if (!mtk_pm_qos_request_active(&vsram_high_floor))
		return min(vsram_uv - VSRAM_TEST_HEADROOM_UV,
			   TOP_VCORE_NO_FLOOR_MAX_UV);

	return min(vsram_uv - VSRAM_HIGH_HEADROOM_UV, TOP_VCORE_MAX_UV);
}

/* The SPM slot is consumed on gear changes. Refresh an already held gear. */
static int dvfsrc_refresh_top_vcore(struct mtk_dvfsrc *dvfsrc, int uv)
{
	struct mtk_pm_qos_request hold = {};
	struct regulator *rail;
	int sram_uv, old_uv, live_uv, ret;

	if ((!vsram_override_active && !dvfsrc->suspend_vsram_uv) ||
	    mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_OPP) != VCORE_OPP_0 ||
	    regulator_get_voltage(dvfsrc->vcore_power) == uv)
		return 0;

	if (!dvfsrc->vsram_test_power)
		return -ENODEV;

	mtk_pm_qos_add_request(&hold, MTK_PM_QOS_VCORE_OPP, VCORE_OPP_0);
	sram_uv = regulator_get_voltage(dvfsrc->vsram_test_power);
	old_uv = regulator_get_voltage(dvfsrc->vcore_power);
	if (mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_OPP) != VCORE_OPP_0 ||
	    sram_uv < uv + VSRAM_TEST_HEADROOM_UV ||
	    sram_uv < old_uv + VSRAM_TEST_HEADROOM_UV ||
	    sram_uv > uv + VSRAM_TEST_MAX_GAP_UV ||
	    sram_uv > old_uv + VSRAM_TEST_MAX_GAP_UV) {
		ret = -EAGAIN;
		goto out;
	}
	if (old_uv == uv) {
		ret = 0;
		goto out;
	}

	/* Drop the consumer vote after this write. SPM still owns DVFS. */
	rail = regulator_get_optional(dvfsrc->dev, "vcore");
	if (IS_ERR(rail)) {
		ret = PTR_ERR(rail);
		goto out;
	}

	ret = regulator_set_voltage(rail, uv, uv);
	live_uv = regulator_get_voltage(rail);
	if (ret || live_uv != uv) {
		if (live_uv != old_uv &&
		    (regulator_set_voltage(rail, old_uv, old_uv) ||
		     regulator_get_voltage(rail) != old_uv))
			dev_err(dvfsrc->dev, "could not restore live VCORE %d uV\n", old_uv);
		if (!ret)
			ret = -EIO;
	}
	regulator_put(rail);

out:
	mtk_pm_qos_remove_request(&hold);
	return ret;
}

static int dvfsrc_set_top_vcore(struct mtk_dvfsrc *dvfsrc, u32 uv)
{
	struct device *dev = dvfsrc->dev;
	u32 selector, old_cmd, new_cmd;
	int ret;
	long status;

	if (uv < TOP_VCORE_STOCK_UV || uv > TOP_VCORE_MAX_UV ||
	    (uv - TOP_VCORE_BASE_UV) % TOP_VCORE_STEP_UV)
		return -EINVAL;

	selector = TOP_VCORE_SELECTOR(uv);

	ret = dvfsrc_top_vcore_read_cmd(dvfsrc, &old_cmd);
	if (ret)
		return ret;

	if (uv > TOP_VCORE_STOCK_UV && (int)uv > dvfsrc_top_vcore_limit(dvfsrc))
		return -ERANGE;
	if (old_cmd != (TOP_VCORE_PMIC_REG | selector)) {
		status = mt6765_spm_write_pwrap_cmd(TOP_VCORE_CMD, selector);
		ret = dvfsrc_top_vcore_read_cmd(dvfsrc, &new_cmd);
		if (status || ret || new_cmd != (TOP_VCORE_PMIC_REG | selector)) {
			dev_warn(dev, "top VCORE update failed, firmware status %ld\n", status);
			ret = status == SIP_SVC_E_NOT_SUPPORTED ? -EOPNOTSUPP : -EIO;
			goto restore_cmd;
		}
	}

	ret = dvfsrc_refresh_top_vcore(dvfsrc, uv);
	if (!ret)
		return 0;

restore_cmd:
	if (spm_read(dvfsrc, SPM_DVFS_CMD0) != old_cmd) {
		mt6765_spm_write_pwrap_cmd(TOP_VCORE_CMD, old_cmd & 0x7f);
		if (spm_read(dvfsrc, SPM_DVFS_CMD0) != old_cmd)
			dev_err(dev, "failed to restore VCORE command slot 0\n");
	}
	return ret;
}

static ssize_t dvfsrc_top_vcore_uv_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	u32 uv;
	int ret;

	if (kstrtou32(buf, 10, &uv))
		return -EINVAL;

	mutex_lock(&top_vcore_lock);
	if (dvfsrc->suspend_vsram_uv) {
		ret = -EBUSY;
	} else {
		ret = dvfsrc_set_top_vcore(dvfsrc, uv ?: TOP_VCORE_STOCK_UV);
		if (!ret && dvfsrc->gpu_rail_restore_failed) {
			ret = mt6765_gpufreq_hold_stock_ceiling(false);
			if (!ret)
				dvfsrc->gpu_rail_restore_failed = false;
		}
	}
	mutex_unlock(&top_vcore_lock);

	if (ret)
		return ret;

	return count;
}

static ssize_t dvfsrc_top_vcore_max_uv_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	int max_uv;

	mutex_lock(&top_vcore_lock);
	max_uv = dvfsrc_top_vcore_limit(dvfsrc);
	mutex_unlock(&top_vcore_lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", max_uv);
}

static ssize_t dvfsrc_vsram_test_uv_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	int ret;

	if (!dvfsrc->vcore_power)
		return -ENODEV;

	mutex_lock(&top_vcore_lock);
	ret = dvfsrc_vsram_test_get_power(dvfsrc);
	if (!ret)
		ret = scnprintf(buf, PAGE_SIZE,
				"vsram_uv=%d vcore_uv=%d active_gear=%d floor_active=%d max_uv=%d min_delta_uv=%d\n",
				regulator_get_voltage(dvfsrc->vsram_test_power),
				regulator_get_voltage(dvfsrc->vcore_power),
				mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_OPP),
				mtk_pm_qos_request_active(&vsram_high_floor),
				VSRAM_TEST_MAX_UV, VSRAM_TEST_HEADROOM_UV);
	mutex_unlock(&top_vcore_lock);

	return ret;
}

static int dvfsrc_set_vsram(struct mtk_dvfsrc *dvfsrc, u32 uv)
{
	struct device *dev = dvfsrc->dev;
	u32 cmd;
	int old_uv, programmed_uv, live_uv, levels, gear, headroom, ret;
	bool added_floor = false;

	if (uv < VSRAM_TEST_STOCK_UV || uv > VSRAM_TEST_MAX_UV ||
	    (uv - VSRAM_TEST_STOCK_UV) % TOP_VCORE_STEP_UV)
		return -EINVAL;

	ret = dvfsrc_top_vcore_read_cmd(dvfsrc, &cmd);
	if (ret)
		goto out;
	ret = dvfsrc_vsram_test_get_power(dvfsrc);
	if (ret)
		goto out;

	old_uv = regulator_get_voltage(dvfsrc->vsram_test_power);
	if (old_uv < VSRAM_TEST_STOCK_UV || old_uv > VSRAM_TEST_MAX_UV ||
	    (!vsram_override_active && old_uv != VSRAM_TEST_STOCK_UV)) {
		ret = -EOPNOTSUPP;
		goto out;
	}
	if ((int)uv == old_uv && (uv != VSRAM_TEST_STOCK_UV ||
				  !vsram_override_active))
		goto out;

	programmed_uv = dvfsrc_vcore_cmd_uv(cmd);
	headroom = programmed_uv > TOP_VCORE_NO_FLOOR_MAX_UV ?
		VSRAM_HIGH_HEADROOM_UV : VSRAM_TEST_HEADROOM_UV;
	if (programmed_uv > min_t(int, old_uv, uv) - headroom) {
		ret = -ERANGE;
		goto out;
	}

	levels = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_NUM_DVFS_OPP);
	if (levels <= 0 || dvfsrc->force_opp_idx < (u32)levels) {
		ret = -EBUSY;
		goto out;
	}

	/* Above 1 V, gear 3 would exceed the permitted VSRAM to VCORE spread. */
	if (max_t(int, old_uv, uv) > VSRAM_NO_FLOOR_MAX_UV) {
		if (mtk_dvfsrc_vcore_uv_table(VCORE_OPP_2) != VCORE_SRAM_FLOOR_UV ||
		    max_t(int, old_uv, uv) > VCORE_SRAM_FLOOR_UV + VSRAM_TEST_MAX_GAP_UV) {
			ret = -EOPNOTSUPP;
			goto out;
		}
		if (!mtk_pm_qos_request_active(&vsram_high_floor)) {
			mtk_pm_qos_add_request(&vsram_high_floor,
					       MTK_PM_QOS_VCORE_OPP, VCORE_OPP_2);
			added_floor = true;
		}
		gear = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_OPP);
		if (gear < VCORE_OPP_0 || gear > VCORE_OPP_2) {
			ret = -EAGAIN;
			goto release_floor;
		}
	}

	live_uv = regulator_get_voltage(dvfsrc->vcore_power);
	if (live_uv < max_t(int, old_uv, uv) - VSRAM_TEST_MAX_GAP_UV ||
	    live_uv > min_t(int, old_uv, uv) - VSRAM_TEST_HEADROOM_UV) {
		ret = -EAGAIN;
		goto release_floor;
	}

	ret = regulator_set_voltage(dvfsrc->vsram_test_power, uv, uv);
	if (ret || regulator_get_voltage(dvfsrc->vsram_test_power) != (int)uv) {
		if (regulator_get_voltage(dvfsrc->vsram_test_power) != old_uv)
			regulator_set_voltage(dvfsrc->vsram_test_power, old_uv, old_uv);
		if (regulator_get_voltage(dvfsrc->vsram_test_power) != old_uv) {
			/* Block suspend while the SRAM regulator is in an unknown state. */
			vsram_override_active = true;
			dev_err(dev, "VSRAM test could not restore %d uV\n", old_uv);
			ret = -EIO;
			goto out;
		}
		if (!ret)
			ret = -EIO;
		goto release_floor;
	}

	if (uv <= VSRAM_NO_FLOOR_MAX_UV &&
	    mtk_pm_qos_request_active(&vsram_high_floor))
		mtk_pm_qos_remove_request(&vsram_high_floor);

	vsram_override_active = uv != VSRAM_TEST_STOCK_UV;
	if (!vsram_override_active) {
		devm_regulator_put(dvfsrc->vsram_test_power);
		dvfsrc->vsram_test_power = NULL;
	}
	return 0;

release_floor:
	if (added_floor && old_uv <= VSRAM_NO_FLOOR_MAX_UV)
		mtk_pm_qos_remove_request(&vsram_high_floor);
out:
	return ret;
}

static ssize_t dvfsrc_vsram_test_uv_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	u32 uv;
	int ret;

	if (kstrtou32(buf, 10, &uv))
		return -EINVAL;

	mutex_lock(&top_vcore_lock);
	ret = dvfsrc->suspend_vsram_uv ? -EBUSY :
		dvfsrc_set_vsram(dvfsrc, uv ?: VSRAM_TEST_STOCK_UV);
	mutex_unlock(&top_vcore_lock);

	if (ret)
		return ret;

	return count;
}

/* PM prepare runs before tasks freeze. Post-suspend runs with the PMIC awake. */
static void dvfsrc_resume_gpu_rails(struct mtk_dvfsrc *dvfsrc)
{
	int ret;

	if (!dvfsrc->suspend_vsram_uv)
		return;

	ret = dvfsrc_set_vsram(dvfsrc, dvfsrc->suspend_vsram_uv);
	if (ret) {
		dev_err(dvfsrc->dev, "GPU resume VSRAM restore failed: %d\n", ret);
		goto stock;
	}

	ret = dvfsrc_set_top_vcore(dvfsrc, dvfsrc->suspend_vcore_uv);
	if (ret) {
		dev_err(dvfsrc->dev, "GPU resume VCORE restore failed: %d\n", ret);
		goto stock;
	}

	ret = mt6765_gpufreq_hold_stock_ceiling(false);
	if (!ret) {
		dvfsrc->gpu_rail_restore_failed = false;
		goto out;
	}
	dev_err(dvfsrc->dev, "GPU resume ceiling restore failed: %d\n", ret);

stock:
	dvfsrc->gpu_rail_restore_failed = true;
	if (dvfsrc_set_top_vcore(dvfsrc, TOP_VCORE_STOCK_UV) ||
	    dvfsrc_set_vsram(dvfsrc, VSRAM_TEST_STOCK_UV))
		dev_err(dvfsrc->dev, "GPU resume stock rail recovery failed\n");
out:
	dvfsrc->suspend_vsram_uv = 0;
}

static int dvfsrc_prepare_gpu_rails(struct mtk_dvfsrc *dvfsrc)
{
	u32 cmd;
	int core_uv, sram_uv, ret;

	ret = dvfsrc_top_vcore_read_cmd(dvfsrc, &cmd);
	if (ret)
		return vsram_override_active ? ret : 0;

	core_uv = dvfsrc_vcore_cmd_uv(cmd);
	if (!vsram_override_active && core_uv == TOP_VCORE_STOCK_UV)
		return 0;

	ret = dvfsrc_vsram_test_get_power(dvfsrc);
	if (ret)
		return ret;
	sram_uv = regulator_get_voltage(dvfsrc->vsram_test_power);
	if (sram_uv < VSRAM_TEST_STOCK_UV || sram_uv > VSRAM_TEST_MAX_UV)
		return -EIO;

	ret = mt6765_gpufreq_hold_stock_ceiling(true);
	if (ret) {
		if (!dvfsrc->gpu_rail_restore_failed)
			mt6765_gpufreq_hold_stock_ceiling(false);
		return ret;
	}

	dvfsrc->suspend_vcore_uv = core_uv;
	dvfsrc->suspend_vsram_uv = sram_uv;
	ret = dvfsrc_set_top_vcore(dvfsrc, TOP_VCORE_STOCK_UV);
	if (!ret)
		ret = dvfsrc_set_vsram(dvfsrc, VSRAM_TEST_STOCK_UV);
	if (ret)
		dvfsrc_resume_gpu_rails(dvfsrc);

	return ret;
}

static int dvfsrc_gpu_rail_pm_notify(struct notifier_block *nb,
		unsigned long event, void *unused)
{
	struct mtk_dvfsrc *dvfsrc;
	int ret = 0;

	dvfsrc = container_of(nb, struct mtk_dvfsrc, gpu_rail_pm_notifier);
	mutex_lock(&top_vcore_lock);
	if (event == PM_SUSPEND_PREPARE) {
		ret = dvfsrc_prepare_gpu_rails(dvfsrc);
		if (ret)
			dev_err(dvfsrc->dev, "GPU suspend stock rail preparation failed: %d\n",
				ret);
	} else if (event == PM_POST_SUSPEND) {
		dvfsrc_resume_gpu_rails(dvfsrc);
	}
	mutex_unlock(&top_vcore_lock);

	return notifier_from_errno(ret);
}

int dvfsrc_gpu_rail_pm_register(struct mtk_dvfsrc *dvfsrc)
{
	int ret;

	dvfsrc->gpu_rail_pm_notifier.notifier_call = dvfsrc_gpu_rail_pm_notify;
	ret = register_pm_notifier(&dvfsrc->gpu_rail_pm_notifier);
	if (ret)
		dvfsrc->gpu_rail_pm_notifier.notifier_call = NULL;

	return ret;
}

void dvfsrc_gpu_rail_pm_unregister(struct mtk_dvfsrc *dvfsrc)
{
	if (dvfsrc->gpu_rail_pm_notifier.notifier_call)
		unregister_pm_notifier(&dvfsrc->gpu_rail_pm_notifier);
}

int dvfsrc_vsram_test_suspend(struct device *dev)
{
	int ret = 0;

	mutex_lock(&top_vcore_lock);
	if (vsram_override_active)
		ret = -EBUSY;
	mutex_unlock(&top_vcore_lock);

	return ret;
}

static ssize_t dvfsrc_gpu_rail_pm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		IS_ENABLED(CONFIG_SUSPEND) &&
		dvfsrc->gpu_rail_pm_notifier.notifier_call != NULL);
}

static DEVICE_ATTR_RW(dvfsrc_vsram_test_uv);
static DEVICE_ATTR_RW(dvfsrc_top_vcore_uv);
static DEVICE_ATTR_RO(dvfsrc_top_vcore_max_uv);
static DEVICE_ATTR_RO(dvfsrc_gpu_rail_pm);

static struct attribute *dvfsrc_top_vcore_attrs[] = {
	&dev_attr_dvfsrc_top_vcore_uv.attr,
	&dev_attr_dvfsrc_top_vcore_max_uv.attr,
	&dev_attr_dvfsrc_vsram_test_uv.attr,
	&dev_attr_dvfsrc_gpu_rail_pm.attr,
	NULL,
};

const struct attribute_group dvfsrc_top_vcore_group = {
	.attrs = dvfsrc_top_vcore_attrs,
};
#endif /* CONFIG_MACH_MT6765 */

#if defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT6765)
#define MTK_SIP_VCOREFS_DVFS_HOPPING_STATE 20
static int dvfsrc_dvfs_hopping_status(void)
{
	struct arm_smccc_res ares;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
		MTK_SIP_VCOREFS_DVFS_HOPPING_STATE,
		0, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		return ares.a1;

	return 0;
}
#endif

#if defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT6765)
#define MTK_SIP_VCOREFS_GET_EFUSE 18
static int dvfsrc_dvfs_get_efuse_data(u32 idx)
{
	struct arm_smccc_res ares;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
		MTK_SIP_VCOREFS_GET_EFUSE,
		idx, 0, 0, 0, 0, 0,
		&ares);

	if (!ares.a0)
		return ares.a1;

	return 0;
}
#endif

static char *dvfsrc_dump_info(struct mtk_dvfsrc *dvfsrc,
	char *p, u32 size)
{
	int vcore_uv = 0;
	char *buff_end = p + size;

	if (dvfsrc->vcore_power)
		vcore_uv = regulator_get_voltage(dvfsrc->vcore_power);

	p += snprintf(p, buff_end - p, "%-10s: %-8u uv\n",
		"Vcore", vcore_uv);
#if IS_ENABLED(CONFIG_MTK_DRAMC_LEGACY)
	p += snprintf(p, buff_end - p, "%-10s: %-8u khz\n",
		"DDR", get_dram_data_rate() * 1000);
#endif
#if defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT6765)
	p += snprintf(p, buff_end - p, "%-10s: %d\n",
		"GPS_HOPPING", dvfsrc_dvfs_hopping_status());
#endif
#if defined(CONFIG_MACH_MT6761)
	p += snprintf(p, buff_end - p, "%-10s: %08x\n",
		"EFUSE_0", dvfsrc_dvfs_get_efuse_data(0));
#endif
#if defined(CONFIG_MACH_MT6765)
	p += snprintf(p, buff_end - p, "%-10s: %08x\n",
		"PTPOD_0", dvfsrc_dvfs_get_efuse_data(0));
	p += snprintf(p, buff_end - p, "%-10s: %08x\n",
		"PTPOD_10", dvfsrc_dvfs_get_efuse_data(1));
	p += snprintf(p, buff_end - p, "%-10s: %08x\n",
		"INFO2", dvfsrc_dvfs_get_efuse_data(2));
#endif

	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static char *dvfsrc_dump_record(struct mtk_dvfsrc *dvfsrc,
	char *p, u32 size)
{
	int i, rec_offset, offset;
	char *buff_end = p + size;

	p += sprintf(p, "%-17s: 0x%08x\n",
			"DVFSRC_LAST",
			dvfsrc_read(dvfsrc, DVFSRC_LAST, 0));

	rec_offset = 0xC;

	for (i = 0; i < 8; i++) {
		offset = i * rec_offset;
		p += snprintf(p, buff_end - p,
			"[%d]%-14s: %08x,%08x,%08x\n",
			i,
			"DVFSRC_REC 0~2",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x0),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x4),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x8));
	}
	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static char *dvfsrc_dump_reg(struct mtk_dvfsrc *dvfsrc, char *p, u32 size)
{
	char *buff_end = p + size;

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"BASIC_CONTROL",
		dvfsrc_read(dvfsrc, DVFSRC_BASIC_CONTROL, 0x0));
	p += snprintf(p, buff_end - p,
		"%-16s: %08x, %08x\n",
		"SW_REQ 1~2",
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x4));

	p += snprintf(p, buff_end - p, "%-16s: %d, %d, %d, %d, %d\n",
		"SW_BW_0~4",
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x4),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x8),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0xC),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x10));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"INT",
		dvfsrc_read(dvfsrc, DVFSRC_INT, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"INT_EN",
		dvfsrc_read(dvfsrc, DVFSRC_INT_EN, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"MD_SCENARIO",
		dvfsrc_read(dvfsrc, DVFSRC_MD_SCENARIO, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"MD_RSV0",
		dvfsrc_read(dvfsrc, DVFSRC_RSRV_0, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"SCP_VCORE_REQ",
		dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"CURRENT_LEVEL",
		dvfsrc_read(dvfsrc, DVFSRC_LEVEL, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"FORCE_OPP_IDX",
		dvfsrc->force_opp_idx);

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"CURR_DVFS_OPP",
		mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DVFS_OPP));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"CURR_VCORE_OPP",
		mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_OPP));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"CURR_DRAM_OPP",
		mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_OPP));

	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static char *dvfsrc_dump_spm_info(struct mtk_dvfsrc *dvfsrc,
	char *p, u32 size)
{
	char *buff_end = p + size;

	if (!dvfsrc->spm_regs)
		return p;

	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"POWERON_CONFIG_EN",
			spm_read(dvfsrc, SPM_POWERON_CONFIG_EN));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_SW_FLAG",
			spm_read(dvfsrc, SPM_SW_FLAG));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_SW_RSV_9",
			spm_read(dvfsrc, SPM_SW_RSV_9));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"MD2SPM_DVFS_CON",
			spm_read(dvfsrc, SPM_MD2SPM_DVFS_CON));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_DVFS_EVENT_STA",
			spm_read(dvfsrc, SPM_DVFS_EVENT_STA));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_DVFS_LEVEL",
			spm_read(dvfsrc, SPM_DVFS_LEVEL));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_DFS_LEVEL",
			spm_read(dvfsrc, SPM_DFS_LEVEL));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_DVS_LEVEL",
			spm_read(dvfsrc, SPM_DVS_LEVEL));
	p += snprintf(p, buff_end - p,
		"%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		"SPM_DVFS_CMD0~4",
		spm_read(dvfsrc, SPM_DVFS_CMD0),
		spm_read(dvfsrc, SPM_DVFS_CMD1),
		spm_read(dvfsrc, SPM_DVFS_CMD2),
		spm_read(dvfsrc, SPM_DVFS_CMD3),
		spm_read(dvfsrc, SPM_DVFS_CMD4));

	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"PCM_IM_PTR",
			spm_read(dvfsrc, SPM_PCM_IM_PTR));
	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static int dvfsrc_query_request_status(struct mtk_dvfsrc *dvfsrc, u32 id)
{
	return 0;
}

static void dvfsrc_force_opp(struct mtk_dvfsrc *dvfsrc, u32 opp)
{
#ifdef CONFIG_MACH_MT6765
	mutex_lock(&top_vcore_lock);
	if (vsram_override_active || dvfsrc->suspend_vsram_uv) {
		dev_warn(dvfsrc->dev, "cannot force DVFSRC while VSRAM override is active\n");
		mutex_unlock(&top_vcore_lock);
		return;
	}
#endif
	dvfsrc->force_opp_idx = opp;
	mtk_dvfsrc_send_request(dvfsrc->dev->parent,
		MTK_DVFSRC_CMD_FORCE_OPP_REQUEST,
		opp);
#ifdef CONFIG_MACH_MT6765
	mutex_unlock(&top_vcore_lock);
#endif
}

const struct dvfsrc_config mt6761_dvfsrc_config = {
	.regs = mt6761_regs,
	.spm_regs = mt6761_spm_regs,
	.dump_info = dvfsrc_dump_info,
	.dump_record = dvfsrc_dump_record,
	.dump_reg = dvfsrc_dump_reg,
	.dump_spm_info = dvfsrc_dump_spm_info,
	.force_opp = dvfsrc_force_opp,
	.query_request = dvfsrc_query_request_status,
};

