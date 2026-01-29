#define DT_DRV_COMPAT zmk_behavior_togtouch

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/settings/settings.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>

/* --- 配置区 --- */
#define PIN_TOUCH_ON   14  
#define PIN_TOUCH_OFF  16 
#define HOLD_TIME_MS   3000
/* -------------- */

static const struct device *gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
bool use_touch = true;
static struct k_work_delayable led_work;
static int active_pin = -1;

/* --- Settings 存储逻辑 --- */
static int togtouch_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    if (settings_name_steq(name, "use_touch", &next) && !next) {
        if (len != sizeof(use_touch)) return -EINVAL;
        read_cb(cb_arg, &use_touch, sizeof(use_touch));
    }
    return 0;
}

struct settings_handler togtouch_conf = {
    .name = "zmk/togtouch",
    .h_set = togtouch_settings_set
};

/* --- 修复后的异步 LED 处理逻辑 --- */
static void led_work_handler(struct k_work *work) {
    if (!device_is_ready(gpio0_dev) || active_pin < 0) return;

    // 1. 配置为输出并拉低 (GPIO_ACTIVE_LOW 模式下，ACTIVE 对应低电平点亮)
    // 强制重新配置为输出，确保接管控制权
    gpio_pin_configure(gpio0_dev, active_pin, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_LOW);
    
    // 2. 维持 3 秒
    k_msleep(HOLD_TIME_MS);

    // 3. 【关键修复点】：恢复为“输出模式且不活跃”
    // 不要使用 GPIO_INPUT！
    // 设为 INACTIVE 且保留 OUTPUT 模式，这样其他 behavior (如 blink) 之后调用时引脚依然是输出状态
    gpio_pin_configure(gpio0_dev, active_pin, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_LOW);
    
    active_pin = -1; 
}

/* --- Behavior 核心回调 --- */
static int on_togtouch_binding_pressed(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    use_touch = !use_touch;
    active_pin = use_touch ? PIN_TOUCH_ON : PIN_TOUCH_OFF;

    k_work_schedule(&led_work, K_NO_WAIT);
    settings_save_one("zmk/togtouch/use_touch", &use_touch, sizeof(use_touch));

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_togtouch_driver_api = {
    .binding_pressed = on_togtouch_binding_pressed,
};

static int behavior_togtouch_init(const struct device *dev) {
    k_work_init_delayable(&led_work, led_work_handler);
    int rc = settings_register(&togtouch_conf);
    if (rc == 0) {
        settings_load_subtree("zmk/togtouch");
    }
    return 0;
}

BEHAVIOR_DT_INST_DEFINE(
    0,
    behavior_togtouch_init,
    NULL,
    NULL,
    NULL,
    POST_KERNEL,
    CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
    &behavior_togtouch_driver_api
);