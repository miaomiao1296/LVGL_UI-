#include "FFT.h"
#include <math.h>

// 引用外部 CCM 内存分配函数
extern void * other_ccm_alloc(size_t size);

// --- 内部私有变量 (CCM 存储) ---
static float32_t *fft_input_buf;  // 复数缓冲区 [R, I, R, I...]
static float32_t *fft_output_buf; // 幅值谱缓冲区
static float32_t *hanning_window; // 预计算窗
static fft_result_t result;       

// 暴力强行引用 CMSIS-DSP 实例 (解决头文件包含问题)
extern const arm_cfft_instance_f32 arm_cfft_sR_f32_len1024;

/**
 * @brief 模块初始化
 */
void fft_module_init(void) {
    if (fft_input_buf == NULL) {
		
        fft_input_buf  = (float32_t *)other_ccm_alloc(FFT_SIZE * 2 * sizeof(float32_t));
        fft_output_buf = (float32_t *)other_ccm_alloc(FFT_SIZE * sizeof(float32_t));
        hanning_window = (float32_t *)other_ccm_alloc(SAMPLE_SIZE * sizeof(float32_t));

        // 预计算汉宁窗
        for (int i = 0; i < SAMPLE_SIZE; i++) {
            hanning_window[i] = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * i / (SAMPLE_SIZE - 1)));
        }
    }
    result.spectrum = fft_output_buf;
    result.is_busy = 0;
}

/**
 * @brief 内部插值算法
 */
static float32_t get_precise_index(int max_idx) {
    if (max_idx <= 0 || max_idx >= FFT_SIZE / 2) return (float32_t)max_idx;
    float32_t yl = fft_output_buf[max_idx - 1], yc = fft_output_buf[max_idx], yr = fft_output_buf[max_idx + 1];
    return max_idx + 0.5f * (yl - yr) / (yl - 2.0f * yc + yr);
}

static float32_t get_precise_amplitude(int max_idx) {
    if (max_idx <= 0 || max_idx >= FFT_SIZE / 2) return fft_output_buf[max_idx];
    float32_t yl = fft_output_buf[max_idx - 1], yc = fft_output_buf[max_idx], yr = fft_output_buf[max_idx + 1];
    float32_t delta = 0.5f * (yl - yr) / (yl - 2.0f * yc + yr);
    return yc - 0.25f * (yl - yr) * delta;
}

/**
 * @brief 核心执行引擎
 */
void fft_module_execute(uint16_t *adc_raw) {
    result.is_busy = 1;

    // 1. 数据搬运与加窗
    for (int i = 0; i < SAMPLE_SIZE; i++) {
        fft_input_buf[2 * i] = (float32_t)adc_raw[i] * hanning_window[i];      //从SRAM搬运到CCM
        fft_input_buf[2 * i + 1] = 0.0f;
    }

    // 2. 运行 DSP 运算
    arm_cfft_f32(&arm_cfft_sR_f32_len1024, fft_input_buf, 0, 1);
    arm_cmplx_mag_f32(fft_input_buf, fft_output_buf, FFT_SIZE);

    // 3. 能量还原与归一化
    // 直流补偿 (Index 0)
    float32_t dc_raw = (fft_output_buf[0] / FFT_SIZE) * 2.0f;
    result.vdc = dc_raw * (3.3f / 4095.0f);
    
    // 交流补偿 (Index 1...N/2)
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        fft_output_buf[i] = (fft_output_buf[i] * 4.0f) / FFT_SIZE;
    }

    // 4. 分析最大峰 (基波)
    float32_t max_val = 0; int max_idx = 0;
    for (int i = 5; i < FFT_SIZE / 2; i++) { // 跳过直流和前几个噪声点
        if (fft_output_buf[i] > max_val) {
            max_val = fft_output_buf[i];
            max_idx = i;
        }
    }

    // 5. 获取结果
    float32_t p_idx = get_precise_index(max_idx);
    result.freq_main = p_idx * (SAMPLING_FREQ / FFT_SIZE);
    result.amp_main  = get_precise_amplitude(max_idx) * (3.3f / 4095.0f);

    // 6. 派生指标计算 (Vpp, Vrms, THD)
    result.vpp = (result.amp_main + (result.amp_main * 0.3f)) * 2.0f; // 模拟版推算        通过基波幅值反推 Vpp，这比在原始数据里找最大最小值（容易受噪声尖峰干扰）要稳健得多。
    
    float32_t h_power = 0;
    for (int h = 2; h <= 10; h++) {
        int h_idx = (int)(p_idx * h + 0.5f);
        if (h_idx < FFT_SIZE / 2) {
            float32_t a = get_precise_amplitude(h_idx);
            h_power += a * a;
        }
    }
    result.thd  = sqrtf(h_power) / get_precise_amplitude(max_idx) * 100.0f;
    result.vrms = sqrtf(result.vdc * result.vdc + (result.amp_main * 0.707f * result.amp_main * 0.707f));

    result.is_busy = 0;
}

fft_result_t* fft_get_result(void) { return &result; }
