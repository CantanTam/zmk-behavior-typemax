#define DT_DRV_COMPAT xinwang_xw12a
// 长按首次输出与长按效果之间的时间差 这里是 7*100 ms
#define TAP_PRESS_GAP 7

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/keys.h>
#include <zephyr/drivers/led_strip.h>

#include "xw12a_dict.c"

// 声明外部工具
typedef enum { WS_OFF = 0, WS_RED, WS_GREEN, WS_BLUE } ws2812_color_t;
#define RED WS_RED
#define GREEN WS_GREEN
#define BLUE WS_BLUE
#define OFF WS_OFF

extern uint8_t use_touch; // use_touch == 0 就关闭 touchtype 功能
extern void light_up_ws2812(ws2812_color_t c1, ws2812_color_t c2, ws2812_color_t c3, ws2812_color_t c4);
extern void ws2812_power_on(void);
extern void ws2812_power_off(void);

bool pad_action_statu = false;
// 记录上一次的 xw12a 寄存器状态值
uint16_t prev_xw12a_value = 0xFFFF;

// left pad 使用的时间戳和触控按钮记录
uint64_t left_prev_time = 0;
uint8_t left_first_pad = 0x0F;
uint8_t left_final_pad = 0x0F;

// right pad 使用的时间戳和触控按钮记录
uint64_t right_prev_time = 0;
uint8_t right_first_pad = 0x0F;
uint8_t right_final_pad = 0x0F;


// top pad 使用的时间戳和触控按钮记录
uint64_t top_prev_time = 0;
uint8_t top_first_pad = 0x0F;
uint8_t top_final_pad = 0x0F;

// 记录 pad9 ~ pad11 在 top_pad_run_mode 为 false 时计算得到的值；
uint8_t top_first_last_pad = 0x0F;

bool top_pad_run_mode = false;

struct xw12a_config {
    struct i2c_dt_spec i2c;
    struct gpio_dt_spec int_gpio;
};

struct xw12a_data {
    struct gpio_callback gpio_cb;
    struct k_work work;
    const struct device *dev;
};

/**
 * key_tap 是完成一次按下、松开操作；key_press 是按下；key_release 是松开
 * encoded_keycode 编码后的键值（支持修饰键组合）
 */
static void key_tap(uint32_t encoded_keycode) {
    if (encoded_keycode == CANCEL) {
        ws2812_power_off();
        top_pad_run_mode = !top_pad_run_mode;
        return;
    }

    int64_t press_time = k_uptime_get();

    // 1. 发送按下事件 (Press)
    raise_zmk_keycode_state_changed(
        zmk_keycode_state_changed_from_encoded(encoded_keycode, true, press_time)
    );

    // 2. 必须给 ZMK 处理逻辑预留一点时间（通常 10-30ms 足够识别组合键）
    k_msleep(30);

    int64_t release_time = k_uptime_get();

    // 3. 发送释放事件 (Release)
    raise_zmk_keycode_state_changed(
        zmk_keycode_state_changed_from_encoded(encoded_keycode, false, release_time)
    );

}

static void key_press(uint32_t encoded_keycode) {
    int64_t press_time = k_uptime_get();

    raise_zmk_keycode_state_changed(
        zmk_keycode_state_changed_from_encoded(encoded_keycode, true, press_time)
    );

    //k_msleep(50);
}

static void key_release(uint32_t encoded_keycode) {
    int64_t release_time = k_uptime_get();

    raise_zmk_keycode_state_changed(
        zmk_keycode_state_changed_from_encoded(encoded_keycode, false, release_time)
    );

    //k_msleep(30);
}

// 获取 xw12a 芯片寄存器的 pad 状态值
static uint16_t get_xw12a_pad_value(const struct device *dev) {
    const struct xw12a_config *config = dev->config;
    uint8_t buf[2];

    if (i2c_read_dt(&config->i2c, buf, sizeof(buf)) != 0) {
        return 0xFFFF;
    }

    return ((uint16_t)buf[0] << 8) | buf[1];
}


/**
 * @param shift_bits 右移位数 (12=最左侧, 8=左二, 4=左三)
 * @return 截取后的 4 位结果 (0x00 - 0x0F)
 */
static uint8_t cut_xw12a_data(uint16_t raw_value, int shift_bits) {
    return (uint8_t)((raw_value >> shift_bits) & 0x0F);
}


static void left_pad_action(const struct device *dev) {

    pad_action_statu = true;

    uint8_t left_first_final_pad = left_first_pad + left_final_pad * left_final_pad;

    uint32_t left_pad_combo = left_dict_addr_padx(left_first_final_pad);

    if (left_pad_combo == 0x00){
        pad_action_statu = false;
        return;
    }

    key_tap(left_pad_combo);

    // --- 1. 长按判定 (500ms 内松手即视为单击) ---
    for (int count = 0; count < TAP_PRESS_GAP; count++) {
        k_msleep(100);
        if (cut_xw12a_data(get_xw12a_pad_value(dev), 12) == 0x0F) {
            pad_action_statu = false;
            prev_xw12a_value = get_xw12a_pad_value(dev);
            return; 
        }
    }

    // 只发送一次按下，不要写循环！
    key_press(left_pad_combo);

    // 阻塞式等待：手指不离开，程序就一直停在这里检查，什么都不发
    while (cut_xw12a_data(get_xw12a_pad_value(dev), 12) != 0x0F) {
        k_msleep(100); // 极高频检查
    }

    // 只要手一松，立刻发释放，并退出
    key_release(left_pad_combo);
    pad_action_statu = false;
    prev_xw12a_value = get_xw12a_pad_value(dev);
    return;

}

static void right_pad_action(const struct device *dev) {

    pad_action_statu = true;

    uint8_t right_first_final_pad = right_first_pad + right_final_pad * right_final_pad;
    
    uint32_t right_pad_combo = right_dict_addr_padx(right_first_final_pad);

    if (right_pad_combo == 0x00){
        pad_action_statu = false;
        return;
    }

    key_tap(right_pad_combo);

    // --- 1. 长按判定 (500ms 内松手即视为单击) ---
    for (int count = 0; count < TAP_PRESS_GAP; count++) {
        k_msleep(100);
        if (cut_xw12a_data(get_xw12a_pad_value(dev), 8) == 0x0F) {
            pad_action_statu = false;
            prev_xw12a_value = get_xw12a_pad_value(dev);
            return; 
        }
    }

    // 只发送一次按下，不要写循环！
    key_press(right_pad_combo);

    // 阻塞式等待：手指不离开，程序就一直停在这里检查，什么都不发
    while (cut_xw12a_data(get_xw12a_pad_value(dev), 8) != 0x0F) {
        k_msleep(100); // 极高频检查
    }

    // 只要手一松，立刻发释放，并退出
    key_release(right_pad_combo);
    pad_action_statu = false;
    prev_xw12a_value = get_xw12a_pad_value(dev);
    return;
}

// 根据 top_pad_mode 函数结果执行相应的 pad 操作
static void top_pad_action(const struct device *dev)
{
    // 对比 xw12a 寄存器九至十二位值的变化，当九至十二位产生变化才执行 pad9 ~ pad11 的操作函数
    if (cut_xw12a_data((get_xw12a_pad_value(dev) ^ prev_xw12a_value),4) == 0x00){
        return;
    }

    if (top_pad_run_mode == false){

        top_first_last_pad = top_first_pad * top_final_pad;

        ws2812_color_t c1 = OFF, c2 = OFF, c3 = OFF, c4 = OFF;

        switch (top_first_last_pad){
            case 0x4D : c1 = OFF; c2 = GREEN; c3 = GREEN; c4 = RED;   break;
            case 0x8F : c1 = OFF; c2 = RED;   c3 = GREEN; c4 = GREEN; break;
            case 0x5B : c1 = OFF; c2 = GREEN; c3 = RED;   c4 = GREEN; break;
            default:
                top_first_last_pad = 0x0F;
                return;
        }

        ws2812_power_on();
        k_msleep(100);
        light_up_ws2812(c1, c2, c3, c4);

        top_pad_run_mode = true;
        
        prev_xw12a_value = get_xw12a_pad_value(dev);

    } else {

        uint8_t top_func_pad = cut_xw12a_data(get_xw12a_pad_value(dev), 4);

        /*
        int count;

        // 0.5 秒内离开才继续下面的操作，否则退出整个函数，所以这里是 1111
        for(count = 0; count < 50; count++) {
            k_msleep(10);
            if (cut_xw12a_data(get_xw12a_pad_value(dev), 4) == 0x0F) {
                    break; 
                }
        }

        if (count == 50){
            while (cut_xw12a_data(get_xw12a_pad_value(dev), 4) != 0x0F) {
                k_msleep(10); // 每 20ms 检查一次，直到你真的把手指拿开
            }
            prev_xw12a_value = get_xw12a_pad_value(dev);
            return;
        }
        */        

        uint8_t top_first_last_func_pad = top_first_last_pad + top_func_pad;

        uint32_t top_pad_combo = top_dict_addr_padx(top_first_last_func_pad);  

        key_tap(top_pad_combo);

        k_msleep(200);

        return;

    }

};

// --- 核心状态检测函数 ---
static void pad_statu_detect(const struct device *dev)
{
    if (pad_action_statu){
        return;
    }

    struct xw12a_data *data = dev->data;

    uint16_t xw12a_pad_value = get_xw12a_pad_value(dev);

    if (xw12a_pad_value == 0xFFFF ){
        prev_xw12a_value = 0xFFFF;
        return;
    }

    //  比较前后两种 touch pad 状态来进行判定
    uint16_t prev_current_pad_compare = prev_xw12a_value ^ xw12a_pad_value;

    // left pad 动作的判定
    if ( cut_xw12a_data(prev_current_pad_compare, 12) != 0x00 ){

        if ( cut_xw12a_data(xw12a_pad_value, 12) != 0x00 ){

            if ( k_uptime_get() - left_prev_time <= 700 ){

                left_prev_time = k_uptime_get();
                left_final_pad = cut_xw12a_data(xw12a_pad_value, 12);
                left_pad_action(dev);

            } else {

                left_prev_time = k_uptime_get();
                left_first_pad = cut_xw12a_data(xw12a_pad_value, 12);

            }
        }
    } 
    
    if ( cut_xw12a_data(prev_current_pad_compare, 8) != 0x00 ){

        if ( cut_xw12a_data(xw12a_pad_value, 8) != 0x00 ){

            if ( k_uptime_get() - right_prev_time <= 700 ){

                right_prev_time = k_uptime_get();
                right_final_pad = cut_xw12a_data(xw12a_pad_value, 8);
                right_pad_action(dev);

            } else {

                right_prev_time = k_uptime_get();
                right_first_pad = cut_xw12a_data(xw12a_pad_value, 8);

            }
        }
    }

    if ( cut_xw12a_data(prev_current_pad_compare, 4) != 0x00 ){
        if ( top_pad_run_mode == false ){

            if ( cut_xw12a_data(xw12a_pad_value, 4) != 0x00 ){

                if ( k_uptime_get() - top_prev_time <= 700 ){

                    top_prev_time = k_uptime_get();
                    top_final_pad = cut_xw12a_data(xw12a_pad_value, 4);
                    top_pad_action(dev);

                } else {

                    top_prev_time = k_uptime_get();
                    top_first_pad = cut_xw12a_data(xw12a_pad_value, 4);

                }
            }
        } else {
            top_pad_action(dev);
        }
    }

    #ifdef CONFIG_TOP_PAD_CONTROL

    if (top_pad_value != 0x0F) {
        top_pad_action(dev);
    }

    #endif /* 顶部 touchpad 控制电脑 */

    prev_xw12a_value = get_xw12a_pad_value(dev);

    // 扫动补丁：若仍有键被按住，继续处理,这一段是 gemini 给的答案，
    // 注释掉就可以正常运行，但我不敢删除它
    /*if (xw12a_pad_value != 0xFFFF) {
        k_work_submit(&data->work);
    }*/
}

// --- Zephyr 驱动标准回调与初始化 ---

static void xw12a_work_handler(struct k_work *work)
{
    struct xw12a_data *data = CONTAINER_OF(work, struct xw12a_data, work);
    // 当 int 中断信号变化时，启动检测
    pad_statu_detect(data->dev);
}

static void xw12a_gpio_callback(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
    if (use_touch == 0 ){
        return;
    }
    
    struct xw12a_data *data = CONTAINER_OF(cb, struct xw12a_data, gpio_cb);
    k_work_submit(&data->work);
}

static int xw12a_init(const struct device *dev)
{
    const struct xw12a_config *config = dev->config;
    struct xw12a_data *data = dev->data;
    data->dev = dev;

    if (!device_is_ready(config->i2c.bus) || !device_is_ready(config->int_gpio.port)) {
        return -ENODEV;
    }

    gpio_pin_configure_dt(&config->int_gpio, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&config->int_gpio, GPIO_INT_EDGE_BOTH);

    k_work_init(&data->work, xw12a_work_handler);
    gpio_init_callback(&data->gpio_cb, xw12a_gpio_callback, BIT(config->int_gpio.pin));
    gpio_add_callback(config->int_gpio.port, &data->gpio_cb);

    return 0;
}

static struct xw12a_data xw12a_data_0;
static const struct xw12a_config xw12a_config_0 = {
    .i2c = I2C_DT_SPEC_INST_GET(0),
    .int_gpio = GPIO_DT_SPEC_INST_GET(0, int_gpios),
};

DEVICE_DT_INST_DEFINE(0, xw12a_init, NULL, &xw12a_data_0, &xw12a_config_0, POST_KERNEL, 99, NULL);