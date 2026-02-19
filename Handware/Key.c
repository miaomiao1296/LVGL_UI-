#include "Key.h"
#include "event.h" 
#include "main.h"

/*=============================
  1. 用户可配置参数区
==============================*/

// 工业级时间参数（单位：ms）
#define SCAN_INTERVAL       10      // 扫描周期 (10ms)
#define DEBOUNCE_COUNT      2       // 20ms消抖（2 * 10ms）
#define HOLD_TIME           1000    // 1秒长按
#define REPEAT_INTERVAL     250     // 连发间隔 250ms

// 按键硬件映射表
static const KeyConfig key_config[KEY_COUNT] = {
    {GPIOE, GPIO_PIN_7},   // KEY0
    {GPIOE, GPIO_PIN_8},   // KEY1
    {GPIOE, GPIO_PIN_9},   // KEY2
    {GPIOE, GPIO_PIN_10},  // KEY3
};

/*=============================
  2. 内部状态存储结构
==============================*/

typedef struct {
    KeyState state;         // 当前状态
    KeyState prev_state;    // 前一个状态
    uint8_t debounce_count; // 消抖计数
    uint32_t press_time;    // 按下时刻
    uint32_t repeat_timer;  // 连发定时
} KeyData;

static KeyData key_data[KEY_COUNT];

/*=============================
  3. 内部函数 - 事件入队        可屏蔽某些 Key，不投递 Event
==============================*/
static inline void Key_PostEvent(uint8_t id, KeyEvent event)
{
    Event_t e;
    e.ptr = NULL;
    e.param = id;
	
    /* 事件映射：把 KeyEvent 映射为通用 EventType */
    switch (event) {
        case KEY_EVENT_PRESS:                          
            e.type = EVT_KEY_PRESS;       
            break;                  
        case KEY_EVENT_RELEASE:
            e.type = EVT_KEY_SHORT;     
            break;
        case KEY_EVENT_HOLD:
            e.type = EVT_KEY_LONG; 
            break;
        case KEY_EVENT_REPEAT:
            e.type = EVT_KEY_REPEAT; 
            break;
		 default:
            return;                  //return 跳出整个函数
    }                       
    Event_Enqueue(&e);   
}

/*=============================
  4. 公共接口实现
==============================*/
void Key_Init(void)
{
    for (int i = 0; i < KEY_COUNT; i++) {
        key_data[i].state = KEY_IDLE;
        key_data[i].prev_state = KEY_IDLE;
        key_data[i].debounce_count = 0;
        key_data[i].press_time = 0;
        key_data[i].repeat_timer = 0;
    }
}

/**
 * @brief 非阻塞扫描函数（10ms周期调用）
 */
void Key_Scan(void)
{
    static uint32_t last_scan_time = 0;
    uint32_t current_time = HAL_GetTick();
    if (current_time - last_scan_time < SCAN_INTERVAL) return;
    last_scan_time = current_time;

    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        GPIO_PinState pin_state = HAL_GPIO_ReadPin(key_config[i].port, key_config[i].pin);
		KeyData* data = &key_data[i];
		
        switch (data->state) {
        case KEY_IDLE:
            if (pin_state == GPIO_PIN_RESET) {
                 data->state = KEY_DEBOUNCE;                                        
                 data->debounce_count = 0;  
            }
            break;

			
		case KEY_DEBOUNCE:
                if (++data->debounce_count >= DEBOUNCE_COUNT) {
                    if (pin_state == GPIO_PIN_RESET) {
                        data->state = KEY_PRESSED;
                        data->press_time = current_time;
						Key_PostEvent(i, KEY_EVENT_PRESS);         
                    } else {
                        data->state = KEY_IDLE;
                    }
                }
                break;	
				
				
		case KEY_PRESSED:
                if (pin_state == GPIO_PIN_SET) {
                    data->state = KEY_RELEASE_DEBOUNCE;
                    data->debounce_count = 0;
                    data->prev_state = KEY_PRESSED; 
                } else if (current_time - data->press_time >= HOLD_TIME) {
                    data->state = KEY_REPEAT;
                    data->repeat_timer = current_time;
                    data->prev_state = KEY_PRESSED; 
					Key_PostEvent(i, KEY_EVENT_HOLD);
                }
                break;		

	    case KEY_REPEAT:
                if (pin_state == GPIO_PIN_SET) {
                    data->state = KEY_RELEASE_DEBOUNCE;
                    data->debounce_count = 0;
                    data->prev_state = KEY_REPEAT;  
                } else if (current_time - data->repeat_timer >= REPEAT_INTERVAL) {    
                    data->repeat_timer = current_time;  
					Key_PostEvent(i, KEY_EVENT_REPEAT);
                }
                break;
                
				
				
        case KEY_RELEASE_DEBOUNCE:
                if (++data->debounce_count >= DEBOUNCE_COUNT) {
                    if (pin_state == GPIO_PIN_SET) {
                        if (data->prev_state == KEY_PRESSED) {
                            Key_PostEvent(i, KEY_EVENT_RELEASE);
                        }
                        data->state = KEY_IDLE;
                    } else {
                        data->state = data->prev_state; 
                    }
                }
                break;


        default:
            data->state = KEY_IDLE;
            break;
        }
    }
}
