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
#include "cmsis_os.h"
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

/* --- 动态字库内存池声明 --- */
// FFT 已经静态分配掉了 16KB，CCMRAM 共有 64KB。
#define FONT_POOL_SIZE  (40 * 1024)

// 让编译器把这个 46KB 数组安全地安顿在 CCM 里，它会自动防碰撞！
__attribute__((section(".ccmram"), aligned(4)))
static uint8_t font_pool[FONT_POOL_SIZE];

static uint32_t font_alloc_offset = 0; // 当前已分配的偏移量

/**
 * @brief  字库内存分配器 (线性增长)
 * @param  size: 申请的大小 (字节)
 * @return 成功返回内存指针，失败返回 NULL
 */
void * lv_font_ccm_alloc(size_t size) {
    // 1. 4字节对齐：将 size 向上取整到 4 的倍数 (ARM 架构规范)
    size = (size + 3) & ~3;

    // 2. 边界检查：超标直接拦截报错
    if (font_alloc_offset + size > FONT_POOL_SIZE) {
        printf("[CCM Alert] Font allocation overflow!\n");
        return NULL;
    }

    // 3. 给出地址，移动偏移量
    void * p = &font_pool[font_alloc_offset];
    font_alloc_offset += size;

    return p;
}

/**
 * @brief  逻辑清空字库区
 */
void lv_font_ccm_reset(void) {
    font_alloc_offset = 0; // 一秒还原，懒清空
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

extern osSemaphoreId_t adcReadySemHandle;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
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
    // 1. 同步链接脚本修改：主栈大小设为 2KB (0x0800)
    // 2. 栈顶在 0x20020000。
    // 3. 向下偏移 64 字节再开始刷，给当前 main 函数的运行留出栈空间，防止自己把自己刷死！
    uint32_t *stack_ptr = (uint32_t *)0x2001F800;
    for(int i=0; i < ((2048 - 64) / 4); i++) {
        stack_ptr[i] = 0xAAAAAAAA;
    }

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

   BSP_W25Q_Init();
   
   Key_Init(); 
   lv_init();                             // LVGL 初始化
   lv_port_disp_init();                   // 注册LVGL的显示任务
   lv_port_indev_init();                  // 注册LVGL的触屏检测任务
   lv_port_fs_init();                     // 虚拟文件系统
   HAL_TIM_Base_Start_IT(&htim6);


   font_load ();
   UI_Init();
	 

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
 * @note 当 DMA 搬满 1024 个 point 时，硬件自动调用此函数
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC3) {
        // 1. 立即停止定时器！
        HAL_TIM_Base_Stop(&htim3);
        
        // 2. 发出“数据就绪”信号
        if (adcReadySemHandle != NULL)
        {
            // CMSIS-RTOS2 的 osSemaphoreRelease 内部已经处理了 ISR 检查
            osSemaphoreRelease(adcReadySemHandle);
        }
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
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM7 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */
  if (htim->Instance == TIM6) {
      lv_tick_inc(1);  //1ms LVGL心跳
  }
  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM7)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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
