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
/* USER CODE BEGIN Variables (force sync) */
extern uint16_t adc_buffer[SAMPLE_SIZE];
// 不再使用 Flag，改为二值信号量
// extern volatile uint8_t Flag;
osSemaphoreId_t adcReadySemHandle;
const osSemaphoreAttr_t adcReadySem_attributes = {
  .name = "adcReadySem"
};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Definitions for FftTask */
osThreadId_t FftTaskHandle;
const osThreadAttr_t FftTask_attributes = {
  .name = "FftTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartFftTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
   (void)xTask;
   (void)pcTaskName;
}
/* USER CODE END 4 */

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

  /* creation of FftTask */
  FftTaskHandle = osThreadNew(StartFftTask, NULL, &FftTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
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
    Key_Scan();
    Encoder_Scan();
    FT6336U_Task();

    Event_t evt;
    if (Event_Dequeue(&evt))
    {
        Menu_OnEvent(&evt);
    }

    osDelay(1);

    static uint8_t myLVGL = 0;
    if(myLVGL++ > 5){
        lv_timer_handler();
        myLVGL = 0;
    }
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Application */
/**
* @brief Function implementing the FftTask thread.
* @param argument: Not used
* @retval None
*/
void StartFftTask(void *argument)
{
  (void)argument;
  for(;;)
  {
    // 无期限等待 ADC 采集完成信号量
    if(osSemaphoreAcquire(adcReadySemHandle, osWaitForever) == osOK)
    {
        // FFT 计算
        fft_module_execute(adc_buffer);

        // 重新启动 ADC 与 Timer 去采下一帧数据
        HAL_ADC_Stop_DMA(&hadc3);
        HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc_buffer, SAMPLE_SIZE);
        HAL_TIM_Base_Start(&htim3);
    }
  }
}
/* USER CODE END Application */

