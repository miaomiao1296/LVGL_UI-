#ifndef __ENCODER_H
#define __ENCODER_H

#include "stm32f4xx_hal.h"
#include "lvgl.h" 

void Encoder_Init(void);
void Encoder_Scan(void);
int16_t Encoder_Get_Diff(void);
lv_indev_state_t Encoder_Get_State(void);
#endif
