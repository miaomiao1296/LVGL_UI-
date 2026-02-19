#ifndef LV_PORT_FS_H
#define LV_PORT_FS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief 初始化 LVGL 虚拟文件系统（挂载 S: 盘）
 */
void lv_port_fs_init(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_PORT_FS_H*/