#include "effect.h"
#include <string.h>

#define TRAIL_MAX_POINTS   20  // 轨迹最大点数（同时存在的红点数）
static lv_obj_t *trail_objs[TRAIL_MAX_POINTS];  // 存储轨迹对象的指针数组

// 初始化函数，清空数组
void effect_init(void) {
    memset(trail_objs, 0, sizeof(trail_objs));  // 所有指针设NULL
} 

// 动画执行回调：设置对象透明度
static void anim_opa_cb(void *var, int32_t v) {
    lv_obj_set_style_bg_opa((lv_obj_t*)var, v, 0);  // var是对象，v是opa值
}

// 动画结束回调：删除对象，清指针
static void anim_delete_cb(lv_anim_t *a) {
    int idx = (int)a->user_data;  // 从动画数据取索引
    lv_obj_del(trail_objs[idx]);  // 删除LVGL对象
    trail_objs[idx] = NULL;  // 清指针，释放数组位置
}

// 触摸移动时调用：创建轨迹点
void effect_on_move(uint16_t x, uint16_t y) {
    for (int i = 0; i < TRAIL_MAX_POINTS; i++) {  // 遍历数组找空位置
        if (!trail_objs[i]) {  // 如果指针NULL，说明位置空
            trail_objs[i] = lv_obj_create(lv_scr_act());  // 创建新对象，加到主屏幕
            lv_obj_set_size(trail_objs[i], 6, 6);  // 设大小6x6（小圆点）
            lv_obj_set_style_bg_color(trail_objs[i], lv_palette_main( LV_PALETTE_BLUE), 0);  // 画点蓝
            lv_obj_set_style_radius(trail_objs[i], LV_RADIUS_CIRCLE, 0);  // 圆角（圆形）
            lv_obj_clear_flag(trail_objs[i], LV_OBJ_FLAG_SCROLLABLE);  // 不滚动
            lv_obj_set_pos(trail_objs[i], x - 3, y - 3);  // 位置居中（x-3,y-3）

            lv_anim_t a;  // 创建动画
            lv_anim_init(&a);  // 初始化
            lv_anim_set_var(&a, trail_objs[i]);  // 动画对象
            lv_anim_set_values(&a, 255, 0);  // opa从255到0（渐隐）
            lv_anim_set_time(&a, 400);  // 400ms
            lv_anim_set_exec_cb(&a, anim_opa_cb);  // 执行回调
            lv_anim_set_ready_cb(&a, anim_delete_cb);  // 结束回调
            lv_anim_set_user_data(&a, (void*)i);  // 存索引
            lv_anim_start(&a);  // 启动动画
            break;  // 找到位置就停
        }
    }
}

