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
#define TOP_VCORE_MAX_UV	875000
#define TOP_VCORE_CMD		0
#define TOP_VCORE_PMIC_REG	((u32)MT6357_RG_BUCK_VCORE_VOSEL_ADDR << 16)
#define TOP_VCORE_SELECTOR(uv)	(((uv) - TOP_VCORE_BASE_UV) / TOP_VCORE_STEP_UV)

#define VSRAM_TEST_STOCK_UV	900000
#define VSRAM_TEST_MAX_UV	975000
#define VSRAM_TEST_FLOOR_UV	700000
#define VSRAM_TEST_HEADROOM_UV	75000
#define VSRAM_TEST_MAX_GAP_UV	250000

static DEFINE_MUTEX(top_vcore_lock);
static struct mtk_pm_qos_request vsram_test_floor;
static int vsram_test_floor_opp = VCORE_OPP_UNREQ;

static int dvfsrc_top_vcore_read_cmd(struct mtk_dvfsrc *dvfsrc, u32 *cmd)
{
	if (!dvfsrc->spm_regs || !dvfsrc->vcore_power ||
	    mtk_dvfsrc_vcore_opp_count() != 4 ||
	    mtk_dvfsrc_vcore_uv_table(0) != TOP_VCORE_STOCK_UV)
		return -EOPNOTSUPP;

	/* Confirm the firmware's VCORE command layout before using slot 0. */
	if (spm_read(dvfsrc, SPM_DVFS_CMD1) != (TOP_VCORE_PMIC_REG | 0x25) ||
	    spm_read(dvfsrc, SPM_DVFS_CMD2) != (TOP_VCORE_PMIC_REG | 0x1d) ||
	    spm_read(dvfsrc, SPM_DVFS_CMD3) != (TOP_VCORE_PMIC_REG | 0x19) ||
	    spm_read(dvfsrc, SPM_DVFS_CMD4) != (TOP_VCORE_PMIC_REG | 0x15))
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
				TOP_VCORE_BASE_UV + (cmd & 0x7f) * TOP_VCORE_STEP_UV,
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

	return min(vsram_uv - VSRAM_TEST_HEADROOM_UV, TOP_VCORE_MAX_UV);
}

static ssize_t dvfsrc_top_vcore_uv_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	u32 uv, selector, old_cmd, new_cmd;
	int ret;
	long status;

	if (kstrtou32(buf, 10, &uv))
		return -EINVAL;
	if (!uv)
		uv = TOP_VCORE_STOCK_UV;
	if (uv < TOP_VCORE_STOCK_UV || uv > TOP_VCORE_MAX_UV ||
	    (uv - TOP_VCORE_BASE_UV) % TOP_VCORE_STEP_UV)
		return -EINVAL;

	selector = TOP_VCORE_SELECTOR(uv);

	mutex_lock(&top_vcore_lock);
	ret = dvfsrc_top_vcore_read_cmd(dvfsrc, &old_cmd);
	if (ret)
		goto out;

	if (uv > TOP_VCORE_STOCK_UV && (int)uv > dvfsrc_top_vcore_limit(dvfsrc)) {
		ret = -ERANGE;
		goto out;
	}
	if (old_cmd == (TOP_VCORE_PMIC_REG | selector))
		goto out;

	status = mt6765_spm_write_pwrap_cmd(TOP_VCORE_CMD, selector);
	ret = dvfsrc_top_vcore_read_cmd(dvfsrc, &new_cmd);
	if (status || ret || new_cmd != (TOP_VCORE_PMIC_REG | selector)) {
		if (spm_read(dvfsrc, SPM_DVFS_CMD0) != old_cmd) {
			mt6765_spm_write_pwrap_cmd(TOP_VCORE_CMD, old_cmd & 0x7f);
			if (spm_read(dvfsrc, SPM_DVFS_CMD0) != old_cmd)
				dev_err(dev, "failed to restore VCORE command slot 0\n");
		}
		dev_warn(dev, "top VCORE update failed, firmware status %ld\n", status);
		ret = status == SIP_SVC_E_NOT_SUPPORTED ? -EOPNOTSUPP : -EIO;
	}

out:
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

static int dvfsrc_vsram_test_floor_for_voltage(int uv)
{
	return uv > VSRAM_TEST_FLOOR_UV + VSRAM_TEST_MAX_GAP_UV ?
		VCORE_OPP_0 : VCORE_OPP_1;
}

static void dvfsrc_vsram_test_set_floor(int opp)
{
	if (opp == vsram_test_floor_opp)
		return;

	if (mtk_pm_qos_request_active(&vsram_test_floor))
		mtk_pm_qos_update_request(&vsram_test_floor, opp);
	else
		mtk_pm_qos_add_request(&vsram_test_floor, MTK_PM_QOS_VCORE_OPP, opp);
	vsram_test_floor_opp = opp;
}

static void dvfsrc_vsram_test_release(struct mtk_dvfsrc *dvfsrc)
{
	mtk_pm_qos_remove_request(&vsram_test_floor);
	vsram_test_floor_opp = VCORE_OPP_UNREQ;
	devm_regulator_put(dvfsrc->vsram_test_power);
	dvfsrc->vsram_test_power = NULL;
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
				mtk_pm_qos_request_active(&vsram_test_floor),
				VSRAM_TEST_MAX_UV, VSRAM_TEST_HEADROOM_UV);
	mutex_unlock(&top_vcore_lock);

	return ret;
}

static ssize_t dvfsrc_vsram_test_uv_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	u32 uv, cmd;
	int old_uv, programmed_uv, live_uv, levels, ret;
	int old_floor, new_floor, transition_floor;

	if (kstrtou32(buf, 10, &uv))
		return -EINVAL;
	if (!uv)
		uv = VSRAM_TEST_STOCK_UV;
	if (uv < VSRAM_TEST_STOCK_UV || uv > VSRAM_TEST_MAX_UV ||
	    (uv - VSRAM_TEST_STOCK_UV) % TOP_VCORE_STEP_UV)
		return -EINVAL;

	mutex_lock(&top_vcore_lock);
	ret = dvfsrc_top_vcore_read_cmd(dvfsrc, &cmd);
	if (ret)
		goto out;
	ret = dvfsrc_vsram_test_get_power(dvfsrc);
	if (ret)
		goto out;

	old_floor = vsram_test_floor_opp;
	old_uv = regulator_get_voltage(dvfsrc->vsram_test_power);
	if (old_uv < VSRAM_TEST_STOCK_UV || old_uv > VSRAM_TEST_MAX_UV ||
	    (old_floor == VCORE_OPP_UNREQ && old_uv != VSRAM_TEST_STOCK_UV)) {
		ret = -EOPNOTSUPP;
		goto out;
	}
	if (uv == VSRAM_TEST_STOCK_UV && old_floor == VCORE_OPP_UNREQ)
		goto out;

	programmed_uv = TOP_VCORE_BASE_UV + (cmd & 0x7f) * TOP_VCORE_STEP_UV;
	if (programmed_uv > min_t(int, old_uv, uv) - VSRAM_TEST_HEADROOM_UV) {
		ret = -ERANGE;
		goto out;
	}

	levels = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_NUM_DVFS_OPP);
	if (levels <= 0 || dvfsrc->force_opp_idx < (u32)levels) {
		ret = -EBUSY;
		goto out;
	}

	new_floor = dvfsrc_vsram_test_floor_for_voltage(uv);
	/* Raise the floor before raising SRAM. Relax it after SRAM falls. */
	transition_floor = min3(old_floor, new_floor,
				dvfsrc_vsram_test_floor_for_voltage(old_uv));
	dvfsrc_vsram_test_set_floor(transition_floor);

	live_uv = regulator_get_voltage(dvfsrc->vcore_power);
	if (mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_OPP) > transition_floor ||
	    live_uv < VSRAM_TEST_FLOOR_UV ||
	    live_uv < max_t(int, old_uv, uv) - VSRAM_TEST_MAX_GAP_UV ||
	    live_uv > min_t(int, old_uv, uv) - VSRAM_TEST_HEADROOM_UV) {
		ret = -EAGAIN;
		goto restore_floor;
	}

	ret = regulator_set_voltage(dvfsrc->vsram_test_power, uv, uv);
	if (ret || regulator_get_voltage(dvfsrc->vsram_test_power) != (int)uv) {
		if (regulator_get_voltage(dvfsrc->vsram_test_power) != old_uv)
			regulator_set_voltage(dvfsrc->vsram_test_power, old_uv, old_uv);
		if (regulator_get_voltage(dvfsrc->vsram_test_power) != old_uv) {
			/* Retain the floor and block suspend after a failed rollback. */
			dev_err(dev, "VSRAM test could not restore %d uV\n", old_uv);
			ret = -EIO;
			goto out;
		}
		if (!ret)
			ret = -EIO;
		goto restore_floor;
	}

	if (uv == VSRAM_TEST_STOCK_UV)
		dvfsrc_vsram_test_release(dvfsrc);
	else
		dvfsrc_vsram_test_set_floor(new_floor);
	goto out;

restore_floor:
	if (old_floor == VCORE_OPP_UNREQ)
		dvfsrc_vsram_test_release(dvfsrc);
	else
		dvfsrc_vsram_test_set_floor(old_floor);
out:
	mutex_unlock(&top_vcore_lock);
	if (ret)
		return ret;

	return count;
}

int dvfsrc_vsram_test_suspend(struct device *dev)
{
	int ret = 0;

	mutex_lock(&top_vcore_lock);
	if (mtk_pm_qos_request_active(&vsram_test_floor))
		ret = -EBUSY;
	mutex_unlock(&top_vcore_lock);

	return ret;
}

static DEVICE_ATTR_RW(dvfsrc_vsram_test_uv);
static DEVICE_ATTR_RW(dvfsrc_top_vcore_uv);
static DEVICE_ATTR_RO(dvfsrc_top_vcore_max_uv);

static struct attribute *dvfsrc_top_vcore_attrs[] = {
	&dev_attr_dvfsrc_top_vcore_uv.attr,
	&dev_attr_dvfsrc_top_vcore_max_uv.attr,
	&dev_attr_dvfsrc_vsram_test_uv.attr,
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
	if (mtk_pm_qos_request_active(&vsram_test_floor)) {
		dev_warn(dvfsrc->dev, "cannot force DVFSRC while VSRAM test is active\n");
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

