#include "LCD.h"
#include "spi.h"

extern SPI_HandleTypeDef hspi2;

/**
 * @brief 极简命令发送
 */
static void LCD_SendCmd(uint8_t cmd) {
    LCD_RS_CLR;
    LCD_CS_CLR;
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 10);
    LCD_CS_SET;
    LCD_RS_SET;
}

/**
 * @brief 极简参数发送
 */
static void LCD_SendData(const uint8_t *data, uint16_t len) {
    LCD_CS_CLR;
    HAL_SPI_Transmit(&hspi2, (uint8_t *)data, len, 10);
    LCD_CS_SET;
}

/**
 * @brief 设置绘图区域
 */
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    uint8_t x_buf[] = {x1 >> 8, x1 & 0xFF, x2 >> 8, x2 & 0xFF};
    uint8_t y_buf[] = {y1 >> 8, y1 & 0xFF, y2 >> 8, y2 & 0xFF};

    LCD_SendCmd(0x2A); LCD_SendData(x_buf, 4);
    LCD_SendCmd(0x2B); LCD_SendData(y_buf, 4);
    LCD_SendCmd(0x2C);
}

/**
 * @brief 异步 DMA 发送 (对接 LVGL)
 */
void LCD_Write_DMA_Async(const uint8_t* data, uint32_t len) {
    LCD_CS_CLR;
    LCD_RS_SET; 
    HAL_SPI_Transmit_DMA(&hspi2, (uint8_t*)data, (uint16_t)len);
}


/**
 * @brief ILI9341 初始化 (完整保留原厂字节序列)
 */
void LCD_Init(void) {
    // 1. 硬件复位
    LCD_RST_SET;  HAL_Delay(50);
    LCD_RST_CLR;  HAL_Delay(100);
    LCD_RST_SET;  HAL_Delay(50);

    // 2. 原厂 ILI9341 驱动配方 (原封不动)
    LCD_SendCmd(0xCF); LCD_SendData((uint8_t[]){0x00, 0xC1, 0x30}, 3);
    LCD_SendCmd(0xED); LCD_SendData((uint8_t[]){0x64, 0x03, 0x12, 0x81}, 4);
    LCD_SendCmd(0xE8); LCD_SendData((uint8_t[]){0x85, 0x00, 0x78}, 3);
    LCD_SendCmd(0xCB); LCD_SendData((uint8_t[]){0x39, 0x2C, 0x00, 0x34, 0x02}, 5);
    LCD_SendCmd(0xF7); LCD_SendData((uint8_t[]){0x20}, 1);
    LCD_SendCmd(0xEA); LCD_SendData((uint8_t[]){0x00, 0x00}, 2);

    LCD_SendCmd(0xC0); LCD_SendData((uint8_t[]){0x13}, 1); // Power 1
    LCD_SendCmd(0xC1); LCD_SendData((uint8_t[]){0x13}, 1); // Power 2
    LCD_SendCmd(0xC5); LCD_SendData((uint8_t[]){0x1C, 0x35}, 2); // VCOM 1
    LCD_SendCmd(0xC7); LCD_SendData((uint8_t[]){0xC8}, 1); // VCOM 2
    LCD_SendCmd(0x21); // Display Inversion ON

    // --- 方向设置核心逻辑 ---
    // 0x36 参数: 0x08(竖屏), 0xC8(180度), 0x68(横屏), 0xA8(270度)
    // 后四位固定为 0x08 确保 BGR 颜色顺序正确                  ///ILI9341 期望收到的是：BGR565 的像素顺序
    uint8_t madctl = (USE_HORIZONTAL == 0) ? 0x08 : 
                     (USE_HORIZONTAL == 1) ? 0xC8 : 
                     (USE_HORIZONTAL == 2) ? 0x68 : 0xA8;
    LCD_SendCmd(0x36); LCD_SendData(&madctl, 1);

    LCD_SendCmd(0xB6); LCD_SendData((uint8_t[]){0x0A, 0xA2}, 2);
    LCD_SendCmd(0x3A); LCD_SendData((uint8_t[]){0x55}, 1); // 16-bit RGB565
    LCD_SendCmd(0xF6); LCD_SendData((uint8_t[]){0x01, 0x30}, 2);
    LCD_SendCmd(0xB1); LCD_SendData((uint8_t[]){0x00, 0x1B}, 2);
    LCD_SendCmd(0xF2); LCD_SendData((uint8_t[]){0x00}, 1);
    LCD_SendCmd(0x26); LCD_SendData((uint8_t[]){0x01}, 1);

    // Gamma 设置
    LCD_SendCmd(0xE0); LCD_SendData((uint8_t[]){0x0F,0x35,0x31,0x0B,0x0E,0x06,0x49,0xA7,0x33,0x07,0x0F,0x03,0x0C,0x0A,0x00}, 15);
    LCD_SendCmd(0xE1); LCD_SendData((uint8_t[]){0x00,0x0A,0x0F,0x04,0x11,0x08,0x36,0x58,0x4D,0x07,0x10,0x0C,0x32,0x34,0x0F}, 15);

    // 3. 退出睡眠并开启显示
    LCD_SendCmd(0x11);
    HAL_Delay(120);
    LCD_SendCmd(0x29);
}
