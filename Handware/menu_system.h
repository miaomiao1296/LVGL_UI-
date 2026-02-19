#ifndef __MENU_H
#define __MENU_H

#include "stm32f4xx_hal.h"
#include "Key.h"
#include "main.h"
#include "event.h"
#include "encoder.h"


/* 系统配置结构体 */
typedef struct {
    uint16_t backlight_val; // 范围 0~1000
    // 以后可以在这里加：uint8_t volume;
} System_Config_t;

extern System_Config_t g_sys_config ;



void LCD_Set_Brightness(uint16_t brightness);


void Menu_OnEvent(const Event_t *e);
void font_load (void);



/*==============================
 * 对外接口
 *==============================*/

// 页面 ID 枚举：从 0 开始，方便作为数组下标
typedef enum {
    UI_PAGE_NONE = 0,      // 0 位留空，代表无效状态或上电初始态
    UI_PAGE_HOME,          // 1: 主菜单
    UI_PAGE_IMAGE,         // 2: 图片/相册
    UI_PAGE_SETTING,       // 3: 设置
    UI_PAGE_1,             // 4: 实验页 1
    UI_PAGE_2,             // 5: 实验页 2
    UI_PAGE_3,             // 6: 频谱分析 (DSP 重点)
    UI_PAGE_4,             // 7: 示波器 (实时数据重点)
    UI_PAGE_5,             // 8: 扩展页

    UI_PAGE_COUNT          // 自动计数：当前为 9
} UIPage_t;

/* 在 LVGL 初始化完成后调用一次 */
void UI_Init(void);

/* 前进到指定页面 */
void UI_Goto(UIPage_t page);

/* 返回上一页 */
void UI_Back(void);


#endif
