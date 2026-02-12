#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>

// 声明外部工具
typedef enum { WS_OFF = 0, WS_RED, WS_GREEN, WS_BLUE } ws2812_color_t;
#define RED WS_RED
#define GREEN WS_GREEN
#define BLUE WS_BLUE
#define OFF WS_OFF

extern void light_up_ws2812(ws2812_color_t c1, ws2812_color_t c2, ws2812_color_t c3, ws2812_color_t c4);
extern void ws2812_power_on(void);

/**
 * 业务层的初始化函数
 */
static int my_app_led_setup(const struct device *dev) {
    //ARG_UNUSED(dev);

    // 1. 业务逻辑决定：先开电源
    ws2812_power_on();
    k_msleep(100);

    // 2. 业务逻辑决定：点亮特定的颜色方案
    light_up_ws2812(BLUE,GREEN, BLUE, RED);

    return 0;
}

// 在业务代码里挂载初始化，这样驱动文件就彻底解耦了
SYS_INIT(my_app_led_setup, APPLICATION, 99);