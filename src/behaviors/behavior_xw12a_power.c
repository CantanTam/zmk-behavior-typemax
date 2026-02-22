#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

// 1. 定义引脚和端口 (去掉 INT_PIN，不再干涉它)
#define PWR_PIN  10
#define INT_PIN  6
#define PWR_PORT DT_NODELABEL(gpio1)

// 外部状态变量
extern uint8_t use_touch;
extern bool pad_action_statu;

static const struct device *gpio_dev;
static struct k_work_delayable xw12a_reset_work;

// --- 核心逻辑：强制复位函数 ---
static void xw12a_reset_handler(struct k_work *work) {
    static int stage = 0;

    switch (stage) {
        case 0: // 第一步：断电
            if (pad_action_statu) {
                k_work_reschedule(&xw12a_reset_work, K_SECONDS(5));
                return;
            }
            gpio_pin_set(gpio_dev, PWR_PIN, 1);
            stage = 1;
            // 100ms 后回来执行第二步
            k_work_reschedule(&xw12a_reset_work, K_MSEC(100));
            break;

        case 1: // 第二步：通电
            gpio_pin_set(gpio_dev, PWR_PIN, 0);
            stage = 2;
            // 450ms 后回来执行第三步
            k_work_reschedule(&xw12a_reset_work, K_MSEC(450));
            break;

        case 2: // 第三步：完成并重新开启长周期
            stage = 0;
            k_work_reschedule(&xw12a_reset_work, K_SECONDS(90)); // 默认值是 60
            break;
    }
}

static int xw12a_pwr_init(const struct device *dev) {
    gpio_dev = DEVICE_DT_GET(PWR_PORT);
    if (!device_is_ready(gpio_dev)) {
        return -ENODEV;
    }

    gpio_pin_configure(gpio_dev, PWR_PIN, (use_touch == 1 ? GPIO_OUTPUT_LOW : GPIO_OUTPUT_HIGH));
    k_work_init_delayable(&xw12a_reset_work, xw12a_reset_handler);

    if (use_touch == 1) {
        k_work_schedule(&xw12a_reset_work, K_SECONDS(60));
    }

    return 0;
}

static int xw12a_pwr_pm_action(const struct device *dev, enum pm_device_action action) {
    switch (action) {
        case PM_DEVICE_ACTION_SUSPEND:
            k_work_cancel_delayable(&xw12a_reset_work);
            gpio_pin_set(gpio_dev, PWR_PIN, 1);

            #ifdef CONFIG_MCU_INT_SLEEP
            gpio_pin_configure(gpio_dev, INT_PIN, GPIO_DISCONNECTED);
            #endif
            
            return 0;

        case PM_DEVICE_ACTION_RESUME:
            // 1. 先恢复物理引脚状态（输入 + 上拉）
            #ifdef CONFIG_MCU_INT_SLEEP
            gpio_pin_configure(gpio_dev, INT_PIN, GPIO_INPUT | GPIO_PULL_UP);
            // 2. 【最关键的一步】重新绑定双边沿触发中断！
            gpio_pin_interrupt_configure(gpio_dev, INT_PIN, GPIO_INT_EDGE_BOTH);
            #endif
            
            gpio_pin_set(gpio_dev, PWR_PIN, 0);
            k_msleep(450);
            k_work_reschedule(&xw12a_reset_work, K_SECONDS(60));
            return 0;

        default:
            return -ENOTSUP;
    }
}

PM_DEVICE_DEFINE(xw12a_pwr_pm_data, xw12a_pwr_pm_action);

DEVICE_DEFINE(xw12a_pwr_inst, "XW12A_PWR", xw12a_pwr_init,
              PM_DEVICE_GET(xw12a_pwr_pm_data), NULL, NULL,
              POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY, NULL);
