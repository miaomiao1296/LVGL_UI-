#ifndef FFT_H
#define FFT_H

#include "arm_math.h"
#include <stdint.h>

// --- 1. 配置参数 ---
#define SAMPLE_SIZE 1024
#define FFT_SIZE    1024
// 注意：该频率必须与你的 TIM3 触发频率严格对齐
#define SAMPLING_FREQ  10000.0f  

// --- 2. 数据结构：保存全维度的分析结果 ---
typedef struct {
    float32_t freq_main;    // 基波频率 (Hz)
    float32_t amp_main;     // 基波振幅 (V)
    float32_t thd;          // 总谐波失真 (%)
    float32_t vdc;          // 直流分量 (V)
    float32_t vpp;          // 峰峰值 (V)
    float32_t vrms;         // 有效值 (V)
    float32_t *spectrum;    // 指向幅值谱数组 (用于 UI 绘图)
    uint8_t   is_busy;      // 忙标志位 (防止计算中途被读取)
} fft_result_t;

// --- 3. 外部接口 ---
void fft_module_init(void);                 // 初始化：申请内存、预计算窗
void fft_module_execute(uint16_t *adc_raw); // 核心：执行 FFT 分析与指标提取
fft_result_t* fft_get_result(void);         // 获取结果结构体指针

#endif