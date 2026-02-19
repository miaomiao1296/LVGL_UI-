#ifndef _SPI_FLASH_H__
#define _SPI_FLASH_H__

#include "stm32f4xx_hal.h"
#include <stdbool.h>

// 回调函数类型定义
typedef void (*BSP_Flash_Callback_t)(void);

// 1. 基础控制
void BSP_W25Q_Init(void);

// 2. 读取接口 (Read)
void BSP_W25Q_Fast_Read(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t Size);                             // [同步读取] 适合读取少量配置参数 (阻塞)
void BSP_W25Q_Normal_Read(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t Size);
void BSP_W25Q_Read_DMA(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t Size, BSP_Flash_Callback_t pCallback);       // [异步DMA读取] 适合读取大资源如图片 (非阻塞，极速返回)   数据就绪后，has_dma_busy()变false，且会自动调用 callback (如果非NULL)

// 查询异步读取是否还在忙
bool BSP_W25Q_Is_DMA_Busy(void);

// 3. 智能写入与擦除 (Write & Erase)
bool BSP_W25Q_Write(uint8_t* pBuffer, uint32_t WriteAddr, uint32_t Size); // [智能写入] 阻塞式 // 自动处理页对齐(Page Alignment)和跨页写入，用户无需关心Flash页大小 // 自动处理写使能和忙等待
bool BSP_W25Q_Erase_Range(uint32_t StartAddr, uint32_t Size);   // 用户只需给一个地址范围，底层自动判断： 优先使用64KB块擦除(极快)，边缘部分自动降级为4KB扇区擦除。

// 4. 系统集成 (放入 main.c 的 HAL_SPI_RxCpltCallback)             
void BSP_W25Q_DMA_Callback_Handler(SPI_HandleTypeDef *hspi);
//例：
//void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)  重写 HAL 库的 SPI 接收完成回调
//{  
//    BSP_W25Q_DMA_Callback_Handler(hspi); // 将事件分发给 Flash 驱动
//}

#endif

