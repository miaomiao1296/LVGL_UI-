/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "arm_math.h"
#include "Key.h"
#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include "LCD.h"
#include "spi_flash.h"
#include "encoder.h"
#include "menu_system.h"
#include "LoRa.h"
#include "event.h"
#include "lv_port_fs_w25q.h"        //虚拟文件
#include "lvgl.h"  
#include "ft6336.h" 
#include "lv_port_disp.h"      
#include "lv_port_indev.h"  
#include "effect.h"
#include "FFT.h"
							

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

///*. 为什么不能开满 64KB？
//内存对齐与元数据：编译器和 C 库在处理大数组时，有时会在数组开头或结尾存放一些隐藏的管理信息（元数据）。
//隐形占用：在某些 MDK 默认配置下，即便你没把栈指过去，Heap（堆） 或者 中断向量表的镜像 可能会被默认安置在 CCM 的起始位置。
//链接器冲突：如果你定义了 64KB 数组，链接器（Linker）就没有任何余地去存放其他的 static 变量，这会逼着链接器做出一些“危险的重叠”。
//*/


//// 清空字库
//void lv_font_ccm_reset(void) {
//    font_ptr = 0; 
//   // memset(ccm_pool, 0, 1); // 删开头，CPU直接。死机变量重叠：虽然你代码里写的是两个变量，但链接器可能为了对齐，把 font_ptr 这个 4 字节的变量正好放在了 ccm_pool 数组地址的“影子”里，或者是紧随其后的首个字节。
////误伤命门：执行 memset(0, 1) 时，你抹掉的不是字库，而是字库的“账本”（指针）或者系统的“地图”（重定向的向量表首地址）。
////连锁反应：系统在接下来的微秒内尝试读取这些变量，读到了 0，逻辑瞬间跑飞，触发死机。
//	
//}


/* --- 架构常量定义 --- */
#define CCM_TOTAL_SIZE      (64 * 1024)   // CCM 物理总大小 64KB
#define CCM_SAFE_HEAD       512           // 头部安全区：避开向量表镜像/管理变量
#define CCM_SAFE_TAIL       1024          // 尾部安全区：避开可能的系统栈/链接器标识

/* 实际可用的起止边界 */
#define CCM_START_ADDR      (CCM_SAFE_HEAD)
#define CCM_END_ADDR        (CCM_TOTAL_SIZE - CCM_SAFE_TAIL)

/* --- 物理内存池声明 --- */
// 使用 aligned(4) 确保所有访问都是 4 字节对齐的，提升 CPU 访问效率
__attribute__((section(".ccmram"), aligned(4))) 
static uint8_t ccm_pool[CCM_TOTAL_SIZE];                ////  全局静态变量 (Global Static)// 只有本文件能操作 ccm_pool，保护了内存池不被外部乱踩

/* --- 动态指针 --- */
static uint32_t font_ptr  = CCM_START_ADDR; // 字库分配器指针 (向上增长)
static uint32_t other_ptr = CCM_END_ADDR;   // 其他数据指针 (向下扣除)

/**
 * @brief  字库内存分配器 (向上增长)
 * @param  size: 申请的大小 (字节)
 * @return 成功返回内存指针，失败返回 NULL
 */
void * lv_font_ccm_alloc(size_t size) {
    // 1. 4字节对齐：将 size 向上取整到 4 的倍数
    // 知识点：ARM 架构访问非对齐地址会触发异常或性能下降
    size = (size + 3) & ~3; 

    // 2. 边界检查：检查 [新指针位置] 是否撞上了 [其他数据区]
    if (font_ptr + size >= other_ptr) {
        printf("[CCM Alert] Font allocation overflow! Collision with Data area.\n");
        return NULL;
    }

    // 3. 分配内存并更新指针
    void * p = &ccm_pool[font_ptr];
    font_ptr += size;
    
    return p;
}

/**
 * @brief  其他模块分配器 (向下增长，如 FFT 缓冲区)
 * @param  size: 申请的大小 (字节)
 * @return 成功返回内存指针，失败返回 NULL
 */
void * other_ccm_alloc(size_t size) {
    size = (size + 3) & ~3;

    // 边界检查：检查 [扣除后的新指针] 是否撞上了 [字库区]
    if (other_ptr < CCM_START_ADDR + size || (other_ptr - size) <= font_ptr) {
        printf("[CCM Alert] Data allocation overflow! Collision with Font area.\n");
        return NULL;
    }

    // 4. 先移动指针，再返回起始地址 (因为是向下增长)
    other_ptr -= size;
    return &ccm_pool[other_ptr];
}

/**
 * @brief  逻辑清空字库区
 * @note   知识点：采用“懒清空”策略，只重置指针，不物理 memset，保护系统安全并节省时间
 */
void lv_font_ccm_reset(void) {
    font_ptr = CCM_START_ADDR; // 回到最初的起点
}

/**
 * @brief  CCM 状态监控定时器回调函数 (用于 LVGL 显示)
 */
void ccm_monitor_timer_cb(lv_timer_t * timer) {
    lv_obj_t * label = (lv_obj_t *)timer->user_data;
    if(!label) return;

    // 计算统计数据
    uint32_t f_used = font_ptr - CCM_START_ADDR;  // 字库已用
    uint32_t o_used = CCM_END_ADDR - other_ptr;   // 其他已用
    uint32_t free_space = other_ptr - font_ptr;   // 核心：剩余对撞空隙

    // 计算百分比 (基于可用总空间)
    uint32_t usable_total = CCM_END_ADDR - CCM_START_ADDR;
    uint32_t f_pct = (f_used * 100) / usable_total;
    uint32_t o_pct = (o_used * 100) / usable_total;

    // 格式化输出到界面
    // 知识点：显示剩余空隙(Free)是判断系统稳定性的最重要指标
    lv_label_set_text_fmt(label, 
        "Font: %d%% (%d.1K)\n"
        "Data: %d%% (%d.1K)\n"
        "Free: %d Bytes", 
        (int)f_pct, (int)(f_used / 1024),
        (int)o_pct, (int)(o_used / 1024),
        (int)free_space
    );
}

void custom_ccm_monitor_init(void) {
    // 1. 创建一个小标签，放在最高层 (Layer Top) 以防被其他 UI 遮挡
    lv_obj_t * ccm_label = lv_label_create(lv_layer_sys());
    
    // 2. 设置样式：黑色背景，50%透明度，白色文字（与官方黑框一致）
    lv_obj_set_style_bg_color(ccm_label, lv_palette_main(LV_PALETTE_NONE), 0);
    lv_obj_set_style_bg_opa(ccm_label, LV_OPA_50, 0);
    lv_obj_set_style_text_color(ccm_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(ccm_label, &lv_font_montserrat_14, 0); // 官方用的是 14 号字
    lv_obj_set_style_pad_all(ccm_label, 2, 0); // 挤一点，更像仪表盘
    
    // 3. 设置位置：
    // 官方监视器通常在左上角或右下角。
    //X 偏移：正数向右移，负数向左移。
    //Y 偏移：正数向下移，负数向上移。
    lv_obj_align(ccm_label, LV_ALIGN_TOP_LEFT, 0, 0); 

    // 4. 创建定时器，每 500ms 刷新一次数据
    lv_timer_create(ccm_monitor_timer_cb, 500, ccm_label);
}


/**
 * 链接器符号说明：
 * 从 GCC 链接脚本 (STM32F407XX_FLASH.ld) 引入地标符号。
 */
extern uint32_t _sdata;          // SRAM 数据区开始 (0x20000000)
extern uint32_t _estack;         // 栈顶 (SRAM 结束地址)
extern uint32_t _Min_Stack_Size; // 链接脚本中定义的最小栈大小
extern uint32_t _ebss;           // BSS 段结束地址 (ZI 数据结束)

/**
 * LVGL 定时器回调函数：定期检测并刷新内存占用显示
 */
static void sram_keil_timer_cb(lv_timer_t * timer) {
    // 获取传递进来的 Label 对象句柄，用于后面改文字
    lv_obj_t * label = (lv_obj_t *)timer->user_data;

    /* --- 1. 获取基础硬件参数 --- */
    uint32_t sram_base   = (uint32_t)&_sdata;
    uint32_t sram_limit  = (uint32_t)&_estack;
    uint32_t stack_size  = (uint32_t)&_Min_Stack_Size;

    // 动态计算总 SRAM 大小
    uint32_t total_sram_kb = 128; // STM32F407VG 有 128KB RAM

    /* --- 2. 寻找栈的历史最高点 --- */
    uint32_t * stack_bottom_ptr = (uint32_t *)(sram_limit - stack_size);
    uint32_t peak_sp_addr = sram_limit;

    for (uint32_t i = 0; i < (stack_size / 4); i++) {
        if (stack_bottom_ptr[i] != 0xAAAAAAAA) {
            peak_sp_addr = (uint32_t)&stack_bottom_ptr[i];
            break;
        }
    }

    /* --- 3. 计算栈的峰值数据 --- */
    uint32_t peak_used = sram_limit - peak_sp_addr;
    uint32_t peak_pct  = (peak_used * 100) / stack_size;

    /* --- 4. 计算 SRAM 整体占用 --- */
    uint32_t static_end = (uint32_t)&_ebss;
    uint32_t total_used_bytes = (static_end - sram_base) + peak_used;
    uint32_t total_pct = (total_used_bytes * 100) / (total_sram_kb * 1024);

    /* --- 5. UI 格式化输出 --- */
    // SRAM : 显示总占用百分比 (已用KB / 总KB)
    // STACK : 显示栈的峰值百分比 (峰值B / 总分配K)
    lv_label_set_text_fmt(label, 
        "SRAM : %d%% (%dK/%dK)\n"
        "STACK: %d%% (%dB/%dK)", 
        (int)total_pct, (int)(total_used_bytes / 1024), (int)total_sram_kb,
        (int)peak_pct, (int)peak_used, (int)(stack_size / 1024)
    );
}

void custom_sys_monitor_init(void) {
    lv_obj_t * sys_label = lv_label_create(lv_layer_sys());
    
    // --- 样式：深色半透明背景 ---
    lv_obj_set_style_bg_color(sys_label, lv_palette_main(LV_PALETTE_NONE), 0);
    lv_obj_set_style_bg_opa(sys_label, LV_OPA_50, 0);
    lv_obj_set_style_text_color(sys_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(sys_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_all(sys_label, 2, 0);

    // --- 位置：右上角 ---
    lv_obj_align(sys_label, LV_ALIGN_TOP_RIGHT, 0, 0); 

    // 每 500ms 刷新一次
    lv_timer_create(sram_keil_timer_cb, 500, sys_label);
}

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// 1. ADC 原始数据缓冲区 (1024个点，每个点2字节)
uint16_t adc_buffer[SAMPLE_SIZE]; 

// 2. 采样完成标志位 (必须加 volatile，因为在中断中修改)
volatile uint8_t Flag = 0; 

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
// //栈区是 0x2001E000 到 0x20020000 (8KB)
// uint32_t *stack_ptr = (uint32_t *)0x2001E000;
// for(int i=0; i < (8192/4); i++) {
//     stack_ptr[i] = 0xAAAAAAAA; // 涂上“油漆”
// }
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_TIM4_Init();
  MX_TIM6_Init();
  MX_SPI2_Init();
  MX_TIM9_Init();
  MX_USART6_UART_Init();
  MX_I2C1_Init();
  MX_ADC3_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
   LCD_Init();
   /* 在 LCD_Init() 之后调用 */
   HAL_TIM_PWM_Start(&htim9, TIM_CHANNEL_1);
   LCD_Set_Brightness(g_sys_config.backlight_val); 
   effect_init();           //轨迹特效
   USART6_Config(9600);
 

  // 2. 开启 ADC 的 DMA 搬运 (一定要在 TIM 开启前)
  // 参数：ADC句柄, 缓冲区地址, 长度
  HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc_buffer, SAMPLE_SIZE);

  // 3. 开启定时器 3 的时钟触发
  HAL_TIM_Base_Start(&htim3);
 
   
   BSP_W25Q_Init();
   
   Key_Init(); 
   lv_init();                             // LVGL 初始化
   lv_port_disp_init();                   // 注册LVGL的显示任务
   lv_port_indev_init();                  // 注册LVGL的触屏检测任务
   lv_port_fs_init();                     // 虚拟文件系统
   HAL_TIM_Base_Start_IT(&htim6);
   
  // HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);//PWM验证
   
//   custom_ccm_monitor_init();
//   custom_sys_monitor_init();

   font_load ();
   UI_Init();
    printf("EVT_KEY_LONG   1\r\n");
	 

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	      // 检查硬件是否采满了 1024 个点
    if (Flag == 1) {
        // --- 核心计算环节 ---
        // 直接处理刚刚采好的 adc_buffer
        fft_module_execute(adc_buffer); 

        // --- 善后工作 ---
        Flag = 0; // 清除标志位
		
		  // 💡 架构师技巧：先 Stop 再 Start 是一种“复位”操作，
    // 虽然多写了几行，但它能解决 99% 的硬件死锁问题。
		HAL_ADC_Stop_DMA(&hadc3); 
		HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc_buffer, SAMPLE_SIZE);
        
        // 重新启动定时器，开始下一轮“快照”采集
        HAL_TIM_Base_Start(&htim3); 
    }
	  

	  Key_Scan(); 
	  Encoder_Scan();
	  FT6336U_Task();    
	
	  
	  
	   Event_t evt;                     //Event Pump（事件泵）模型       Run-to-completion event loop
        if (Event_Dequeue(&evt))
        {
            Menu_OnEvent(&evt);
        }
	  
	  HAL_Delay(1-1);
	  static uint8_t myLVGL=0;
	  if(myLVGL++>5){	  
	       lv_timer_handler() ;
		  myLVGL=0;
	  }
	 
	  
	
  }	  
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
int fputc(int ch, FILE *f)   {
    (void)f;
    uint8_t temp[1] = {ch};
 HAL_UART_Transmit(&huart1, temp, 1, 2);
 return ch;
}



void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
		lv_tick_inc(1);  //1ms
	
		
		static uint16_t i=0;
		if(i++>=500){
		 //printf("Tick\n");
		i=0;	
		}		
    }
}



 /* 引用我们在 lv_port_disp.c 里定义的全局指针 */
extern lv_disp_drv_t * disp_drv_ptr;


// 0: 总线空闲, 2: Flash 正在占用
//volatile uint8_t SPI1_Bus_Owner = 0; 

// DMA 传输完成回调（TX）
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI2 )
    {
		// 拉高片选，结束本次通信 
         LCD_CS_SET;
        //  核心：通知 LVGL 刚才那块数据发完了，你可以把下一块交给我了
        if(disp_drv_ptr != NULL)
        {
            lv_disp_flush_ready(disp_drv_ptr);
        }
    }
}

// DMA 传输完成回调（RX）
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
     if (hspi->Instance == SPI1) {
        BSP_W25Q_DMA_Callback_Handler(hspi);
		// SPI1_Bus_Owner = 0;        // 释放总线
    }                         
}









// HAL UART接收完成回调

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	    if (huart->Instance == USART6) {
        // 存储接收字节
        buff[buf_count] = rx_byte;
        buf_count++;
        if (buf_count >= 128) {
            buf_count = 0;
        }
	   // printf(" 0x%02X, %d\r\n", rx_byte, buf_count);
		
        // HAL 标准：重新启动接收
        HAL_UART_Receive_IT(&huart6, &rx_byte, 1);
    }
}


		  /**
 * @brief ADC 采样完成回调
 * @note 当 DMA 搬满 1024 个点时，硬件自动调用此函数
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC3) {
        // 1. 立即停止定时器！
        // 理由：我们要给 CPU 时间去算 FFT。如果不停止，DMA 就会在
        // CPU 计算期间继续往 adc_buffer 里写新数据，导致数据错乱（数据撕裂）。
		  // 1. 如果你选的是 Normal 模式，采集会自动停止
//        // 2. 如果你选的是 Circular 模式，它会不停。为了保证 FFT 计算时数据不被覆盖，
//        //    我们可以在这里暂时关闭定时器，拿个“快照”
        HAL_TIM_Base_Stop(&htim3);
        
        // 2. 发出“数据就绪”信号
        Flag = 1;         // 摇人！告诉主循环可以开始算 FFT 了
    }            
}






void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)               //触摸屏
{
    if (GPIO_Pin == FT_INT_PIN) {
        ft6336_irq_flag =true;
    }
}




/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
