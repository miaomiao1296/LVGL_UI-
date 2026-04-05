#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>
#include <stdbool.h>

/* 事件类型：根据你系统扩展，可继续添加 */
typedef enum {
    EVT_NONE = 0,
	EVT_KEY_PRESS,
    EVT_KEY_SHORT,
    EVT_KEY_LONG,
	EVT_KEY_REPEAT,
} EventType_t;

///事件结构：可携带参数或指针 
typedef struct {
    EventType_t type;
    uint32_t    param; /* 通用参数（比如按键ID，时间，value delta 等） */
    void*       ptr;   /* 可选，指向静态数据或上下文（ISR 入队时必须保证有效） */
} Event_t;

void Event_Init(void);

/* ISR-safe 入队（在中断中调用） - 返回 true 表示成功，false 表示队列已满 */
bool Event_EnqueueFromISR(const Event_t *e);

/* 主循环入队（非 ISR） */
bool Event_Enqueue(const Event_t *e);

/* 主循环出队，返回 true 表示取得事件，false 表示队列空 */
bool Event_Dequeue(Event_t *out);

/* 查询当前队列长度（用于监控/调试） */
uint16_t Event_GetCount(void);

#endif 
