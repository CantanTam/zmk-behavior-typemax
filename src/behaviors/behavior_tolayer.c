#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/settings/settings.h> // 必须引入 settings 头文件
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/keymap.h>

#ifdef CONFIG_RECORD_LAYER

// 默认层级（如果 Flash 中还没有存过值，则跳到这一层）
#define DEFAULT_LAYER 0

static uint8_t layer_to_restore = DEFAULT_LAYER;

/**
 * Settings 回调：当从 Flash 读取到 zmk/reclayer/val 时，将其赋值给变量
 */
static int reclayer_settings_set(const char *name, size_t len,
                                 settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    if (settings_name_steq(name, "val", &next) && !next) {
        if (len == sizeof(layer_to_restore)) {
            return read_cb(cb_arg, &layer_to_restore, sizeof(layer_to_restore));
        }
    }
    return -ENOENT;
}

struct settings_handler reclayer_conf = {
    .name = "zmk/reclayer",
    .h_set = reclayer_settings_set
};

/**
 * 核心逻辑：跳转到记录的层
 */
static void jump_to_target_layer() {
    // 1. 先从 Flash 加载最新的值
    settings_load_subtree("zmk/reclayer");
    
    // 2. 执行跳转
    // 此时 layer_to_restore 已经被 reclayer_settings_set 更新为 Flash 里的值
    zmk_keymap_layer_to(layer_to_restore, "AutoLayer");
}

/* --- 事件监听逻辑 --- */

static int activity_event_handler(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev != NULL && ev->state == ZMK_ACTIVITY_ACTIVE) {
        jump_to_target_layer();
    }
    return 0;
}

ZMK_LISTENER(jump_on_wake, activity_event_handler);
ZMK_SUBSCRIPTION(jump_on_wake, zmk_activity_state_changed);

/**
 * 初始化
 */
static int behavior_tolayer_init(void) {
    // 注册 settings 处理器，这样系统才知道怎么处理 "zmk/reclayer/..."
    settings_subsys_init();
    settings_register(&reclayer_conf);
    
    // 执行初始跳转
    jump_to_target_layer();
    return 0;
}

// 优先级 99 确保在设置系统之后启动
SYS_INIT(behavior_tolayer_init, APPLICATION, 99);

#endif /* CONFIG_RECORD_LAYER */
