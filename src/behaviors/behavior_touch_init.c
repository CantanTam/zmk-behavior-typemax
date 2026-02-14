#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/settings/settings.h>
#include <zephyr/pm/device.h>  // 新增：用于调用电源管理

// 定义全局变量，供其他文件使用
uint8_t use_touch = 1; 

/**
 * 当系统从 Flash 读取到 zmk/togtouch/val 时触发
 */
static int togtouch_settings_set(const char *name, size_t len,
                                 settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    if (settings_name_steq(name, "val", &next) && !next) {
        if (len == sizeof(use_touch)) {
            return read_cb(cb_arg, &use_touch, sizeof(use_touch));
        }
    }
    return -ENOENT;
}

struct settings_handler togtouch_conf = {
    .name = "zmk/togtouch",
    .h_set = togtouch_settings_set
};

static int behavior_touch_init_sys(void) {
    // 1. 注册处理器
    settings_register(&togtouch_conf);
    
    // 2. 强制加载一次
    settings_load_subtree("zmk/togtouch");

    // 3. 【新增关键步骤】：根据加载出的 use_touch 值，强制更新电源引脚状态
    const struct device *pwr_dev = device_get_binding("XW12A_PWR");
    if (pwr_dev) {
        pm_device_action_run(pwr_dev,(use_touch == 1 ? PM_DEVICE_ACTION_RESUME : PM_DEVICE_ACTION_SUSPEND));
    }
    
    return 0;
}

// 优先级 99 确保在设置系统之后启动 (模仿 tolayer)
SYS_INIT(behavior_touch_init_sys, APPLICATION, 99);