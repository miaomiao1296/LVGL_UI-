#include "encoder.h"
#include "main.h"

// 对应你在 CubeMX 中配置的 TIM4 句柄
extern TIM_HandleTypeDef htim4;

#define SCAN_INTERVAL    10      // 扫描间隔 (ms)
#define ENCODER_STEP     4       // 编码器步长（4倍频通常为4个脉冲计1次）

/* 内部变量 */
static uint16_t last_cnt = 0;
static int16_t  lv_encoder_diff = 0;
static lv_indev_state_t lv_encoder_state = LV_INDEV_STATE_REL;

/**
 * @brief 初始化编码器硬件
 */
void Encoder_Init(void)
{
    // 将计数器初始化在中间位置，防止溢出处理复杂化
    __HAL_TIM_SET_COUNTER(&htim4, 32768);
    last_cnt = 32768;
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
}

/**
 * @brief 编码器扫描函数。
 * 建议在 main 循环中调用，或者在 10ms 的定时器中断中调用。
 */
void Encoder_Scan(void)
{
    static uint32_t last_scan_time = 0;
    uint32_t current_time = HAL_GetTick();
    
    // 限制扫描频率
    if (current_time - last_scan_time < SCAN_INTERVAL) return;
    last_scan_time = current_time;
    
    /* 1. 处理旋转逻辑 */
    uint16_t current_cnt = __HAL_TIM_GET_COUNTER(&htim4);
    int16_t delta = (int16_t)(current_cnt - last_cnt);
    
    if (delta >= ENCODER_STEP)
    {
        lv_encoder_diff += (delta / ENCODER_STEP); // 累加旋转增量
        last_cnt = current_cnt;
    }
    else if (delta <= -ENCODER_STEP)
    {
        lv_encoder_diff += (delta / ENCODER_STEP); // 负数增量
        last_cnt = current_cnt;
    }

    /* 2. 处理按键逻辑 */
    // 请根据你的实际原理图修改引脚 (使用 PD11)
    // 如果没有按键，可忽略此段，lv_encoder_state 保持 REL
    if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_11) == GPIO_PIN_RESET) {
        lv_encoder_state = LV_INDEV_STATE_PR;  // 按下
    } else {
        lv_encoder_state = LV_INDEV_STATE_REL; // 释放
    }
}

/**
 * @brief 供 LVGL read_cb 调用：获取并清零增量
 */
int16_t Encoder_Get_Diff(void)
{
    int16_t diff = lv_encoder_diff;
    lv_encoder_diff = 0;
    return diff;
}

/**
 * @brief 供 LVGL read_cb 调用：获取当前按键状态
 */
lv_indev_state_t Encoder_Get_State(void)
{
    return lv_encoder_state;
}
