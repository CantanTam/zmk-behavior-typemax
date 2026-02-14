#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>

// 1. 定义引脚和端口
#define PWR_PIN  10
#define INT_PIN  6
#define PWR_PORT DT_NODELABEL(gpio1)

static const struct device *gpio_dev;

static int xw12a_pwr_init(const struct device *dev) {
    // 获取 GPIO1 端口句柄
    gpio_dev = DEVICE_DT_GET(PWR_PORT); 
    if (!device_is_ready(gpio_dev)) {
        return -ENODEV;
    }

    // 初始化 P1.10 为输出低电平（开启电源）
    gpio_pin_configure(gpio_dev, PWR_PIN, GPIO_OUTPUT_LOW);
    
    // 注意：INT 引脚（P1.6）通常由传感器驱动程序初始化，这里初始化电源即可
    return 0;
}

static int xw12a_pwr_pm_action(const struct device *dev, enum pm_device_action action) {
    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND:
        // --- 步骤 1: 拉高 P1.10 关断 P-MOS ---
        gpio_pin_set(gpio_dev, PWR_PIN, 1);

        // --- 步骤 2: 断开 P1.6 防止倒灌 ---
        // 将中断引脚设为高阻态，切断漏电路径
        gpio_pin_configure(gpio_dev, INT_PIN, GPIO_DISCONNECTED);
        
        return 0;

    case PM_DEVICE_ACTION_RESUME:
        // --- 步骤 1: 恢复 P1.6 的输入状态 ---
        // 假设你的传感器中断是上拉输入，请根据实际电路调整
        gpio_pin_configure(gpio_dev, INT_PIN, GPIO_INPUT | GPIO_PULL_UP);

        // --- 步骤 2: 拉低 P1.10 恢复供电 ---
        gpio_pin_set(gpio_dev, PWR_PIN, 0);
        
        return 0;

    default:
        return -ENOTSUP;
    }
}

// 定义电源管理数据
PM_DEVICE_DEFINE(xw12a_pwr_pm_data, xw12a_pwr_pm_action);

// 定义系统设备实例
DEVICE_DEFINE(xw12a_pwr_inst, "XW12A_PWR", xw12a_pwr_init, 
              PM_DEVICE_GET(xw12a_pwr_pm_data), NULL, NULL, 
              POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY, NULL);