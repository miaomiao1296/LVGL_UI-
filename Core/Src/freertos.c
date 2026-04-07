/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FFT.h"
#include "Key.h"
#include "encoder.h"
#include "ft6336.h"
#include "event.h"
#include "menu_system.h"
#include "lvgl.h"
#include "adc.h"
#include "tim.h"

extern ADC_HandleTypeDef hadc3;
extern TIM_HandleTypeDef htim3;
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osSemaphoreId_t adcReadySemHandle;
const osSemaphoreAttr_t adcReadySem_attributes = {
  .name = "adcReadySem"
};
extern uint16_t adc_buffer[1024];

// FFT
osThreadId_t fftTaskHandle;
const osThreadAttr_t fftTask_attributes = {
  .name = "fftTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

//Input
osThreadId_t inputTaskHandle;
const osThreadAttr_t inputTask_attributes = {
  .name = "inputTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

// 任务堆栈监控变量
uint32_t fftTaskStackFree = 0;        //任务专门的栈监测
uint32_t defaultTaskStackFree = 0;
uint32_t inputTaskStackFree = 0;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartFftTask(void *argument);
void StartInputTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
   (void)xTask;
   (void)pcTaskName;

   // 挂起中断并进入死循环，方便在调试器里查看Call Stack
   taskDISABLE_INTERRUPTS();
   while (1) {
       // Stack overflow detected!
       // Check xTask and pcTaskName in debugger variables
   }
}

void vApplicationMallocFailedHook(void)
{
    /* vApplicationMallocFailedHook() will only be called if
    configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
    function that will get called if a call to pvPortMalloc() fails. */

    // 挂起中断并进入死循环，方便在调试器里排查
    taskDISABLE_INTERRUPTS();
    while (1) {
        // Malloc failed! Not enough FreeRTOS heap.
    }
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
// 这里保持为空，或者由 CubeMX 自动更新。实际实现已经放在 USER CODE 4 中并由 USER CODE 5 的注释块管理实现
/* USER CODE END 5 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  adcReadySemHandle = osSemaphoreNew(1, 0, &adcReadySem_attributes);
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  fftTaskHandle = osThreadNew(StartFftTask, NULL, &fftTask_attributes);
  inputTaskHandle = osThreadNew(StartInputTask, NULL, &inputTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  (void)argument; // 消除未使用的参数警告
  /* Infinite loop */
  for(;;)
  {
    Event_t evt;
    if (Event_Dequeue(&evt))
    {
        // 菜单事件处理放在 GUI 任务中，避免多线程调用 LVGL 函数导致崩溃
        Menu_OnEvent(&evt);
    }

    // ==========================================
    // 监控自身堆栈
    defaultTaskStackFree = uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);

    osDelay(1);

    static uint8_t myLVGL = 0;
    if(myLVGL++ > 5){
        lv_timer_handler();
        myLVGL = 0;
    }
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/**
* @brief Function implementing the InputTask thread.
* @param argument: Not used
* @retval None
*/
void StartInputTask(void *argument)
{
  (void)argument;
  for(;;)
  {
    Key_Scan();
    Encoder_Scan();
    FT6336U_Task(); // 触摸屏扫描

    // 监控自身堆栈
    inputTaskStackFree = uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);

    // 延时 10ms，保证 100Hz 的扫描频率，按键和编码器绝对不会丢步
    osDelay(10);
  }
}

/**
* @brief Function implementing the FftTask thread.
* @param argument: Not used
* @retval None
*/
void StartFftTask(void *argument)
{
  (void)argument;
  // 确保 FFT 内存已经分配好
  fft_module_init();

  // 1. 首次启动 ADC 与 Timer 去采第一帧数据
  HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc_buffer, SAMPLE_SIZE);
  HAL_TIM_Base_Start(&htim3);

  for(;;)
  {
    // 无期限等待 ADC 采集完成信号量
    if(osSemaphoreAcquire(adcReadySemHandle, osWaitForever) == osOK)
    {
        // 在算FFT之前，就把硬件关干净，防挂死！
        HAL_ADC_Stop_DMA(&hadc3);
        HAL_TIM_Base_Stop(&htim3);

        // FFT 计算
        fft_module_execute(adc_buffer);

        // 清除可能存在的 Overrun 错误状态，否则会导致 ADC_Start_DMA 失败 (HAL_BUSY)
        __HAL_ADC_CLEAR_FLAG(&hadc3, ADC_FLAG_OVR);

        // 暴力复位状态机（兼容不同的HAL版本）
        hadc3.State = HAL_ADC_STATE_READY;
        hadc3.ErrorCode = HAL_ADC_ERROR_NONE;

        if (hadc3.DMA_Handle) {
            hadc3.DMA_Handle->State = HAL_DMA_STATE_READY;
        }

        if (HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc_buffer, SAMPLE_SIZE) != HAL_OK) {
            // 如果启动失败，尝试硬件复位
            HAL_ADC_DeInit(&hadc3);
            MX_ADC3_Init();
            HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc_buffer, SAMPLE_SIZE);
        }

        // 重新清零 TIM 计数器保证等间隔触发
        __HAL_TIM_SET_COUNTER(&htim3, 0);
        HAL_TIM_Base_Start(&htim3);

        // 监控自身堆栈：uxTaskGetStackHighWaterMark 返回的是字(word)数，STM32上1字=4字节
        fftTaskStackFree = uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);
    }
  }
}
/* USER CODE END Application */

