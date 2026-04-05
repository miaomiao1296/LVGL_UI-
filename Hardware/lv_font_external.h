#ifndef LV_FONT_EXTERNAL_H
#define LV_FONT_EXTERNAL_H

#include "lvgl.h" 
#include "stm32f4xx_hal.h"



/**
 * 外部字库扩展描述符
 */
typedef struct {
    lv_font_fmt_txt_dsc_t lv_dsc; // LVGL 标准描述符
    uint32_t flash_glyf_start;    // glyf 块在 Flash 中的数据起始地址
} lv_font_ext_dsc_t;

/**
 * 核心加载函数：从文件系统加载字库索引到 CCM RAM
 * @param font_name 字库路径，如 "S:font_cn_24.bin"
 * @return 成功返回字库指针，失败返回 NULL
 */
lv_font_t * lv_font_external_load(const char * font_name);

/**
 * 卸载字库
 */
void lv_font_external_free(lv_font_t * font);

#endif 

