#include "event.h"
#include "stm32f4xx.h" /* for __disable_irq/__enable_irq and __DSB */
#include <string.h>


/*
 * 实现说明（关键点）：
 * - 使用固定大小环形缓冲（EVT_QUEUE_SIZE 必须为 2 的幂）以便使用位掩码快速取模。
 * - ISR 入队不禁中断（尽量短），主循环入队使用禁中断保护以避免与 ISR 竞争写尾指针。
 * - 使用 __DSB() 在写入后确保数据对其他核/主循环可见（对 Cortex-M 是良好习惯）。
 */

#define EVT_QUEUE_SIZE 128  /* 必须为 2 的幂：64/128/256 */
#if (EVT_QUEUE_SIZE & (EVT_QUEUE_SIZE - 1)) != 0
#error "EVT_QUEUE_SIZE must be power of two"                  //否则报错
#endif

/* 环形队列存储 */
static Event_t evt_queue[EVT_QUEUE_SIZE];
static volatile uint16_t evt_head = 0; /* 取出索引 */
static volatile uint16_t evt_tail = 0; /* 写入索引 */

/* 快速 next 计算（使用掩码代替模运算） */
static inline uint16_t evt_next(uint16_t v) {
    return (v + 1) & (EVT_QUEUE_SIZE - 1);
}

void Event_Init(void) {
    evt_head = evt_tail = 0;         /* 清零索引，清队列结构（可选） */
    /* 如果需要把队列内容清空，可用 memset(evt_queue,0,sizeof(evt_queue)); */
	//memset(evt_queue,0,sizeof(evt_queue));
}

/* ISR-safe 入队：不关中断，尽量短 */
bool Event_EnqueueFromISR(const Event_t *e) {
    uint16_t next_tail = evt_next(evt_tail);
    if (next_tail == evt_head) {
        /* 队列满：不能阻塞或等待，直接返回失败 */
        return false;
    }
    evt_queue[evt_tail] = *e;
    __DSB(); /* 确保写入完成可见 */
    evt_tail = next_tail; /* 单字写入（保证原子） */
    return true;
}

/* 主循环入队：为安全起见禁中断并更新尾指针 */
bool Event_Enqueue(const Event_t *e) {
    __disable_irq();
    uint16_t next_tail = evt_next(evt_tail);
    if (next_tail == evt_head) {
        __enable_irq();
        return false; /* 队列满 */
    }
    evt_queue[evt_tail] = *e;
    evt_tail = next_tail;
    __enable_irq();
    return true;
}

//出队：主循环调用，无需更复杂同步（我们在头尾读操作时对称保护） 
bool Event_Dequeue(Event_t *out) {
    if (evt_head == evt_tail) return false; /* 空 */
    __disable_irq(); /* 防止 ISR 在我们复制数据时修改 tail（微小临界区） */
    if (evt_head == evt_tail) { __enable_irq(); return false; }
    *out = evt_queue[evt_head];
    evt_head = evt_next(evt_head);
    __enable_irq();
    return true;
}

//返回当前事件队列里有多少个事件
uint16_t Event_GetCount(void) {
    uint16_t h = evt_head, t = evt_tail;
    if (t >= h) return t - h;
    return EVT_QUEUE_SIZE - (h-t);
}
