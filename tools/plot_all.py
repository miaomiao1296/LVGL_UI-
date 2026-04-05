import numpy as np
import matplotlib.pyplot as plt
import os

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
    plt.rcParams['font.sans-serif'] = ['SimHei'] 
    plt.rcParams['axes.unicode_minus'] = False

    # 创建上下两层图表
    fig, axs = plt.subplots(2, 1, figsize=(10, 8))
    
    # --- 1. 绘制上半区: ADC 时域波形 ---
    if has_adc:
        # ADC 是 uint16 类型
        adc_data = np.fromfile(adc_file, dtype=np.uint16)
        axs[0].plot(adc_data, label='ADC 时域波形 (1024 点)', color='#2ca02c') # 绿色
        axs[0].set_title('时域 (Time Domain) - adc_buffer')
        axs[0].set_ylabel('ADC 离散值 (0-4095)')
        axs[0].grid(True, linestyle='--', alpha=0.6)
        axs[0].legend()
    else:
        axs[0].text(0.5, 0.5, '未找到 adc_data.bin', ha='center', fontsize=12)
        axs[0].set_title('时域 (Time Domain) - adc_buffer')

    # --- 2. 绘制下半区: FFT 频域频谱 ---
    if has_fft:
        # FFT 结果是 float32 类型
        fft_data = np.fromfile(fft_file, dtype=np.float32)
        # 根据奈奎斯特定理，我们通常只看前一半的频谱（第 1 到 512 个点）
        # 如果你想跳过直流分量(第0个点)，可以改成 fft_data[1:512]
        # axs[1].plot(fft_data[:512], label='FFT 频域幅值 (512 点)', color='#1f77b4') # 蓝色
        # 改成这样，直接跳过直流分量，专门看交流波形！
        axs[1].plot(range(1, 512), fft_data[1:512], label='FFT 交流频域幅值 (已丢掉直流偏置)', color='#1f77b4')
        
        # --- 自动寻峰并标注 ---
        search_start = 5 # 对应你 C 代码里的跳过直流和低频噪声阶段
        peak_idx = np.argmax(fft_data[search_start:512]) + search_start
        peak_mag = fft_data[peak_idx]
        
        # 用红点标出，并在旁边写字
        axs[1].plot(peak_idx, peak_mag, 'ro') 
        axs[1].annotate(f' 主频峰值\n Index: {peak_idx}\n 幅值: {peak_mag:.1f}', 
                        xy=(peak_idx, peak_mag), 
                        xytext=(peak_idx + 15, peak_mag * 0.9),
                        fontsize=12, color='red',
                        arrowprops=dict(facecolor='red', shrink=0.05, width=1.5, headwidth=6))

        axs[1].set_title('频域 (Frequency Domain) - fft_output_buf')
        axs[1].set_xlabel('频率索引 (Frequency Bin Index)')
        axs[1].set_ylabel('幅值 (Magnitude)')
        axs[1].grid(True, linestyle='--', alpha=0.6)
        axs[1].legend()
    else:
        axs[1].text(0.5, 0.5, '未找到 fft_data.bin', ha='center', fontsize=12)
        axs[1].set_title('频域 (Frequency Domain) - fft_output_buf')

    plt.tight_layout() # 自动调整间距
    plt.show()

