#ifndef __LCD_H
#define __LCD_H		

#include "main.h"

// --- 方向设置 ---
// 0:竖屏(向上) 1:竖屏(向下) 2:横屏（保守) 3:横屏（改革）
#define USE_HORIZONTAL  3  

#if USE_HORIZONTAL == 0 || USE_HORIZONTAL == 1
    #define LCD_W 240
    #define LCD_H 320
#else
    #define LCD_W 320
    #define LCD_H 240
#endif

// --- 高速 IO 操作 (保持不变) ---
#define LCD_CS_SET    GPIOE->BSRR = GPIO_PIN_6
#define LCD_CS_CLR    GPIOE->BSRR = (uint32_t)GPIO_PIN_6 << 16

#define LCD_RS_SET    GPIOE->BSRR = GPIO_PIN_4
#define LCD_RS_CLR    GPIOE->BSRR = (uint32_t)GPIO_PIN_4 << 16

#define LCD_RST_SET   GPIOE->BSRR = GPIO_PIN_3
#define LCD_RST_CLR   GPIOE->BSRR = (uint32_t)GPIO_PIN_3<< 16

// --- 导出函数 ---
void LCD_Init(void);
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_Write_DMA_Async(const uint8_t* data, uint32_t len);


#endif

