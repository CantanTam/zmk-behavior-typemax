#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>

// 定义 WS2812 的电源引脚 P0.19 和 GPIO0
#define LED_PWR_PIN  19
#define LED_PWR_PORT DT_NODELABEL(gpio0)

static const struct device *led_gpio_dev;

// 【核心修改】：只有被显式调用时，才配置引脚并拉低
void ws2812_power_on(void) {
    if (led_gpio_dev && device_is_ready(led_gpio_dev)) {
        // 配置为输出并初始化为低电平 (0V 开启电源)
        gpio_pin_configure(led_gpio_dev, LED_PWR_PIN, GPIO_OUTPUT_LOW);
    }
}

// 显式关断函数：将引脚设为高电平 (3.3V 关闭电源)
void ws2812_power_off(void) {
    if (led_gpio_dev && device_is_ready(led_gpio_dev)) {
        gpio_pin_configure(led_gpio_dev, LED_PWR_PIN, GPIO_OUTPUT_HIGH);
    }
}

static int ws2812_pwr_init(const struct device *dev) {
    // 仅仅获取句柄，不进行 gpio_pin_configure
    led_gpio_dev = DEVICE_DT_GET(LED_PWR_PORT);
    
    if (!device_is_ready(led_gpio_dev)) {
        return -ENODEV;
    }

    // 这里不写任何 gpio_pin_configure，引脚保持系统默认（通常是高阻输入）
    return 0;
}

// 电源管理：系统休眠时可以强制关断以省电
static int ws2812_pwr_pm_action(const struct device *dev, enum pm_device_action action) {
    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND:
        ws2812_power_off();
        return 0;
    case PM_DEVICE_ACTION_RESUME:
        // 唤醒时如果不希望自动开灯，这里可以留空，等待业务逻辑重新调用 power_on
        return 0;
    default:
        return -ENOTSUP;
    }
}

PM_DEVICE_DEFINE(ws2812_pwr_pm_data, ws2812_pwr_pm_action);

DEVICE_DEFINE(ws2812_pwr_inst, "WS2812_PWR", ws2812_pwr_init, 
              PM_DEVICE_GET(ws2812_pwr_pm_data), NULL, NULL, 
              POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY, NULL);