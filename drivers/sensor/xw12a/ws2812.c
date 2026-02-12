#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/spi.h>

/* 1. 引用外部定义的电源开启函数 */
extern void ws2812_power_on(void);

/* 2. 获取设备树中定义的 led_strip 设备 */
#define STRIP_NODE DT_NODELABEL(led_strip)
#define STRIP_NUM_PIXELS 4

static const struct device *strip = DEVICE_DT_GET(STRIP_NODE);

/* 3. 定义颜色缓冲区（4 颗灯） */
static struct led_rgb pixels[STRIP_NUM_PIXELS];

/**
 * 初始化并点亮第一颗红色 LED
 */
int ws2812_init_display(void)
{
    /* A. 开启电源 (调用你提到的外部函数) */
    ws2812_power_on();

    /* B. 等待电源稳定 */
    k_msleep(100);

    /* C. 检查 LED 设备是否就绪 */
    if (!device_is_ready(strip)) {
        return -ENODEV;
    }

    /* D. 清空所有灯珠颜色数据 (保证 2,3,4 颗是灭的) */
    for (int i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels[i].r = 0;
        pixels[i].g = 0;
        pixels[i].b = 0;
    }

    // 第 1 颗：红色
    pixels[0].r = 4;
    pixels[0].g = 0;
    pixels[0].b = 0;

    // 第 2 颗：绿色
    pixels[1].r = 0;
    pixels[1].g = 4;
    pixels[1].b = 0;

    // 第 3 颗：蓝色
    pixels[2].r = 0;
    pixels[2].g = 0;
    pixels[2].b = 4;

    // 第 4 颗：红色
    pixels[3].r = 4;
    pixels[3].g = 0;
    pixels[3].b = 0;

    /* F. 将数据发送给灯珠 */
    return led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
}

/* * 假设这是你的驱动初始化入口
 * 如果 ZMK 自动调用初始化，可以将 ws2812_init_display 放在这里
 */
static int ws2812_driver_init(const struct device *dev)
{
    ARG_UNUSED(dev);
    return ws2812_init_display();
}

/* 将该初始化逻辑挂载到系统启动流程中 (POST_KERNEL 级别确保 SPI 已就绪) */
SYS_INIT(ws2812_driver_init, POST_KERNEL, 99);