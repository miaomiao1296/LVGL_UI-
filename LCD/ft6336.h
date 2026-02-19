#ifndef __FT6336_H
#define __FT6336_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include "LCD.h"
#include "stdio.h"
#include <stdbool.h>

/* ===== 硬件配置 ===== */
extern I2C_HandleTypeDef hi2c1;

#define FT6336U_I2C_ADDR   (0x38 << 1)

/* GPIO */
#define FT_RST_PORT GPIOE
#define FT_RST_PIN  GPIO_PIN_12

#define FT_INT_PORT GPIOE
#define FT_INT_PIN  GPIO_PIN_15


/* ===== 寄存器 ===== */
#define FT_REG_TD_STATUS   0x02
#define FT_REG_P1_XH       0x03
#define FT_REG_P2_XH       0x09



/* ===== 触摸状态结构 ===== */
typedef struct {
    bool pressed;
    uint8_t  point_num;   // 0 / 1 / 2        触摸点数
    uint16_t x[2];
    uint16_t y[2];
} FT6336U_State_t;    

/* ===== API ===== */
void FT6336U_Init(void);
void FT6336U_Task(void);

/* ===== 全局 ===== */
extern volatile FT6336U_State_t ft6336_state;      //最近一次成功读取到的触摸结果缓存
extern volatile bool ft6336_irq_flag;            //是否有新的触摸事件需要处理

#endif
