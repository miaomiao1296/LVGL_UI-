/**
 * @file lv_port_indev_templ.c
 *
 */

/*Copy this file as "lv_port_indev.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "lvgl.h"
#include "encoder.h"
#include "ft6336.h"
#include "stdio.h"
#include "effect.h"  

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static void encoder_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

lv_group_t * main_group = NULL;              //����������


/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t * indev_touchpad;
lv_indev_t * indev_encoder;


void lv_port_indev_init(void)
{
   // ��������Ҫ�����붨�����������ľ�̬���������ܹ���һ��������
    static lv_indev_drv_t encoder_drv_instance;
	static lv_indev_drv_t indev_drv_touch;

    /*------------------
     * Touchpad
     * -----------------*/

    FT6336U_Init() ;

	lv_indev_drv_init(&indev_drv_touch);
    indev_drv_touch.type    = LV_INDEV_TYPE_POINTER;
    indev_drv_touch.read_cb = touchpad_read;
    
    
    indev_touchpad = lv_indev_drv_register(&indev_drv_touch);

    /*------------------
     * Encoder
     * -----------------*/
	 
	Encoder_Init();
 
    lv_indev_drv_init(&encoder_drv_instance);
    encoder_drv_instance.type = LV_INDEV_TYPE_ENCODER;
    encoder_drv_instance.read_cb = encoder_read;
    indev_encoder = lv_indev_drv_register(&encoder_drv_instance);


 /* --- �ؼ���������������(Group) --- */
    // ������������� Group ���ܿ��ư�ť���б���
    main_group= lv_group_create();
 //   lv_group_set_default(main_group);                 //ɾ�����У���Ϊ�ֶ�����������ȫ    ���κ��´����ġ����н������ԵĶ��󣨱��簴ť���б������飩�������ڴ�����һ˲���Զ����������Ĭ���顣
    lv_indev_set_group(indev_encoder, main_group);
    
}

///*------------------
// * Touchpad
// * -----------------*/

/* --- 1. 定义全局触摸开关 --- */
static bool touchpad_enabled = true;

/**
 * @brief 触摸屏开关切换 (供物理按键调用)
 * 每次调用，触摸状态都会反转：开 -> 关 -> 开
 */
void lv_touchpad_toggle(void) {
    touchpad_enabled = !touchpad_enabled;
    
    // 调试打印：串口看到当前状态
    if(touchpad_enabled) printf("[System] Touchpad UNLOCKED\n");
    else printf("[System] Touchpad LOCKED\n");
}



/* LVGL ��ȡ�ص����� */             //  ��ֻ����һ���£����� LVGL�������Ƿ񡰰��� / �ɿ�����ǰ�����Ƕ���
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
    (void)indev_drv;
	     // 物理锁定判定      逻辑拦截
    if (!touchpad_enabled) {
        data->state = LV_INDEV_STATE_REL; // 强制设为“松开”
        return; 
    }
	
	
	 if (ft6336_state.pressed) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = ft6336_state.x[0];          // �� ft6336_state �ṹ���ȡ����  LVGL �����к��Ľ��������ڵ���
        data->point.y = ft6336_state.y[0];     
	 //   printf("X: %d, Y: %d\r\n",  ft6336_state.x[0], ft6336_state.y[0]);            //��ǰ�����������		 
        effect_on_move(data->point.x, data->point.y);    // �켣��Ч	
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

/*------------------
 * Encoder
 * -----------------*/

/*Will be called by the library to read the encoder*/
static void encoder_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    (void)indev_drv;
    data->enc_diff = Encoder_Get_Diff();
    data->state = Encoder_Get_State();
}



#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
