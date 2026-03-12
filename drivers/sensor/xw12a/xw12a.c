#define DT_DRV_COMPAT xinwang_xw12a
#define TAP_TAP_GAP 700
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

//bool pad_action_statu = false;
bool top_pad_run_mode = false; 

struct xw12a_data {
    struct gpio_callback gpio_cb;
    struct k_work work;
    struct k_work_delayable keep_alive_dwork; // 保活定时器现在属于每个芯片自己
    const struct device *dev;                 // 指向设备实例的指针

    // --- 以下是搬进来的“私有记忆” ---
    bool pad_action_statu;

    uint16_t prev_xw12a_value;

    uint64_t left_prev_time;
    uint8_t front_first_pad;
    uint8_t front_final_pad;

    uint64_t top_prev_time;
    uint8_t top_first_pad;
    uint8_t top_final_pad;
    uint8_t top_first_last_pad;
};

struct xw12a_config {
    struct i2c_dt_spec i2c;
    struct gpio_dt_spec int_gpio;
};

 /* key_tap 是完成一次按下、松开操作；key_press 是按下；key_release 是松开
 * encoded_keycode 编码后的键值（支持修饰键组合）
 */
static void key_tap(const struct device *dev, uint32_t encoded_keycode) {
    struct xw12a_data *data = dev->data;

    if (encoded_keycode == CANCEL) {
        ws2812_power_off();
        top_pad_run_mode = !top_pad_run_mode;
        data->pad_action_statu = false;
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
}

static void key_release(uint32_t encoded_keycode) {
    int64_t release_time = k_uptime_get();

    raise_zmk_keycode_state_changed(
        zmk_keycode_state_changed_from_encoded(encoded_keycode, false, release_time)
    );
}

// 定时读取 i2c 数据，刺激 xw12a 以保持唤醒状态
void xw12a_keep_alive_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct xw12a_data *data = CONTAINER_OF(dwork, struct xw12a_data, keep_alive_dwork);
    const struct device *dev = data->dev;

    // 只有在开启触摸功能 且 当前没有按键动作时，才执行保活逻辑
    if (use_touch == 1 && data->pad_action_statu == false) {
        const struct xw12a_config *config = dev->config;
        uint8_t dummy_buf[4]; // 维持 4 字节，产生足够的物理脉冲
        
        /* * 【强力唤醒逻辑】
         * 在满足 pad_action_statu == false 的窗口期，进行连续 3 次尝试。
         * 这样即便其中一次因为总线繁忙或芯片微睡没响应，
         * 后续的连击也能确保 SDA 被拉低，满足手册防休眠要求。
         */
        for (int i = 0; i < 4; i++) {
            int ret = i2c_read_dt(&config->i2c, dummy_buf, sizeof(dummy_buf));
            
            // 如果读取成功，说明芯片已被激活，提前退出循环
            if (ret == 0) {
                break;
            }
            // 如果读取失败，微等 1 毫秒后再次重试，增加唤醒成功率
            k_busy_wait(1000);
        }
    }

    // 无论是否执行了读取，都重新调度下一次 5 秒的心跳
    if (use_touch == 1) {
        k_work_schedule(&data->keep_alive_dwork, K_SECONDS(15));
    }
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


static void front_pad_action(const struct device *dev) {
    struct xw12a_data *data = dev->data; // 获取私有记忆
    data->pad_action_statu = true;

    uint8_t front_first_final_pad = data->front_first_pad + data->front_final_pad * data->front_final_pad;

    uint32_t front_pad_combo = left_dict_addr_padx(front_first_final_pad);

    if (front_pad_combo == 0x00){
        data->pad_action_statu = false;
        return;
    }

    key_tap(dev, front_pad_combo);

    // --- 1. 长按判定 (500ms 内松手即视为单击) ---
    for (int count = 0; count < TAP_PRESS_GAP; count++) {
        k_msleep(100);
        if (cut_xw12a_data(get_xw12a_pad_value(dev), 12) == 0x0F) {
            data->pad_action_statu = false;
            data->prev_xw12a_value = get_xw12a_pad_value(dev);
            return; 
        }
    }

    // 只发送一次按下，不要写循环！
    key_press(front_pad_combo);

    // 阻塞式等待：手指不离开，程序就一直停在这里检查，什么都不发
    while (cut_xw12a_data(get_xw12a_pad_value(dev), 12) != 0x0F) {
        k_msleep(100); // 极高频检查
    }

    // 只要手一松，立刻发释放，并退出
    key_release(front_pad_combo);
    data->pad_action_statu = false;
    data->prev_xw12a_value = get_xw12a_pad_value(dev);
    data->left_prev_time -= TAP_TAP_GAP;
    return;

}

// 根据 top_pad_mode 函数结果执行相应的 pad 操作
static void top_pad_action(const struct device *dev)
{
    struct xw12a_data *data = dev->data;
    data->pad_action_statu = true;
    if (cut_xw12a_data((get_xw12a_pad_value(dev) ^ data->prev_xw12a_value),8) == 0x00){
        return;
    }

    if (top_pad_run_mode == false){

        data->top_first_last_pad = data->top_first_pad * data->top_final_pad;

        ws2812_color_t c1 = OFF, c2 = OFF, c3 = OFF, c4 = OFF;

        switch (data->top_first_last_pad){
            case 0x4D : c1 = GREEN; c2 = GREEN; c3 = RED;   c4 = RED;   break;
            case 0x8F : c1 = RED;   c2 = GREEN; c3 = GREEN; c4 = RED;   break;
            case 0xB6 : c1 = RED;   c2 = BLUE;  c3 = GREEN; c4 = GREEN; break;
            case 0x62 : c1 = GREEN; c2 = RED;   c3 = BLUE;  c4 = GREEN; break;
            case 0x9A : c1 = BLUE;  c2 = GREEN; c3 = RED;   c4 = GREEN; break;
            case 0x5B : c1 = GREEN; c2 = RED;   c3 = GREEN; c4 = BLUE;  break;
            default:
                data->top_first_last_pad = 0x0F;
                data->pad_action_statu = false;
                return;
        }

        ws2812_power_on();
        k_msleep(100);
        light_up_ws2812(c1, c2, c3, c4);
        top_pad_run_mode = true;
        data->prev_xw12a_value = get_xw12a_pad_value(dev);

    } else {

        uint8_t top_func_pad = cut_xw12a_data(get_xw12a_pad_value(dev), 8);     

        uint8_t top_first_last_func_pad = data->top_first_last_pad + top_func_pad;

        uint32_t top_pad_combo = top_dict_addr_padx(top_first_last_func_pad);  

        key_tap(dev, top_pad_combo);

        k_msleep(200);

    }

    data->top_prev_time -= TAP_TAP_GAP;
    data->pad_action_statu = false;

};

// --- 核心状态检测函数 ---
static void pad_statu_detect(const struct device *dev)
{
    struct xw12a_data *data = dev->data;

    if (data->pad_action_statu){
        return;
    }

    //struct xw12a_data *data = dev->data;

    uint16_t xw12a_pad_value = get_xw12a_pad_value(dev);

    if (xw12a_pad_value == 0xFFFF ){
        data->prev_xw12a_value = 0xFFFF;
        return;
    }

    //  比较前后两种 touch pad 状态来进行判定
    uint16_t prev_current_pad_compare = data->prev_xw12a_value ^ xw12a_pad_value;

    // left pad 动作的判定
    if ( cut_xw12a_data(prev_current_pad_compare, 12) != 0x00 ){

        if ( cut_xw12a_data(xw12a_pad_value, 12) != 0x00 ){

            if ( k_uptime_get() - data->left_prev_time <= TAP_TAP_GAP ){

                data->front_final_pad = cut_xw12a_data(xw12a_pad_value, 12);
                front_pad_action(dev);

            } else {   

                data->front_first_pad = cut_xw12a_data(xw12a_pad_value, 12);

            }

            data->left_prev_time = k_uptime_get();
        }
    } 

    if ( cut_xw12a_data(prev_current_pad_compare, 8) != 0x00 ){
        if ( top_pad_run_mode == false ){

            if ( cut_xw12a_data(xw12a_pad_value, 8) != 0x00 ){

                if ( k_uptime_get() - data->top_prev_time <= TAP_TAP_GAP ){

                    data->top_final_pad = cut_xw12a_data(xw12a_pad_value, 8);
                    top_pad_action(dev);

                } else {

                    data->top_first_pad = cut_xw12a_data(xw12a_pad_value, 8);

                }

                data->top_prev_time = k_uptime_get();
            }

        } else {

            top_pad_action(dev);
        }
    }

    data->prev_xw12a_value = get_xw12a_pad_value(dev);

    //这里我不确定是如何操作的，如果有反作用，尝试注释掉它
    uint8_t front_pad = cut_xw12a_data(xw12a_pad_value, 12);
    uint8_t top_pad  = cut_xw12a_data(xw12a_pad_value, 8);
    if (front_pad != 0x0F || top_pad != 0x0F) {
        if (xw12a_pad_value != 0xFFFF) {
            k_work_reschedule(&data->keep_alive_dwork, K_MSEC(10));
        }
    }


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

    // --- 初始化这个实例的私有变量 ---
    data->pad_action_statu = false;
    data->prev_xw12a_value = 0xFFFF;
    data->left_prev_time = 0;
    data->front_first_pad = 0x0F;
    data->front_final_pad = 0x0F;
    data->top_prev_time = 0;
    data->top_first_pad = 0x0F;
    data->top_final_pad = 0x0F;
    data->top_first_last_pad = 0x0F;

    if (!device_is_ready(config->i2c.bus) || !device_is_ready(config->int_gpio.port)) {
        return -ENODEV;
    }

    gpio_pin_configure_dt(&config->int_gpio, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&config->int_gpio, GPIO_INT_EDGE_BOTH);

    k_work_init(&data->work, xw12a_work_handler);
    gpio_init_callback(&data->gpio_cb, xw12a_gpio_callback, BIT(config->int_gpio.pin));
    gpio_add_callback(config->int_gpio.port, &data->gpio_cb);

    // --- 新增：初始化并启动心跳定时器 ---
    k_work_init_delayable(&data->keep_alive_dwork, xw12a_keep_alive_work_handler);

    if (use_touch == 1) {
        // 安排 15 秒后进行第一次“心跳”
        k_work_schedule(&data->keep_alive_dwork, K_SECONDS(15));
    }

    return 0;
}

// --- 自动化分身宏 ---
// 这个宏就像一个模具，传入 n=0，它就造出第 0 号芯片的内存和配置；传入 n=1，它就造出第 1 号的。
#define XW12A_INST(n)                                              \
    static struct xw12a_data xw12a_data_##n;                       \
    static const struct xw12a_config xw12a_config_##n = {          \
        .i2c = I2C_DT_SPEC_INST_GET(n),                            \
        .int_gpio = GPIO_DT_SPEC_INST_GET(n, int_gpios),           \
    };                                                             \
    DEVICE_DT_INST_DEFINE(n, xw12a_init, NULL,                     \
                          &xw12a_data_##n, &xw12a_config_##n,      \
                          POST_KERNEL, 99, NULL);

// 编译器大管家：去读取你的 .dts 文件，发现几个 "okay" 状态的 xw12a，就自动运行几次上面的模具！
DT_INST_FOREACH_STATUS_OKAY(XW12A_INST)