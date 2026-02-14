#define DT_DRV_COMPAT zmk_behavior_togtouch

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/settings/settings.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>

extern void ws2812_power_off(void);
extern uint8_t use_touch; // 引用外部变量

#define PIN_TOUCH_ON   14  
#define PIN_TOUCH_OFF  16 
#define HOLD_TIME_MS   3000

static const struct device *gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static struct k_work_delayable led_work;
static int active_pin = -1;

static void led_work_handler(struct k_work *work) {
    if (!device_is_ready(gpio0_dev) || active_pin < 0) return;
    gpio_pin_configure(gpio0_dev, active_pin, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_LOW);
    k_msleep(HOLD_TIME_MS);
    gpio_pin_configure(gpio0_dev, active_pin, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_LOW);
    active_pin = -1; 
}

static int on_togtouch_binding_pressed(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    // 1. 状态翻转
    use_touch = (use_touch == 1) ? 0 : 1;

    // 2. 电源控制
    if (use_touch == 0) {
        ws2812_power_off();
    }
    
    // 3. LED 反馈
    active_pin = (use_touch == 1) ? PIN_TOUCH_ON : PIN_TOUCH_OFF;
    k_work_schedule(&led_work, K_NO_WAIT);

    // 4. 写入 Flash (完全模仿 reclayer 的路径格式)
    int ret;
    for (int i = 0; i < 3; i++) {
        ret = settings_save_one("zmk/togtouch/val", &use_touch, sizeof(use_touch));
        if (ret == 0) break;
        k_msleep(20);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_togtouch_driver_api = {
    .binding_pressed = on_togtouch_binding_pressed,
};

static int behavior_togtouch_init(const struct device *dev) {
    k_work_init_delayable(&led_work, led_work_handler);
    return 0;
}

BEHAVIOR_DT_INST_DEFINE(0, behavior_togtouch_init, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_togtouch_driver_api);