// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/jiffies.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/string.h>

#include <linux/ryzen_smu.h>

#define DRVNAME "ryzen_smu_hwmon"
#define PM_BUF_MAX 0x2000

/* =======================
 * Channel layout
 * ======================= */
#define TEMP_CH_TCTL    0
#define TEMP_CH_CCD1    1
#define TEMP_CH_CCD2    2
#define TEMP_CH_CORE0   3

#define POWER_CH_PKG    0
#define POWER_CH_SOC    1
#define POWER_CH_CORE0  2

#define IN_CH_VCORE     0
#define IN_CH_VSOC      1

/* =======================
 * SMU PM table layout
 * ======================= */
struct smu_pm_layout {
    /* CPUID match */
    u32 family;
    u32 model;
    u32 stepping;
    u32 pkg_type;

    /* topology */
    u32 max_cores;
    u32 max_ccd;
    u32 stride;

    /* offsets */
    u32 off_tctl;
    u32 off_ccd_block;
    u32 off_core_temp_base;
    u32 off_core_power_base;

    u32 off_pkg_power;
    u32 off_soc_power;

    u32 off_vcore;
    u32 off_vsoc;
};

/* Raphael / Ryzen 9 7950X3D */
static const struct smu_pm_layout smu_layout_raphael_7950x3d = {
    .family   = 0x19,
    .model    = 0x61,
    .stepping = 0x2,
    .pkg_type = 0x0,

    .max_cores = 16,
    .max_ccd   = 2,
    .stride    = 4,

    .off_tctl            = 0x0454,
    .off_ccd_block       = 0x0534,
    .off_core_temp_base  = 0x0514,
    .off_core_power_base = 0x0554,

    .off_pkg_power = 0x00DC,
    .off_soc_power = 0x00D8,

    .off_vcore = 0x0048,
    .off_vsoc  = 0x0148,
};

/* Active layout */
static const struct smu_pm_layout *smu_layout;

/* =========================================================
 * IEEE754 float32 → scaled integer (NO FP in kernel)
 * ========================================================= */
static long smu_read_f32_scaled(const u8 *buf, u32 off, s64 scale)
{
    u32 raw;
    s32 exp;
    u32 mant;
    bool sign;
    s32 shift;
    s64 m, out;

    memcpy(&raw, buf + off, sizeof(raw));

    sign = !!(raw & 0x80000000);
    raw &= 0x7fffffff;

    if (!raw)
        return 0;

    if ((raw & 0x7f800000) == 0x7f800000)
        return 0;

    exp  = ((raw >> 23) & 0xff) - 127;
    mant = (raw & 0x7fffff) | 0x800000;

    shift = exp - 23;
    m = (s64)mant * scale;

    if (shift >= 0) {
        if (shift > 31)
            out = 0;
        else
            out = m << shift;
    } else {
        s32 r = -shift;
        if (r > 62)
            out = 0;
        else
            out = (m + (1LL << (r - 1))) >> r;
    }

    return sign ? -(long)out : (long)out;
}

static inline long smu_f32_mC(const u8 *b, u32 o)
{
    return smu_read_f32_scaled(b, o, 1000);
}

static inline long smu_f32_uW(const u8 *b, u32 o)
{
    return smu_read_f32_scaled(b, o, 1000000);
}

static inline long smu_f32_mV(const u8 *b, u32 o)
{
    return smu_read_f32_scaled(b, o, 1000);
}

/* =======================
 * Driver context
 * ======================= */
struct smu_hwmon_ctx {
    struct device *hwmon_dev;
    struct mutex lock;
    unsigned long last_update;
    bool valid;
    u8 pm_buf[PM_BUF_MAX];
};

static struct smu_hwmon_ctx *g_ctx;
static unsigned int update_ms = 500;
module_param(update_ms, uint, 0644);

/* =======================
 * PM table refresh
 * ======================= */
static int smu_refresh_locked(struct smu_hwmon_ctx *ctx)
{
    unsigned long min = msecs_to_jiffies(update_ms);
    size_t len = sizeof(ctx->pm_buf);
    int ret;

    if (ctx->valid && time_before(jiffies, ctx->last_update + min))
        return 0;

    ret = ryzen_smu_read_pm_table(ctx->pm_buf, &len);
    if (ret < 0) {
        ctx->valid = false;
        return ret;
    }

    ctx->last_update = jiffies;
    ctx->valid = true;
    return 0;
}

/* =======================
 * hwmon read
 * ======================= */
static int smu_hwmon_read(struct device *dev,
                          enum hwmon_sensor_types type,
                          u32 attr, int channel, long *val)
{
    struct smu_hwmon_ctx *ctx = dev_get_drvdata(dev);
    int ret = -EOPNOTSUPP;

    mutex_lock(&ctx->lock);
    if (smu_refresh_locked(ctx))
        goto out;

    switch (type) {

        case hwmon_temp:
            if (attr != hwmon_temp_input)
                break;

        if (channel == TEMP_CH_TCTL) {
            *val = smu_f32_mC(ctx->pm_buf, smu_layout->off_tctl);
            ret = 0;
        } else if (channel == TEMP_CH_CCD1) {
            *val = smu_f32_mC(ctx->pm_buf,
                              smu_layout->off_ccd_block +
                              0 * smu_layout->stride);
            ret = 0;
        } else if (channel == TEMP_CH_CCD2 &&
            smu_layout->max_ccd > 1) {
            *val = smu_f32_mC(ctx->pm_buf,
                              smu_layout->off_ccd_block +
                              1 * smu_layout->stride);
            ret = 0;
            } else if (channel >= TEMP_CH_CORE0) {
                int core = channel - TEMP_CH_CORE0;
                if (core >= smu_layout->max_cores)
                    break;
                *val = smu_f32_mC(ctx->pm_buf,
                                  smu_layout->off_core_temp_base +
                                  core * smu_layout->stride);
                ret = 0;
            }
            break;

        case hwmon_power:
            if (attr != hwmon_power_input)
                break;

        if (channel == POWER_CH_PKG) {
            *val = smu_f32_uW(ctx->pm_buf, smu_layout->off_pkg_power);
            ret = 0;
        } else if (channel == POWER_CH_SOC) {
            *val = smu_f32_uW(ctx->pm_buf, smu_layout->off_soc_power);
            ret = 0;
        } else if (channel >= POWER_CH_CORE0) {
            int core = channel - POWER_CH_CORE0;
            if (core >= smu_layout->max_cores)
                break;
            *val = smu_f32_uW(ctx->pm_buf,
                              smu_layout->off_core_power_base +
                              core * smu_layout->stride);
            ret = 0;
        }
        break;

        case hwmon_in:
            if (attr != hwmon_in_input)
                break;

        if (channel == IN_CH_VCORE) {
            *val = smu_f32_mV(ctx->pm_buf, smu_layout->off_vcore);
            ret = 0;
        } else if (channel == IN_CH_VSOC) {
            *val = smu_f32_mV(ctx->pm_buf, smu_layout->off_vsoc);
            ret = 0;
        }
        break;

        default:
            break;
    }

    out:
    mutex_unlock(&ctx->lock);
    return ret;
}

/* =======================
 * visibility
 * ======================= */
static umode_t smu_hwmon_is_visible(const void *data,
                                    enum hwmon_sensor_types type,
                                    u32 attr, int channel)
{
    switch (type) {
        case hwmon_temp:
            if (attr == hwmon_temp_input || attr == hwmon_temp_label)
                return 0444;
        break;
        case hwmon_power:
            if (attr == hwmon_power_input || attr == hwmon_power_label)
                return 0444;
        break;
        case hwmon_in:
            if (attr == hwmon_in_input || attr == hwmon_in_label)
                return 0444;
        break;
        default:
            break;
    }
    return 0;
}

/* =======================
 * labels
 * ======================= */
static int smu_hwmon_read_string(struct device *dev,
                                 enum hwmon_sensor_types type,
                                 u32 attr, int channel,
                                 const char **str)
{
    static char core_temp_label[16][16];
    static char core_pwr_label[16][16];
    static bool inited;
    int i;

    if (!inited) {
        for (i = 0; i < smu_layout->max_cores; i++) {
            snprintf(core_temp_label[i], sizeof(core_temp_label[i]),
                     "Core%d Temp", i);
            snprintf(core_pwr_label[i], sizeof(core_pwr_label[i]),
                     "Core%d Power", i);
        }
        inited = true;
    }

    if (type == hwmon_temp && attr == hwmon_temp_label) {
        if (channel == TEMP_CH_TCTL) { *str = "Tctl"; return 0; }
        if (channel == TEMP_CH_CCD1) { *str = "Tccd1"; return 0; }
        if (channel == TEMP_CH_CCD2 && smu_layout->max_ccd > 1) {
            *str = "Tccd2"; return 0;
        }
        if (channel >= TEMP_CH_CORE0) {
            int core = channel - TEMP_CH_CORE0;
            if (core >= smu_layout->max_cores)
                return -EOPNOTSUPP;
            *str = core_temp_label[core];
            return 0;
        }
    }

    if (type == hwmon_power && attr == hwmon_power_label) {
        if (channel == POWER_CH_PKG) { *str = "Package Power"; return 0; }
        if (channel == POWER_CH_SOC) { *str = "SoC Power"; return 0; }
        if (channel >= POWER_CH_CORE0) {
            int core = channel - POWER_CH_CORE0;
            if (core >= smu_layout->max_cores)
                return -EOPNOTSUPP;
            *str = core_pwr_label[core];
            return 0;
        }
    }

    if (type == hwmon_in && attr == hwmon_in_label) {
        if (channel == IN_CH_VCORE) { *str = "Vcore"; return 0; }
        if (channel == IN_CH_VSOC)  { *str = "VSOC";  return 0; }
    }

    return -EOPNOTSUPP;
}

/* =======================
 * hwmon description
 * ======================= */
static const struct hwmon_ops smu_hwmon_ops = {
    .is_visible  = smu_hwmon_is_visible,
    .read        = smu_hwmon_read,
    .read_string = smu_hwmon_read_string,
};

static const struct hwmon_channel_info *smu_hwmon_info[] = {
    HWMON_CHANNEL_INFO(temp,
                       HWMON_T_INPUT | HWMON_T_LABEL,
                       HWMON_T_INPUT | HWMON_T_LABEL,
                       HWMON_T_INPUT | HWMON_T_LABEL,
                       HWMON_T_INPUT | HWMON_T_LABEL, HWMON_T_INPUT | HWMON_T_LABEL,
                       HWMON_T_INPUT | HWMON_T_LABEL, HWMON_T_INPUT | HWMON_T_LABEL,
                       HWMON_T_INPUT | HWMON_T_LABEL, HWMON_T_INPUT | HWMON_T_LABEL,
                       HWMON_T_INPUT | HWMON_T_LABEL, HWMON_T_INPUT | HWMON_T_LABEL,
                       HWMON_T_INPUT | HWMON_T_LABEL, HWMON_T_INPUT | HWMON_T_LABEL,
                       HWMON_T_INPUT | HWMON_T_LABEL, HWMON_T_INPUT | HWMON_T_LABEL,
                       HWMON_T_INPUT | HWMON_T_LABEL, HWMON_T_INPUT | HWMON_T_LABEL
    ),
    HWMON_CHANNEL_INFO(power,
                       HWMON_P_INPUT | HWMON_P_LABEL,
                       HWMON_P_INPUT | HWMON_P_LABEL,
                       HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL,
                       HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL,
                       HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL,
                       HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL,
                       HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL,
                       HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL,
                       HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL,
                       HWMON_P_INPUT | HWMON_P_LABEL, HWMON_P_INPUT | HWMON_P_LABEL
    ),
    HWMON_CHANNEL_INFO(in,
                       HWMON_I_INPUT | HWMON_I_LABEL,
                       HWMON_I_INPUT | HWMON_I_LABEL
    ),
    NULL
};

static const struct hwmon_chip_info smu_chip_info = {
    .ops  = &smu_hwmon_ops,
    .info = smu_hwmon_info,
};

/* =======================
 * init / exit
 * ======================= */
static int __init smu_hwmon_init(void)
{
    struct smu_hwmon_ctx *ctx;
    struct device *parent;
    size_t len;
    int ret;

    u32 fam  = ryzen_smu_get_cpu_family();
    u32 mod  = ryzen_smu_get_cpu_model();
    u32 step = ryzen_smu_get_cpu_stepping();
    u32 pkg  = ryzen_smu_get_cpu_package_type();

    if (fam == smu_layout_raphael_7950x3d.family &&
        mod == smu_layout_raphael_7950x3d.model &&
        step == smu_layout_raphael_7950x3d.stepping &&
        pkg == smu_layout_raphael_7950x3d.pkg_type) {

        smu_layout = &smu_layout_raphael_7950x3d;

        } else {
            pr_err(DRVNAME ": unsupported CPU "
            "(family=0x%x model=0x%x stepping=0x%x pkg=0x%x)\n",
                   fam, mod, step, pkg);
            return -ENODEV;
        }

        parent = ryzen_smu_get_device();
        if (!parent)
            return -ENODEV;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    mutex_init(&ctx->lock);

    len = sizeof(ctx->pm_buf);
    ret = ryzen_smu_read_pm_table(ctx->pm_buf, &len);
    if (ret < 0) {
        kfree(ctx);
        return ret;
    }

    ctx->valid = true;
    ctx->last_update = jiffies;

    ctx->hwmon_dev = hwmon_device_register_with_info(
        parent, DRVNAME, ctx, &smu_chip_info, NULL);

    if (IS_ERR(ctx->hwmon_dev)) {
        ret = PTR_ERR(ctx->hwmon_dev);
        kfree(ctx);
        return ret;
    }

    g_ctx = ctx;
    pr_info(DRVNAME ": found supported CPU, driver loaded\n");
    return 0;
}

static void __exit smu_hwmon_exit(void)
{
    if (!g_ctx)
        return;

    hwmon_device_unregister(g_ctx->hwmon_dev);
    kfree(g_ctx);
    pr_info(DRVNAME ": unloaded\n");
}

module_init(smu_hwmon_init);
module_exit(smu_hwmon_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You");
MODULE_DESCRIPTION("Ryzen SMU hwmon driver (CPUID-gated, per-core temps & power)");
