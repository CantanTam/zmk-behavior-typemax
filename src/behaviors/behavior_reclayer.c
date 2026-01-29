#define DT_DRV_COMPAT zmk_behavior_reclayer

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/settings/settings.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/keymap.h>

#ifdef CONFIG_RECORD_LAYER

/* -------------------------------------------------------------------------- */
/* Behavior: &reclayer X                                                      */
/* -------------------------------------------------------------------------- */

static uint8_t saved_layer;

/* binding_pressed 只负责记录到 Flash，不切换层 */
static int on_reclayer_binding_pressed(
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event)
{
    saved_layer = binding->param1;
    int ret;

    /* Retry 机制确保写入 Flash 成功 */
    for (int i = 0; i < 3; i++) {
        ret = settings_save_one("zmk/reclayer/val",
                                &saved_layer,
                                sizeof(saved_layer));
        if (ret == 0) {
            break;
        }
        k_msleep(20); /* 等待 Flash 可用 */
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_reclayer_driver_api = {
    .binding_pressed = on_reclayer_binding_pressed,
};

/* -------------------------------------------------------------------------- */
/* Behavior device                                                            */
/* -------------------------------------------------------------------------- */

BEHAVIOR_DT_INST_DEFINE(
    0,
    NULL, /* init 不需要 */
    NULL,
    NULL,
    NULL,
    POST_KERNEL,
    CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
    &behavior_reclayer_driver_api
);

#endif /* CONFIG_RECORD_LAYER */
