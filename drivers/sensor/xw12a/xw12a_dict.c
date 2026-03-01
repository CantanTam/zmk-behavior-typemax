#include <dt-bindings/zmk/keys.h>

/* 给 CANCEL 定义一个不在 HID 标准的 uint32_t 类型值
，key_tap 才能实现反转 top_pad_run_mode 布尔值*/
#define CANCEL 0x0F000001 

//定义各个 PAD_X_Y 的操作：
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

#define PAD_FOUR_FOUR    (BACKSPACE)
#define PAD_FIVE_FIVE    (PAGE_DOWN)
#define PAD_SIX_SIX      (LEFT)
#define PAD_SEVEN_SEVEN  (RIGHT)
#define PAD_FOUR_FIVE    (END)
#define PAD_FIVE_FOUR    (LC(Z))
#define PAD_SIX_SEVEN    (RIGHT)
#define PAD_SEVEN_SIX    (LEFT)

#define PAD_NINE_X_TEN_NINE       (C_PREV)
#define PAD_NINE_X_TEN_TEN        (C_NEXT)
#define PAD_NINE_X_TEN_ELEVEN     (CANCEL)
#define PAD_NINE_X_ELEVEN_NINE    (C_BRIGHTNESS_DEC)
#define PAD_NINE_X_ELEVEN_TEN     (CANCEL)
#define PAD_NINE_X_ELEVEN_ELEVEN  (C_BRIGHTNESS_INC)
#define PAD_TEN_X_ELEVEN_NINE     (CANCEL)
#define PAD_TEN_X_ELEVEN_TEN      (C_VOLUME_DOWN)
#define PAD_TEN_X_ELEVEN_ELEVEN   (C_VOLUME_UP)

//extern bool left_pad_statu;


// 构建通用 dict 结构
struct dict_struct {
    uint8_t  dict_key;  // 寄存器地址 (你提到的 0x38, 0x84 是 1 byte)
    uint32_t dict_value;   // 对应的按键值
};

// pad0 pad1 pad2 pad3 使用 left_pad_dict 字典
static const struct dict_struct left_pad_dict[] = {
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
#define LEFT_PAD_DICT_SIZE (sizeof(left_pad_dict) / sizeof(left_pad_dict[0]))

// 然后在查找函数里使用它
uint32_t left_dict_addr_padx(uint8_t padx) {
    for (int i = 0; i < LEFT_PAD_DICT_SIZE; i++) { // 永远是正确的次数
        if (left_pad_dict[i].dict_key == padx) {
            return left_pad_dict[i].dict_value;
        }
    }
    //left_pad_statu = false;
    return 0;
}



// pad0 pad1 pad2 pad3 使用 right_pad_dict 字典
static const struct dict_struct right_pad_dict[] = {
    { 0x38, PAD_FOUR_FOUR   },
    { 0x84, PAD_FIVE_FIVE   },
    { 0xB6, PAD_SIX_SIX     },
    { 0xD2, PAD_SEVEN_SEVEN },
    { 0x80, PAD_FOUR_FIVE   },
    { 0x3C, PAD_FIVE_FOUR   },
    { 0xD1, PAD_SIX_SEVEN   },
    { 0xB7, PAD_SEVEN_SIX   },
    
};

// 紧跟其后，自动计算长度
#define RIGHT_PAD_DICT_SIZE (sizeof(right_pad_dict) / sizeof(right_pad_dict[0]))

// 然后在查找函数里使用它
uint32_t right_dict_addr_padx(uint8_t padx) {
    for (int i = 0; i < RIGHT_PAD_DICT_SIZE; i++) { // 永远是正确的次数
        if (right_pad_dict[i].dict_key == padx) {
            return right_pad_dict[i].dict_value;
        }
    }
    return 0;
}


// pad8 pad9 pad10 pad11 在 top_pad_run_mode == true 状态下使用的字典
// 如果新的 xw12a 芯片正常的话，这里要做出修改
static const struct dict_struct top_pad_dict[] = {
    // 媒体控制
    { 0x54, PAD_NINE_X_TEN_NINE    }, 
    { 0x58, PAD_NINE_X_TEN_TEN     }, 
    { 0x5A, PAD_NINE_X_TEN_ELEVEN  }, 

    // 亮度控制
    { 0x62, PAD_NINE_X_ELEVEN_NINE   }, 
    { 0x66, PAD_NINE_X_ELEVEN_TEN    }, 
    { 0x68, PAD_NINE_X_ELEVEN_ELEVEN }, 

    // 音量控制
    { 0x96, PAD_TEN_X_ELEVEN_NINE    }, 
    { 0x9A, PAD_TEN_X_ELEVEN_TEN     }, 
    { 0x9C, PAD_TEN_X_ELEVEN_ELEVEN  }, 
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