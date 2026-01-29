#define DT_DRV_COMPAT zmk_behavior_blink

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define BLINK_INTERVAL 100 // 固定 150ms 间隔

struct blink_item {
    struct gpio_dt_spec gpio;
    struct k_work_delayable work;
    uint32_t remaining_count; // 剩余闪烁次数
    bool is_active;           // 当前是否处于点亮状态
};

#define BLINK_ITEM_INIT(node_id, prop, idx) \
    { \
        .gpio = GPIO_DT_SPEC_GET_BY_IDX(node_id, prop, idx), \
    },

static struct blink_item blink_items[] = {
    DT_FOREACH_PROP_ELEM(DT_DRV_INST(0), gpios, BLINK_ITEM_INIT)
};

// 核心控制逻辑
static void blink_worker(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct blink_item *item = CONTAINER_OF(dwork, struct blink_item, work);

    if (item->is_active) {
        // 当前是亮的 -> 关掉
        gpio_pin_set_dt(&item->gpio, 0);
        item->is_active = false;
        
        // 如果还有剩余次数，继续下一次循环
        if (item->remaining_count > 0) {
            k_work_reschedule(&item->work, K_MSEC(BLINK_INTERVAL));
        }
    } else {
        // 当前是灭的 -> 点亮
        if (item->remaining_count > 0) {
            gpio_pin_set_dt(&item->gpio, 1);
            item->is_active = true;
            item->remaining_count--; // 完成一次点亮，计数减一
            k_work_reschedule(&item->work, K_MSEC(BLINK_INTERVAL));
        }
    }
}

static int behavior_blink_init(const struct device *dev) {
    for (int i = 0; i < ARRAY_SIZE(blink_items); i++) {
        if (!gpio_is_ready_dt(&blink_items[i].gpio)) return -ENODEV;
        gpio_pin_configure_dt(&blink_items[i].gpio, GPIO_OUTPUT_INACTIVE);
        k_work_init_delayable(&blink_items[i].work, blink_worker);
    }
    return 0;
}

static int on_blink_pressed(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
    uint32_t idx = binding->param1;
    uint32_t count = binding->param2;

    if (idx >= ARRAY_SIZE(blink_items)) return ZMK_BEHAVIOR_OPAQUE;

    struct blink_item *item = &blink_items[idx];

    // 如果当前正在闪烁，先取消之前的任务
    k_work_cancel_delayable(&item->work);

    item->remaining_count = count;
    item->is_active = false; // 从“准备点亮”状态开始

    // 立即触发第一次动作
    k_work_reschedule(&item->work, K_NO_WAIT);

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_blink_driver_api = {
    .binding_pressed = on_blink_pressed,
};

BEHAVIOR_DT_INST_DEFINE(0, behavior_blink_init, NULL, NULL, NULL, 
                       POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, 
                       &behavior_blink_driver_api);