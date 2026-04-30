#include "FFT.h"
#include <math.h>

// --- 内部私有变量 (CCM 存储) ---
// 1. 关于对齐：类型是 float32_t（本身就是4字节），编译器会自动在内存里给它分配 4 字节对齐的地址。
// 但对于 CMSIS-DSP 库的 FFT 这种吃性能的模块，加一句 aligned(4) (甚至是 aligned(8)) 能绝对保证 FPU 存取效率最高。
// 2. 关于指针：之前我们用指针是因为内存是运行时“要”来的，现在已经是确凿的静态数组了，直接把数组名字改成我们想要的就行，彻底干掉脱裤子放屁的指针！

__attribute__((section(".ccmram"), aligned(4))) static float32_t fft_input_buf[FFT_SIZE * 2];  // 复数缓冲区 [R, I, R, I...]
__attribute__((section(".ccmram"), aligned(4))) static float32_t fft_output_buf[FFT_SIZE];     // 幅值谱缓冲区
__attribute__((section(".ccmram"), aligned(4))) static float32_t hanning_window[SAMPLE_SIZE];  // 预计算窗

static fft_result_t result;

// 暴力强行引用 CMSIS-DSP 实例 (解决头文件包含问题)
extern const arm_cfft_instance_f32 arm_cfft_sR_f32_len1024;

/**
 * @brief 模块初始化
 */
void fft_module_init(void) {
    // 使用静态分配，不再需要 other_ccm_alloc
    static int initialized = 0;
    if (!initialized) {
        // 预计算汉宁窗
        for (int i = 0; i < SAMPLE_SIZE; i++) {
            hanning_window[i] = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * i / (SAMPLE_SIZE - 1)));
        }
        initialized = 1;
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

    // 1. 先计算直流分量(DC Offset)以防止加窗后导致频谱泄漏 (Spectral Leakage)
    // 为什么要这样做？因为 ADC 采样进来常常带有硬件的直流偏置（例如1.65V）
    // 如果带着这个几十数百倍于交流信号的偏置直接加窗，窗函数会将这巨大的直流调制成低频交流分量，导致低频段“炸开”
    float32_t dc_sum = 0;
    for (int i = 0; i < SAMPLE_SIZE; i++) {
        dc_sum += (float32_t)adc_raw[i];
    }
    float32_t dc_offset = dc_sum / SAMPLE_SIZE;

    // 保存直流电压 (用 ADC 参考电压和分辨率转换为实际电压值)
    result.vdc = dc_offset * (3.3f / 4095.0f);

    // 2. 去除直流、数据搬运与加窗
    // 剔除 DC 偏置后，数据变成了以 0 为中心的纯交流信号，这时候再乘上汉宁窗，能极大压制频谱中非周期信号的边缘跳变
    for (int i = 0; i < SAMPLE_SIZE; i++) {
        float32_t ac_val = (float32_t)adc_raw[i] - dc_offset;
        fft_input_buf[2 * i] = ac_val * hanning_window[i];
        fft_input_buf[2 * i + 1] = 0.0f; // 虚部为0
    }

    // 3. 运行 CMSIS-DSP 核心运算
    // 执行基4浮点 FFT，然后计算各频率点的复数模值（幅值）
    arm_cfft_f32(&arm_cfft_sR_f32_len1024, fft_input_buf, 0, 1);
    arm_cmplx_mag_f32(fft_input_buf, fft_output_buf, FFT_SIZE);

    // 4. 能量还原与归一化 (交流补偿, Index 1...N/2)
    // 为什么乘 4.0f？因为 N 点 FFT 后幅值为原始的 N/2 倍，需乘系数 2.0/N。加上汉宁窗后，幅值会损失一半（相干增益为0.5），所以再乘以2，总系数就是 4.0/N
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        fft_output_buf[i] = (fft_output_buf[i] * 4.0f) / FFT_SIZE;
    }

    // 5. 分析最大峰 (基波寻峰)
    float32_t max_val = 0; int max_idx = 0;
    // 直流已经被去除，并且频谱被隔离，现在可以直接从 Index 1 开始找交流的最大峰值
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        if (fft_output_buf[i] > max_val) {
            max_val = fft_output_buf[i];
            max_idx = i;
        }
    }

    // 6. 获取精确结果
    // 使用高斯/抛物线插值，突破 FFT 的频率分辨率（Fs/N = 10kHz/1024 ≈ 9.77Hz）限制，获得亚分辨率级的精准频率

    float32_t p_idx = get_precise_index(max_idx);
    result.freq_main = p_idx * (SAMPLING_FREQ / FFT_SIZE);
    result.amp_main  = get_precise_amplitude(max_idx) * (3.3f / 4095.0f);

    // 7. 派生指标计算 (Vpp, Vrms, THD)
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
