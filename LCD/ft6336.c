#include "ft6336.h"


volatile FT6336U_State_t ft6336_state = {0};
volatile bool ft6336_irq_flag = false;

//inline告诉编译器“这个函数很小，调用它的时候，别真的跳过去执行，直接把函数代码‘贴’到调用的地方。” （只是建议，非强制）   宏等于文本替换，inline可调试，可检查，可维护的宏

static inline void FT6336U_ParseXY(const uint8_t *buf, volatile uint16_t *x,   volatile uint16_t *y)  //解包
{
    *x = ((uint16_t)(buf[0] & 0x0F) << 8) | buf[1];
    *y = ((uint16_t)(buf[2] & 0x0F) << 8) | buf[3];
}

static inline void FT6336U_Transform( volatile uint16_t *x,  volatile uint16_t *y)     //方向变换   
{
#if USE_HORIZONTAL == 0
	//原样，不做映射
#elif USE_HORIZONTAL == 1
    *x = LCD_W - *x;
    *y = LCD_H - *y;
#elif USE_HORIZONTAL == 2
    uint16_t t = *x;
    *x = *y;
    *y = LCD_H - t;
#elif USE_HORIZONTAL == 3
    uint16_t t = *y;
    *y = *x;
    *x = LCD_W - t;
#endif
}

/* I2C 读寄存器 */
static uint8_t FT6336U_Read(uint8_t reg, uint8_t *buf, uint8_t len)
{
    if (HAL_I2C_Master_Transmit(&hi2c1, FT6336U_I2C_ADDR, &reg, 1, 10) != HAL_OK)
        return 1;
    if (HAL_I2C_Master_Receive(&hi2c1, FT6336U_I2C_ADDR, buf, len, 10) != HAL_OK)
        return 1;
    return 0;
}

void FT6336U_Init(void)
{
    HAL_GPIO_WritePin(FT_RST_PORT, FT_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(FT_RST_PORT, FT_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(300);
}

/* 主循环调用 */
void FT6336U_Task(void)
{
    uint8_t points;
    uint8_t buf[4];

    //    if (!ft6336_irq_flag)   return;         //被中断触发的轮询任务
    //	 ft6336_irq_flag = false;


    if (FT6336U_Read(FT_REG_TD_STATUS, &points, 1))
        return;

    points &= 0x0F;
    ft6336_state.point_num = points;

    if (points == 0) {
        ft6336_state.pressed = false;
        return;
    }

    /* Point 1 */
    if (FT6336U_Read(FT_REG_P1_XH, buf, 4))
        return;

	 FT6336U_ParseXY(buf, &ft6336_state.x[0], &ft6336_state.y[0]);
     FT6336U_Transform(&ft6336_state.x[0], &ft6336_state.y[0]);


//    /* Point2 */
//    if (points >= 2) {
//        uint8_t buf2[4];
//	 FT6336U_ParseXY(buf2, &ft6336_state.x[1], &ft6336_state.y[1]);
//    FT6336U_Transform(&ft6336_state.x[1], &ft6336_state.y[1]);
//    }


    ft6336_state.pressed = true;
}

