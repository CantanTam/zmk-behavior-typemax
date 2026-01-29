#define DT_DRV_COMPAT zmk_behavior_led

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// 定义每个灯的控制结构体
struct led_item {
    struct gpio_dt_spec gpio;
    struct k_work_delayable work;
    uint32_t index;
};

// 修正后的宏：接收 3 个参数，并将其传递给 GPIO 获取函数
#define LED_ITEM_INIT(node_id, prop, idx) \
    { \
        .gpio = GPIO_DT_SPEC_GET_BY_IDX(node_id, prop, idx), \
        .index = idx, \
    },

// 自动根据 DTS 生成实例数组
static struct led_item led_items[] = {
    DT_FOREACH_PROP_ELEM(DT_DRV_INST(0), gpios, LED_ITEM_INIT)
};

// 计时结束后的回调：关闭对应的引脚
static void led_off_handler(struct k_work *work) {
    // 换用这个更通用的宏，将 work 指针转为 delayable_work 指针
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    
    // 然后再通过 CONTAINER_OF 找到我们自定义的 led_item 结构体
    struct led_item *item = CONTAINER_OF(dwork, struct led_item, work);
    
    gpio_pin_set_dt(&item->gpio, 0); // 熄灭
    LOG_INF("LED index %d timer expired, turned OFF", item->index);
}

static int behavior_led_init(const struct device *dev) {
    for (int i = 0; i < ARRAY_SIZE(led_items); i++) {
        if (!gpio_is_ready_dt(&led_items[i].gpio)) {
            LOG_ERR("GPIO %d not ready", i);
            return -ENODEV;
        }
        // 初始化引脚
        gpio_pin_configure_dt(&led_items[i].gpio, GPIO_OUTPUT_INACTIVE);
        // 初始化延时任务
        k_work_init_delayable(&led_items[i].work, led_off_handler);
    }
    return 0;
}

static int on_led_binding_pressed(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {
    uint32_t idx = binding->param1;      // 对应 &led 后的第一个参数 (0 或 1)
    uint32_t duration = binding->param2; // 对应 &led 后的第二个参数 (毫秒)

    if (idx >= ARRAY_SIZE(led_items)) {
        LOG_ERR("LED index %d out of bounds", idx);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    struct led_item *item = &led_items[idx];

    // 点亮引脚 (Active)
    gpio_pin_set_dt(&item->gpio, 1);
    
    // 启动/重置延时计时
    k_work_reschedule(&item->work, K_MSEC(duration));

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_led_driver_api = {
    .binding_pressed = on_led_binding_pressed,
};

// 注册 Behavior
BEHAVIOR_DT_INST_DEFINE(0, behavior_led_init, NULL, NULL, NULL, 
                       POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, 
                       &behavior_led_driver_api);