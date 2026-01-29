#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zmk/battery.h>
#include <zmk/endpoints.h>

#if IS_ENABLED(CONFIG_BATTERY_ALERT)

/* --- 自定义配置区 --- */
#define BATTERY_ALERT_CYCLE_COUNT    5     
#define TIME_LOW_MS                  1000  
#define TIME_HIGH_MS                 1000  // 统一名称：高电平（灭灯）持续时间
#define ALERT_PIN                    16    
/* -------------------- */

static const struct device *gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static struct k_work_delayable alert_work;

void perform_battery_alert_action(void) {
    if (!device_is_ready(gpio0_dev)) return;

    for (int i = 0; i < BATTERY_ALERT_CYCLE_COUNT; i++) {
        // 1. 配置并拉低 (Active = 0V = 亮灯)
        // 每次循环都配置，确保即便被 blink 抢走也能夺回来
        gpio_pin_configure(gpio0_dev, ALERT_PIN, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_LOW);
        k_msleep(TIME_LOW_MS);

        // 2. 配置并拉高 (Inactive = VCC = 灭灯)
        // 使用 INACTIVE 保持输出驱动能力
        gpio_pin_configure(gpio0_dev, ALERT_PIN, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_LOW);
        k_msleep(TIME_HIGH_MS);
    }
    
    // 结束后，引脚保留在 GPIO_OUTPUT_INACTIVE 状态
    // 这是一个标准的“输出口空闲”状态，blink 驱动可以直接通过 gpio_pin_set 操作它
}

/* --- 后续逻辑保持不变 --- */

static void alert_work_handler(struct k_work *work) {
    uint8_t level = zmk_battery_state_of_charge();
    if (level > 0 && level < CONFIG_BATTERY_LEVEL) {
        perform_battery_alert_action();
    }
}

static int battery_alert_init(void) {
    k_work_init_delayable(&alert_work, alert_work_handler);
    k_work_schedule(&alert_work, K_SECONDS(5));
    return 0;
}

SYS_INIT(battery_alert_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif