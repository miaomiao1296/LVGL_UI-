#ifndef __KEY_H
#define __KEY_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/*=============================
  1. 用户自定义宏
==============================*/
#define KEY_COUNT 4   // 按键数量

/*=============================
  2. 类型定义
==============================*/
typedef enum {
    KEY_IDLE,
    KEY_DEBOUNCE,
    KEY_PRESSED,
    KEY_REPEAT,
    KEY_RELEASE_DEBOUNCE
} KeyState;

typedef enum {
    KEY_EVENT_PRESS,
    KEY_EVENT_HOLD,
    KEY_EVENT_REPEAT,
    KEY_EVENT_RELEASE
} KeyEvent;

typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} KeyConfig;

/*=============================
  3. 公共API声明
==============================*/
void Key_Init(void);
void Key_Scan(void);


#endif

