#ifndef _EFFECT_H_
#define _EFFECT_H_

#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>


/* 接口 */
void effect_init(void);
void effect_on_move(uint16_t x, uint16_t y);


#endif

