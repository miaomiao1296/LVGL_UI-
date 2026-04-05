#ifndef __LORA_H
#define __LORA_H

#include "stm32f4xx_hal.h"
#include "main.h"
#include "usart.h"
#include "stdint.h"
#include "string.h"     
#include "stdio.h"

void rx_control(void);
extern uint8_t buf_count;
extern  uint8_t rx_byte ;
extern  uint8_t buff[128];
void USART6_Config(uint32_t bound);
void USART6_SEND(uint8_t* data, uint16_t length);
#endif
