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
extern struct k_work_delayable keep_alive_dwork;

static int xw12a_pwr_init(const struct device *dev) {
    gpio_dev = DEVICE_DT_GET(PWR_PORT);
    if (!device_is_ready(gpio_dev)) {
        return -ENODEV;
    }

    gpio_pin_configure(gpio_dev, PWR_PIN, (use_touch == 1 ? GPIO_OUTPUT_LOW : GPIO_OUTPUT_HIGH));

    return 0;
}

static int xw12a_pwr_pm_action(const struct device *dev, enum pm_device_action action) {
    switch (action) {
        case PM_DEVICE_ACTION_SUSPEND:
            // 键盘休眠时，停止心跳并断电
            k_work_cancel_delayable(&keep_alive_dwork);

            gpio_pin_set(gpio_dev, PWR_PIN, 1);

            gpio_pin_configure(gpio_dev, INT_PIN, GPIO_DISCONNECTED);
            
            return 0;

        case PM_DEVICE_ACTION_RESUME:
            // 1. 先恢复物理引脚状态（输入 + 上拉）
            gpio_pin_configure(gpio_dev, INT_PIN, GPIO_INPUT | GPIO_PULL_UP);
            // 2. 【最关键的一步】重新绑定双边沿触发中断！
            gpio_pin_interrupt_configure(gpio_dev, INT_PIN, GPIO_INT_EDGE_BOTH);
            
            gpio_pin_set(gpio_dev, PWR_PIN, 0);
            k_msleep(450);

            // 5. 重新开启保活循环：安排 15 秒后执行第一次心跳
            if (use_touch == 1) {
                k_work_schedule(&keep_alive_dwork, K_SECONDS(15));
            }
            return 0;

        default:
            return -ENOTSUP;
    }
}

PM_DEVICE_DEFINE(xw12a_pwr_pm_data, xw12a_pwr_pm_action);

DEVICE_DEFINE(xw12a_pwr_inst, "XW12A_PWR", xw12a_pwr_init,
              PM_DEVICE_GET(xw12a_pwr_pm_data), NULL, NULL,
              POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY, NULL);
