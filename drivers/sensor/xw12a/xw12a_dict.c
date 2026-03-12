#include <dt-bindings/zmk/keys.h>

/* 给 CANCEL 定义一个不在 HID 标准的 uint32_t 类型值
，key_tap 才能实现反转 top_pad_run_mode 布尔值*/
#define CANCEL 0x0F000001 

// front pad 的各种操作
#define PAD_ZERO_ZERO    (LS(LC(Z)))
#define PAD_ONE_ONE      (DELETE)
#define PAD_TWO_TWO      (BACKSPACE)
#define PAD_THREE_THREE  (LC(Z))
#define PAD_ZERO_ONE     (DOWN)
#define PAD_ONE_ZERO     (UP)
#define PAD_TWO_THREE    (RIGHT)
#define PAD_THREE_TWO    (LEFT)
#define PAD_ZERO_THREE   (END)
#define PAD_THREE_ZERO   (HOME)


// top pad 的各种操作
#define PAD_FOUR_X_FIVE_FOUR      (C_BRIGHTNESS_INC)
#define PAD_FOUR_X_FIVE_FIVE      (C_BRIGHTNESS_DEC)
#define PAD_FOUR_X_FIVE_SIX       (CANCEL)
#define PAD_FOUR_X_FIVE_SEVEN     (CANCEL)

#define PAD_FIVE_X_SIX_FOUR       (CANCEL)
#define PAD_FIVE_X_SIX_FIVE       (C_REWIND)
#define PAD_FIVE_X_SIX_SIX        (C_FAST_FORWARD)
#define PAD_FIVE_X_SIX_SEVEN      (CANCEL)

#define PAD_SIX_X_SEVEN_FOUR      (CANCEL)
#define PAD_SIX_X_SEVEN_FIVE      (C_MUTE)
#define PAD_SIX_X_SEVEN_SIX       (C_VOLUME_DOWN)
#define PAD_SIX_X_SEVEN_SEVEN     (C_VOLUME_UP)

#define PAD_FOUR_X_SEVEN_FOUR     (C_PREV)
#define PAD_FOUR_X_SEVEN_FIVE     (CANCEL)
#define PAD_FOUR_X_SEVEN_SIX      (C_PLAY_PAUSE)
#define PAD_FOUR_X_SEVEN_SEVEN    (C_NEXT)

#define PAD_FIVE_X_SEVEN_FOUR     (LC(N0))
#define PAD_FIVE_X_SEVEN_FIVE     (LC(MINUS))
#define PAD_FIVE_X_SEVEN_SIX      (CANCEL)
#define PAD_FIVE_X_SEVEN_SEVEN    (LC(PLUS))

#define PAD_FOUR_X_SIX_FOUR       (LG(PLUS))
#define PAD_FOUR_X_SIX_FIVE       (CANCEL)
#define PAD_FOUR_X_SIX_SIX        (LG(MINUS))
#define PAD_FOUR_X_SIX_SEVEN      (LG(ESC))


//extern bool left_pad_statu;


// 构建通用 dict 结构
struct dict_struct {
    uint8_t  dict_key;  // 寄存器地址 (你提到的 0x38, 0x84 是 1 byte)
    uint32_t dict_value;   // 对应的按键值
};

// pad0 pad1 pad2 pad3 使用 front_pad_dict 字典
static const struct dict_struct front_pad_dict[] = {
    { 0x38, PAD_ZERO_ZERO   },
    { 0x84, PAD_ONE_ONE     },
    { 0xB6, PAD_TWO_TWO     },
    { 0xD2, PAD_THREE_THREE },
    { 0x80, PAD_ZERO_ONE    },
    { 0x3C, PAD_ONE_ZERO    },
    { 0xD1, PAD_TWO_THREE   },
    { 0xB7, PAD_THREE_TWO   },
    { 0xCB, PAD_ZERO_THREE  },
    { 0x3F, PAD_THREE_ZERO  },
    
};


// 紧跟其后，自动计算长度
#define FRONT_PAD_DICT_SIZE (sizeof(front_pad_dict) / sizeof(front_pad_dict[0]))

// 然后在查找函数里使用它
uint32_t left_dict_addr_padx(uint8_t padx) {
    for (int i = 0; i < FRONT_PAD_DICT_SIZE; i++) { // 永远是正确的次数
        if (front_pad_dict[i].dict_key == padx) {
            return front_pad_dict[i].dict_value;
        }
    }
    //left_pad_statu = false;
    return 0;
}


// pad8 pad9 pad10 pad11 在 top_pad_run_mode == true 状态下使用的字典
// 如果新的 xw12a 芯片正常的话，这里要做出修改
static const struct dict_struct top_pad_dict[] = {
    // 屏幕亮度控制
    { 0x54, PAD_FOUR_X_FIVE_FOUR     },
    { 0x58, PAD_FOUR_X_FIVE_FIVE     },
    { 0x5A, PAD_FOUR_X_FIVE_SIX      },
    { 0x5B, PAD_FOUR_X_FIVE_SEVEN    },

    // 快进快退控制
    { 0x96, PAD_FIVE_X_SIX_FOUR      },
    { 0x9A, PAD_FIVE_X_SIX_FIVE      },
    { 0x9C, PAD_FIVE_X_SIX_SIX       },
    { 0x9D, PAD_FIVE_X_SIX_SEVEN     },

    // 音量控制
    { 0xBD, PAD_SIX_X_SEVEN_FOUR     },
    { 0xC1, PAD_SIX_X_SEVEN_FIVE     },
    { 0xC3, PAD_SIX_X_SEVEN_SIX      },
    { 0xC4, PAD_SIX_X_SEVEN_SEVEN    },

    // 媒体控制
    { 0x69, PAD_FOUR_X_SEVEN_FOUR    },
    { 0x6D, PAD_FOUR_X_SEVEN_FIVE    },
    { 0x6F, PAD_FOUR_X_SEVEN_SIX     },
    { 0x70, PAD_FOUR_X_SEVEN_SEVEN   },

    // 放大缩小页面
    { 0xA1, PAD_FIVE_X_SEVEN_FOUR    },
    { 0xA5, PAD_FIVE_X_SEVEN_FIVE    },
    { 0xA7, PAD_FIVE_X_SEVEN_SIX     },
    { 0xA8, PAD_FIVE_X_SEVEN_SEVEN   },

    // 放大缩小桌面
    { 0x62, PAD_FOUR_X_SIX_FOUR      },
    { 0x66, PAD_FOUR_X_SIX_FIVE      },
    { 0x68, PAD_FOUR_X_SIX_SIX       },
    { 0x69, PAD_FOUR_X_SIX_SEVEN     },
};

// 紧跟其后，自动计算长度
#define TOP_PAD_DICT_SIZE (sizeof(top_pad_dict) / sizeof(top_pad_dict[0]))

// 然后在查找函数里使用它
uint32_t top_dict_addr_padx(uint8_t padx) {
    for (int i = 0; i < TOP_PAD_DICT_SIZE; i++) { // 永远是正确的次数
        if (top_pad_dict[i].dict_key == padx) {
            return top_pad_dict[i].dict_value;
        }
    }
    return 0; // 因为 top_pad_action 所有的组合都被使用了，所以没有 dict_key 值为空的情况
}