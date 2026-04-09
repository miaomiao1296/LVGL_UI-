import numpy as np
import matplotlib.pyplot as plt
import os

# --- 硬件参数配置区 ---
# 为什么要加这些？把代码里的“魔法数字”变成物理意义上的实际数值，
# 这样画出来的图不仅是干瘪的数组索引，而是真实的“时间(ms)”和“频率(Hz)”！
FS = 10000.0         # 采样频率(Hz) - 根据你的项目(10kHz)推测
N_POINTS = 1024      # FFT 点数
V_REF = 3.3          # ADC 参考电压(V)
ADC_RES = 4095.0     # 12-bit ADC 最大值

adc_file = 'adc_data.bin'
fft_file = 'fft_data.bin'

has_adc = os.path.exists(adc_file)
has_fft = os.path.exists(fft_file)

if not has_adc and not has_fft:
    print("未找到任何 .bin 文件！请先在 GDB 中导出数据。")
    print("导出 ADC 命令 (在 GDB Console 输入):")
    print("dump binary memory tools/adc_data.bin &adc_buffer[0] &adc_buffer[1024]")
    print("导出 FFT 命令 (在 GDB Console 输入):")
    print("dump binary memory tools/fft_data.bin &fft_output_buf[0] &fft_output_buf[512]")
else:
    # 设置中文字体（部分电脑可能需要，如果标题变方块可以忽略）
    plt.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'Arial Unicode MS'] 
    plt.rcParams['axes.unicode_minus'] = False

    # 创建上下两层图表
    fig, axs = plt.subplots(2, 1, figsize=(12, 9))
    
    # --- 1. 绘制上半区: ADC 时域波形 ---
    if has_adc:
        adc_data = np.fromfile(adc_file, dtype=np.uint16)
        
        # 优化1：将 ADC 值转换成真实的电压值 (V)
        voltage_data = adc_data * (V_REF / ADC_RES)
        
        # 优化2：将 X 轴变成真实的时间轴 (ms)
        time_axis = np.arange(N_POINTS) * (1000.0 / FS)
        
        axs[0].plot(time_axis, voltage_data, label='ADC 实时波形', color='#2ca02c', linewidth=1.2)
        axs[0].set_title(f'时域波形 (Time Domain) - 采样率: {FS/1000} kHz')
        axs[0].set_xlabel('时间 (ms)')
        axs[0].set_ylabel('电压 (V)')
        axs[0].set_ylim([-0.1, V_REF + 0.1])
        axs[0].grid(True, linestyle='--', alpha=0.6)
        axs[0].legend(loc='upper right')
    else:
        axs[0].text(0.5, 0.5, '未找到 adc_data.bin', ha='center', fontsize=12)
        axs[0].set_title('时域波形 (Time Domain)')

    # --- 2. 绘制下半区: FFT 频域频谱 ---
    if has_fft:
        fft_data = np.fromfile(fft_file, dtype=np.float32)
        
        # 优化3：计算真实的频率轴 X (Hz)
        freq_axis = np.arange(512) * (FS / N_POINTS)
        
        # 为了更清楚看交流分量，忽略 DC（索引0）
        # 优化4：如果你在 C 代码里除以了 4095，这里不用管；
        # 但我们之前 C 代码算出来的是缩放过的数字，这里依然可以直接画出它的幅值
        axs[1].plot(freq_axis[1:512], fft_data[1:512], label='FFT 交流频谱', color='#1f77b4')
        
        # --- 优化5：自动寻峰与谐波标注 ---
        search_start = 1 # C代码中也是从 1 开始寻找交流峰值
        peak_idx = np.argmax(fft_data[search_start:512]) + search_start
        peak_mag = fft_data[peak_idx]
        peak_freq = freq_axis[peak_idx]
        
        # 标记基波
        axs[1].plot(peak_freq, peak_mag, 'ro', markersize=6) 
        axs[1].annotate(f' 基波 (1st)\n {peak_freq:.1f} Hz\n 峰值: {peak_mag:.2f}', 
                        xy=(peak_freq, peak_mag), 
                        xytext=(peak_freq + (FS*0.02), peak_mag * 0.95),
                        fontsize=11, color='red',
                        arrowprops=dict(facecolor='red', shrink=0.05, width=1.5, headwidth=6))

        # 试着标记 2次 和 3次 谐波（如果有的话），用来做 THD 直观显示
        for harmonic in [2, 3]:
            h_idx = peak_idx * harmonic
            if h_idx < 512:
                h_freq = freq_axis[h_idx]
                h_mag = fft_data[h_idx]
                # 只有谐波明显时才标出
                if h_mag > peak_mag * 0.05:
                    axs[1].plot(h_freq, h_mag, 'go', markersize=5)
                    axs[1].text(h_freq, h_mag + peak_mag*0.05, f'{harmonic}次\n{h_mag:.2f}', 
                                color='green', ha='center', fontsize=9)

        axs[1].set_title('频域频谱 (Frequency Domain)')
        axs[1].set_xlabel('频率 (Hz)')
        axs[1].set_ylabel('幅值 (归一化相对值)')
        axs[1].grid(True, linestyle='--', alpha=0.6)
        axs[1].legend(loc='upper right')
        
        # 自动限制X轴，不一定要画到奈奎斯特频率，只看重点即可
        axs[1].set_xlim([0, peak_freq * 5 if peak_freq * 5 < FS/2 else FS/2])
    else:
        axs[1].text(0.5, 0.5, '未找到 fft_data.bin', ha='center', fontsize=12)
        axs[1].set_title('频域频谱 (Frequency Domain)')

    plt.tight_layout() 
    # 自动保存一张截图以备归档
    plt.savefig('plot_result.png', dpi=150)
    print("图表已渲染并保存为 'plot_result.png'")
    plt.show()

