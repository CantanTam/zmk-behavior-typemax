#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/led_strip.h>


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

/* --- 1. 基础定义 --- */
typedef enum { 
    WS_OFF = 0, 
    WS_RED, 
    WS_GREEN, 
    WS_BLUE 
} ws2812_color_t;

// 定义宏方便调用
#define RED   WS_RED
#define GREEN WS_GREEN
#define BLUE  WS_BLUE
#define OFF   WS_OFF

#define STRIP_NODE DT_NODELABEL(led_strip)
static const struct device *strip = DEVICE_DT_GET(STRIP_NODE);
static struct led_rgb pixels[4];

/* --- 2. 供外部调用的工具函数 --- */
void light_up_ws2812(ws2812_color_t c1, ws2812_color_t c2, ws2812_color_t c3, ws2812_color_t c4) {
    if (!device_is_ready(strip)) return;

    ws2812_color_t colors[4] = {c1, c2, c3, c4};
    const uint8_t brt = 4; 

    for (int i = 0; i < 4; i++) {
        pixels[i].r = (colors[i] == WS_RED)   ? brt : 0;
        pixels[i].g = (colors[i] == WS_GREEN) ? brt : 0;
        pixels[i].b = (colors[i] == WS_BLUE)  ? brt : 0;
    }
    led_strip_update_rgb(strip, pixels, 4);
}

PM_DEVICE_DEFINE(ws2812_pwr_pm_data, ws2812_pwr_pm_action);

DEVICE_DEFINE(ws2812_pwr_inst, "WS2812_PWR", ws2812_pwr_init, 
              PM_DEVICE_GET(ws2812_pwr_pm_data), NULL, NULL, 
              POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY, NULL);