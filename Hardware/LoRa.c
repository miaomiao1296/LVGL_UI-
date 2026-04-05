#include "LoRa.h"


uint8_t buff[128] = {0};      //接收缓冲区
uint8_t buf_count = 0;        //接收字符计数

// 临时变量用于逐字节接收
 uint8_t rx_byte;


// USART6初始化函数）
// bound: 波特率（e.g., 9600）
void USART6_Config(uint32_t bound)
{   
    // 设置波特率（如果需要动态改变）
    huart6.Init.BaudRate = bound;
    if (HAL_UART_Init(&huart6) != HAL_OK) {
        Error_Handler();
    }   
   
    HAL_UART_Receive_IT(&huart6, &rx_byte, 1);   // 启动逐字节中断接收
    __HAL_UART_ENABLE_IT(&huart6, UART_IT_IDLE);   // 启用 IDLE 中断
}



// USART6字符串发送函数）
void USART6_SEND(uint8_t* data, uint16_t length)
{
    HAL_UART_Transmit(&huart6, data, length, HAL_MAX_DELAY);
}
// USART6_SEND((uint8_t*)"Hello LoRa\r\n", 12);


	
/*************************
     接收控制
  1 LED 开    2 LED 灭
***************************/
void rx_control(void)
{
	  char response[32] = {0};      //回传缓冲区
    
    // 假设 buff 是字符串，检查长度和结束符
    if (buf_count > 2 && buff[buf_count-2] == '\r' && buff[buf_count-1] == '\n') {
		
        buff[buf_count-2] = '\0';  // 去掉 \r\n
        
        // 解析命令
        if (strcmp((const char*)buff, "LED ON") == 0)             //比较
		{
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
            strcpy(response, "OK\r\n");                            //复制
        } 
		
		else if (strcmp((const char*)buff, "LED OFF") == 0)
    	{
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
            strcpy(response, "OK\r\n");            
        } 	
		else 
		{
            strcpy(response, "ERROR Yuan Shen Qi Dong !\r\n");
        }
		
        USART6_SEND((uint8_t*)response, strlen(response));    // 回传响应
        memset(buff,0,buf_count);        //清空接收缓冲区
	    buf_count = 0;                   //接收计数清0
    } 
	
	else {      // 无效帧，清空缓冲区加计数
		memset(buff,0,buf_count);        
	    buf_count = 0;                   
    }
}


