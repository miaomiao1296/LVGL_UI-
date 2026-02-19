#include "arm_math.h"
#include "Key.h"
#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include "lcd.h"
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
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "FFT.h"


extern void lv_font_ccm_reset(void);    //字库重置
extern void * other_ccm_alloc(size_t size) ;  //CCM
extern void lv_touchpad_toggle(void);   //触摸开关

void custom_ccm_monitor_init(void);
void custom_sys_monitor_init(void);


extern lv_group_t * main_group;       //编码器的组
//示例：lv_group_add_obj(main_group, 控件名称); // 手动入编码器组     //添加顺序 = 旋转顺序： 调用 lv_group_add_obj 的先后顺序，直接决定了用户顺时针旋转编码器时，光标是在按钮间怎么跳动的。


System_Config_t g_sys_config = { .backlight_val = 500 };           //初始化背光亮度值

/////////////////////////////////////////////////字库开始//////////////////////////////////////////////////////////////////////////
lv_font_t * my_font24= NULL;; // 用于存放加载后的字库指针
lv_font_t * my_font16= NULL;; // 用于存放加载后的字库指针
static lv_style_t style_24_YELLOW;    
static lv_style_t style_24_BLUE;

static lv_style_t style_16_Black;
static lv_style_t style_16_BLUE;
static lv_style_t style_16_White;

void font_load (void){

	my_font24 = lv_font_load("S:font_cn_24.bin");           // 执行加载
	my_font16 = lv_font_load("S:font_cn_16.bin");           // 执行加载
//	if(my_font16 == NULL) {
//    // 加载失败，强制使用内置字体，防止系统崩溃
//    my_font16 = &lv_font_montserrat_14; 
//}

	lv_style_init(&style_24_YELLOW);
	lv_style_set_text_font(&style_24_YELLOW, my_font24);
	lv_style_set_text_color(&style_24_YELLOW, lv_palette_main(LV_PALETTE_YELLOW));


	lv_style_init(&style_24_BLUE);
	lv_style_set_text_font(&style_24_BLUE, my_font24);
	lv_style_set_text_color(&style_24_BLUE, lv_palette_main(LV_PALETTE_BLUE));
	
	
	
	lv_style_init(&style_16_Black);
	lv_style_set_text_font(&style_16_Black, my_font16);
	lv_style_set_text_color(&style_16_Black, lv_color_black());    //十六进制（注意是 24 位格式）
	
		
	lv_style_init(&style_16_White);
	lv_style_set_text_font(&style_16_White, my_font16);
	lv_style_set_text_color(&style_16_White, lv_color_white());
	
	
	
	lv_style_init(&style_16_BLUE);
	lv_style_set_text_font(&style_16_BLUE, my_font16);
	lv_style_set_text_color(&style_16_BLUE, lv_palette_main(LV_PALETTE_BLUE));
   }

 /////////////////////////////////////////////////字库结束//////////////////////////////////////////////////////////////////////////  

   
   
   
   
/**
 * 架构精要：
 * 1. 路由表(Route Table)：将页面ID与执行函数解耦，增加页面只需在表中注册。//解耦：把每个页面的 ID、进入函数、退出函数、动画类型 像填表一样写在数组里。  ：主引擎 ui_switch 根本不需要知道“频谱页”长什么样，它只需要去查表：“哦，要去 UI_PAGE_3 啊，表里说了，用滑动动画，先执行 exit，再执行 enter
 * 2. 页面栈(Page Stack)：记录访问历史，实现类似手机的“返回”功能。
 * 3. 差异化动效：针对不同资源压力，提供滑动、淡入、瞬切三种模式。
 */

 //内部配置（物理约束与性能参数）
#define LCD_W              320   // 屏幕宽度
#define LCD_H              240   // 屏幕高度
#define PAGE_STACK_DEPTH   8     // 页面栈深度：决定了你最多能“回退”多少层
#define ANIM_TIME_MS       300   // 全局动画时长：300ms 是人眼感知的黄金平衡点



////////////////////////////////////////// 页面生命周期函数声明开始//////////////////////////////////////// ////////
/*  
每个页面必须具备 enter(进入并创建) 和 exit(退出并清理) 两个钩子函数
exit()（临终遗言）：在离开旧页面前，执行清理工作（比如关掉不用的定时器、清理内存）。
lv_group_remove_all_objs（清理组）：在这里自动清理了编码器控制的对象，防止“人走了，魂还在”（指针悬空）。
enter()（初生仪式）：在新页面被创建时，初始化它的所有按钮、图表和样式。
*/

static void page_home_enter(void);
static void page_home_exit_logic(void);

static void page_image_enter(void);
static void page_image_exit_logic(void);

static void page_setting_enter(void);
static void page_setting_exit_logic(void);

static void page_1_enter(void);
static void page_1_exit_logic(void);

static void page_2_enter(void);
static void page_2_exit_logic(void);

static void page_3_enter(void);
static void page_3_exit_logic(void);

static void page_4_enter(void);
static void page_4_exit_logic(void);

static void page_5_enter(void);
static void page_5_exit_logic(void);
////////////////////////////////////////////// 页面生命周期函数声明结束//////////////////////////////////////// ////////




///////////////////////////////////////// 类型定义与路由配置开始////////////////////////////////////////////////// 
// 页面动画模式
typedef enum {
    UI_ANIM_SLIDE = 0,     // 左右滑动 强调页面层级（适合主功能切换）
    UI_ANIM_FADE,          // 淡入淡出 强调状态重叠（适合菜单或模拟弹窗）
    UI_ANIM_INSTANT        // 暴力瞬切 (性能模式)
} UIAnim_t;

// 页面路由结构体
typedef struct {
    void (*enter)(void);      // 构建函数：创建 UI 组件
    void (*exit_logic)(void); // 逻辑关停：停止采样、释放大块 Buffer、关中断
    UIAnim_t anim_type;       // 指定该页面进入时的动画类型
} PageRoute_t;

// 路由表：这是整个 UI 的“地图”，修改此处即可改变全局交互行为
static const PageRoute_t page_table[UI_PAGE_COUNT] = {          //const存放在单片机的 Flash 中，而不是 RAM）。用 const 指针接收，保证了程序不会在运行过程中意外修改路由表，确保了系统的稳定性
    [UI_PAGE_NONE]    = { NULL,               NULL,                    UI_ANIM_INSTANT },
    [UI_PAGE_HOME]    = { page_home_enter,    page_home_exit_logic,    UI_ANIM_SLIDE },
    [UI_PAGE_IMAGE]   = { page_image_enter,   page_image_exit_logic,   UI_ANIM_INSTANT },
    [UI_PAGE_SETTING] = { page_setting_enter, page_setting_exit_logic, UI_ANIM_FADE },
    [UI_PAGE_1]       = { page_1_enter,       page_1_exit_logic,       UI_ANIM_SLIDE },
    [UI_PAGE_2]       = { page_2_enter,       page_2_exit_logic,       UI_ANIM_INSTANT },
    [UI_PAGE_3]       = { page_3_enter,       page_3_exit_logic,       UI_ANIM_SLIDE },
    [UI_PAGE_4]       = { page_4_enter,       page_4_exit_logic,       UI_ANIM_SLIDE },
    [UI_PAGE_5]       = { page_5_enter,       page_5_exit_logic,       UI_ANIM_SLIDE },
};
///////////////////////////////////////// 类型定义与路由配置结束////////////////////////////////////////////////// 



////////////////////////////////////// 内部状态变量与上下文开始//////////////////////////////////////////////////////////
static UIPage_t page_stack[PAGE_STACK_DEPTH]; // 静态数组实现的栈     (历史记录) LIFO（后进先出）的栈
static int8_t   stack_top = -1;                // 栈顶指针
static UIPage_t cur_page = UI_PAGE_NONE;      // 记录当前活跃页面 ID (uint8_t 安全性)

static lv_obj_t *page_root = NULL;            // 当前页面的根容器，所有组件必须挂载其上   //创建了一个透明的、跟屏幕一样大的 root 容器。这个页面所有的按钮、图表都挂在它上面。删掉这个 root，整个页面就瞬间消失，干净利落。
static bool  ui_animating = false;          // 全局动画锁：防止动画未结束时用户快速点击导致崩溃        //否则  系统可能会同时跑两个转场，导致内存溢出或指针跑飞。

// 动画上下文单例：利用锁的唯一性，彻底消灭动态内存申请 (No malloc)
static struct {
    lv_obj_t * old_root;
} g_anim_ctx;


////////////////////////////////////// 内部状态与上下文结束////////////////////////////////////////////////////////

 ////////////////////////////////////////////////内部工具函数开始///////////////////////////////////////////////////// 


/** 统一动画终点回调 */
static void ui_anim_finished_cb(lv_anim_t *a) {
    // 动画结束，物理销毁旧页面容器
    if (g_anim_ctx.old_root && lv_obj_is_valid(g_anim_ctx.old_root)) {
        lv_obj_del(g_anim_ctx.old_root);
        g_anim_ctx.old_root = NULL;
    }
    ui_animating = false;     //动画彻底完成后才释放 ui_animating 锁      //动画开始设为 true，return 掉所有新请求；动画结束通过回调设为 false
}

/** 辅助：创建根容器并执行进入逻辑 ，为每个页面提供一个独立的“底座”，方便统一平移或销毁 */
static void prepare_new_page(const PageRoute_t * route, lv_coord_t x) {
    page_root = lv_obj_create(lv_scr_act());    // 在当前激活屏幕上创建
    lv_obj_remove_style_all(page_root);     // 彻底移除默认样式（去除边框、背景、填充）   //默认容器有边框和填充，会消耗渲染性能。把它“剥光”了，只留下最纯粹的像素承载力
    lv_obj_set_size(page_root,LCD_W, LCD_H);     //跟屏幕一样大的 root 容器
    lv_obj_set_pos(page_root, x, 0);
    lv_obj_clear_flag(page_root, LV_OBJ_FLAG_SCROLLABLE);   // 根容器通常不滚动，内部组件才滚动
    
    // 执行页面的 UI 构建钩子
    if (route->enter) route->enter();
}

/** 透明度回调包装 */
static void anim_opa_cb(void * var, int32_t v) {    //LVGL 动画无法直接操作 Style，需通过此包装函数
    lv_obj_set_style_opa((lv_obj_t *)var, v, 0);
}


////////////////////////////////////////////////内部工具函数结束///////////////////////////////////////////////////// 


////////////////////////////////////////////////核心切换引擎开始/////////////////////////////////////////////////////// 
static void ui_switch(UIPage_t target, bool is_back) {
	
	// 越界检查与锁检查
    if (target >= UI_PAGE_COUNT || target == UI_PAGE_NONE) return;
    if (ui_animating) return;
	  //查路由表
    const PageRoute_t *new_route = &page_table[target];
    const PageRoute_t *old_route = (cur_page != UI_PAGE_NONE) ? &page_table[cur_page] : NULL;

    //  首次启动处理       （无动画直接显示，不加锁）   
    if (cur_page == UI_PAGE_NONE) {
        prepare_new_page(new_route, 0);
        cur_page = target;
        return;
    }

    //  锁定与逻辑撤离 (必须在创建新页面前执行，以释放计算资源)
	
    ui_animating = true;         // 只要进入切换流程，立即开启全局锁
    if (old_route && old_route->exit_logic) old_route->exit_logic();    //开始切换逻辑：触发旧页面的 exit 生命周期钩子
    if (main_group) lv_group_remove_all_objs(main_group);     // 强制清理编码器组，防止交互冲突
    g_anim_ctx.old_root = page_root; //暂存旧根容器指针用于后续销毁

	
    /*==================================================
     * 模式 1：INSTANT (暴力瞬切) 
     *==================================================*/
	 if (new_route->anim_type == UI_ANIM_INSTANT) {
        if (g_anim_ctx.old_root) lv_obj_del(g_anim_ctx.old_root);  // 合法性检查    立刻删除旧页面 ，腾出内存
        prepare_new_page(new_route, 0);
        cur_page = target;
        ui_animating = false;        // 瞬切无需动画，立即解锁
        return;
    }


    /*==================================================
     * 模式 2：FADE (覆盖式淡入) 
     *==================================================*/
	 if (new_route->anim_type == UI_ANIM_FADE) {
        prepare_new_page(new_route, 0);                 //先执行了 new_route->enter()，此时旧页面的内存还没释放。在转场动画的 300ms 内，内存里同时存在两个页面的所有对象
        lv_obj_set_style_opa(page_root, LV_OPA_TRANSP, 0);    // 初始全透明
        cur_page = target;

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, page_root);
        lv_anim_set_exec_cb(&a, anim_opa_cb);
        lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_time(&a, ANIM_TIME_MS);
        lv_anim_set_ready_cb(&a, ui_anim_finished_cb);
        lv_anim_start(&a);
    }
    /*==================================================
     * 模式 3：SLIDE (左右滑动)
     *==================================================*/
	else {
	    // 根据是“前进”还是“回退”计算滑动的坐标方向
        lv_coord_t start_x = is_back ? -LCD_W : LCD_W;
        lv_coord_t end_x   = is_back ?  LCD_W : -LCD_W;

        prepare_new_page(new_route, start_x);      // 先执行了 new_route->enter()，此时旧页面的内存还没释放。  在转场动画的 300ms 内，内存里同时存在两个页面的所有对象 
        cur_page = target;

        // 新页面滑入 (负责解锁)
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, page_root);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
        lv_anim_set_values(&a, start_x, 0);
        lv_anim_set_time(&a, ANIM_TIME_MS);          
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);   // 缓速退出，动效更自然
        lv_anim_set_ready_cb(&a, ui_anim_finished_cb);    // 新页面就位，解锁 UI
        lv_anim_start(&a);
	 

        // 旧页面滑出 (只管动，不关锁)
        lv_anim_t b;
        lv_anim_init(&b);
        lv_anim_set_var(&b, g_anim_ctx.old_root);
        lv_anim_set_exec_cb(&b, (lv_anim_exec_xcb_t)lv_obj_set_x);
        lv_anim_set_values(&b, 0, end_x);
        lv_anim_set_time(&b, ANIM_TIME_MS);
        lv_anim_set_path_cb(&b, lv_anim_path_ease_out);
        lv_anim_start(&b);
		
	}
}

////////////////////////////////////////////////核心切换引擎结束///////////////////////////////////////////////////////


/////////////////////////////////////////////// 对外 API 接口开始/////////////////////////////////////////////////////////////// 
void UI_Init(void) {
    UI_Goto(UI_PAGE_HOME);     // 上电自动跳转主页
}

void UI_Goto(UIPage_t page) {
    if (ui_animating || page == cur_page) return;  // 动画锁或重入保护

    // 历史栈压入与溢出保护
    if (cur_page != UI_PAGE_NONE) {
        if (stack_top < PAGE_STACK_DEPTH - 1) {          // 历史页入栈：只有“前进”才需要记录轨迹
            page_stack[++stack_top] = cur_page;
        } else {                                            
            // 工业做法：平移栈，丢弃最早的历史
            for(int i=0; i<PAGE_STACK_DEPTH-1; i++) page_stack[i] = page_stack[i+1];
            page_stack[stack_top] = cur_page;
        }
    }
    ui_switch(page, false);   // 执行切换，is_back = false
}

void UI_Back(void) {
    if (ui_animating || stack_top < 0) return;    // 锁保护或已经到顶了
    UIPage_t prev = page_stack[stack_top--];   // 弹栈，获取上一页 ID
    ui_switch(prev, true);      // 执行切换，is_back = true
}
////////////////////////////////////////// 对外 API 接口结束/////////////////////////////////////////////////////////////// 





/*======================================================================
 * 页面具体实现
 *======================================================================*/

 static void menu_event_cb(lv_event_t *e) {
 // 获取你在 add_event_cb 最后一个参数传进来的数字
    uintptr_t id = (uintptr_t)lv_event_get_user_data(e);            //lv_event_get_user_data 返回的是一个指针（void *）。但我们其实只是想传一个简单的数字
	//uintptr_t 是一种特殊的整数类型，它的长度刚好等于指针的长度     因为在 32 位单片机（STM32）里，指针是 32 位的。直接把数字 2 强转成指针发送，再强转回来，是最节省内存、速度最快的方法。
	 
      switch(id) {
        // [修正] 使用枚举名，即便以后顺序变了，逻辑依然稳如泰山
        case UI_PAGE_IMAGE:   UI_Goto(UI_PAGE_IMAGE);   break;
        case UI_PAGE_SETTING: UI_Goto(UI_PAGE_SETTING); break;
        case UI_PAGE_1:       UI_Goto(UI_PAGE_1);       break; 
        case UI_PAGE_2:       UI_Goto(UI_PAGE_2);       break; 
        case UI_PAGE_3:       UI_Goto(UI_PAGE_3);       break; 
        case UI_PAGE_4:       UI_Goto(UI_PAGE_4);       break; 
        case UI_PAGE_5:       UI_Goto(UI_PAGE_5);       break; 
		
        default: break;
    }
}
 
/**
 * 优雅的列表项工厂函数
 * @param list 父列表对象
 * @param symbol 图标 (如 LV_SYMBOL_SETTINGS)
 * @param text 文字 (汉字或英文)
 * @param id 页面 ID (void *)
 * @return 创建好的按钮对象
 */
static lv_obj_t * add_list_item(lv_obj_t * list, const char * symbol, const char * text, UIPage_t target_page)  {
    // 1. 创建按钮
    lv_obj_t * btn = lv_list_add_btn(list, symbol, text);        //lv_list_add_btn 会在内存里创建一个真实的按钮对象，并返回它的地址（指针）。当你写 btn = ... 时，你只是用这个叫 btn 的盒子临时装了一下这个地址。当你下一次写 btn = ... 时，盒子里的地址换成了新按钮的，但旧的按钮依然安安静静地躺在内存里，也挂载在 list 下面。
    
    // 2. 定向给内部文字 Label 应用 16px 样式
    // 这样图标保持默认大小，汉字使用 16px
    lv_obj_t * lbl = lv_obj_get_child(btn, 1);
    lv_obj_add_style(lbl, &style_16_Black, 0); 
    
    // 3. 设置按钮高度 (16px 字体配合 40-42 像素高度视觉最舒适)
    //  lv_obj_set_height(btn, 40);
	
	//  直接透传枚举值
    lv_obj_add_event_cb(btn, menu_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)target_page);  // 绑定事件
    if(main_group) lv_group_add_obj(main_group, btn);     // 趁现在，加入编码器组
    return btn;
}

static void page_home_enter(void) {
	
	lv_obj_t *obj = lv_label_create(page_root);
    lv_obj_add_style(obj, &style_24_BLUE, 0);               //字体选择，不选即默认字体
    lv_label_set_text(obj, "主菜单");                   //主菜单标题
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, 10);   

    lv_obj_t *list = lv_list_create(page_root);
    lv_obj_set_size(list, LCD_W - 20, LCD_H - 60);              //尺寸对齐
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -10);
	
    lv_obj_t *first_btn = NULL; // 专门开个小盒子，记住第一个按钮，用来设置焦点      
	
    first_btn = add_list_item(list, LV_SYMBOL_IMAGE,    "花海",   UI_PAGE_IMAGE);   // 记住它是老大，焦点
    add_list_item(list, LV_SYMBOL_SETTINGS, "亮度",               UI_PAGE_SETTING);
    add_list_item(list, LV_SYMBOL_LIST,     "海绵宝宝",           UI_PAGE_1);
    add_list_item(list, LV_SYMBOL_LIST,     "测试",               UI_PAGE_2);
    add_list_item(list, LV_SYMBOL_AUDIO,    "频谱",               UI_PAGE_3);             
    add_list_item(list, LV_SYMBOL_EYE_OPEN, "示波",               UI_PAGE_4);
    add_list_item(list, LV_SYMBOL_LIST,     "小猫咪",             UI_PAGE_5);
	
	
	 //每次进入后老大获得焦点
    if(first_btn) {
        lv_group_focus_obj(first_btn); 
        lv_obj_scroll_to_view(first_btn, LV_ANIM_OFF);
    }
	
	
}



static void page_home_exit_logic(void) {}
//主菜单	
	
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void back_btn_cb(lv_event_t *e) { UI_Back(); }              //绑定返回按键
/**
 * 极简·半透明冰晶返回键
 */
static lv_obj_t * add_back_btn_minimal(lv_obj_t * parent) {
    // 1. 创建按钮
    lv_obj_t * btn = lv_btn_create(parent);
    
    // 2. 尺寸瘦身：宽度从 100 缩减到 60，高度保持 30 (配合 16px 汉字)
    lv_obj_set_size(btn, 60, 30); 
    lv_obj_set_style_radius(btn, 15, 0); // 依然保持圆润的胶囊感
    
    // 3. 极致透明度：从 OPA_80 降到 OPA_50 (50%透明)
    // 这样它就像一块落在内容上的半透明冰块，能隐约看到后面的图片内容
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_50, 0); 
    
    // 4. 细节美化：去掉边框，减少视觉干扰
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    
    // 5. 位置：依然在底部中间，距离底部 10像素
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -7);
    
    // 6. 纯净文字：仅保留“返回”
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "返回"); 
    lv_obj_add_style(label, &style_16_White, 0); // 喜欢白色
    lv_obj_center(label);
	
	// --- 点击反馈状态 (状态设为 LV_STATE_PRESSED) ---
    // 当点下去的一瞬间，按钮会变成深蓝色，文字依然白色
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED); // 按下时不透明，更有力
    
    
    // 7. 绑定事件与组
    lv_obj_add_event_cb(btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(main_group, btn); 
    
    return btn;
}

static void page_image_enter(void) {
    lv_obj_t *img = lv_img_create(page_root);
    lv_img_set_src(img, "S:huahai.bin"); // 确保文件系统已挂载
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

   
    lv_obj_t * btn_back = add_back_btn_minimal(page_root);    // 调用优雅的返回键
    lv_group_focus_obj(btn_back);     // 编码器用户体验优化：进入页面直接选中返回键
}

static void page_image_exit_logic(void) {}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* 定义一个静态指针，让回调函数能抓到这个标签 */
static lv_obj_t * brightness_label;

	
	static void slider_event_cb(lv_event_t * e) {                          //滑块事件回调
		lv_obj_t * slider = lv_event_get_target(e);
		int32_t val = lv_slider_get_value(slider);   // 1. 获取滑块当前数值    (0~1000)
		g_sys_config.backlight_val = (uint16_t)val;        // 2. 同步到全局变量（这就是“永久记忆”的关键）
		LCD_Set_Brightness(g_sys_config.backlight_val);      // 3. 驱动硬件
			
		// 将 0~1000 映射到 0~100% (简单除以 10 即可)
		lv_label_set_text_fmt(brightness_label, "%d%%", (int)val / 10);       //  动态更新标签文字
	}
	
	
	static void page_setting_enter(void) {
		
		   
		lv_obj_t * title = lv_label_create(page_root);
		lv_label_set_text(title, "Display Brightness");      // 标题
		lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
		
			 // 2. 创建滑块 (Slider)
		lv_obj_t * slider = lv_slider_create(page_root);
		lv_obj_set_size(slider, 160, 15); // 窄一点，给数字留位置
		lv_obj_align(slider, LV_ALIGN_CENTER, -20, 0); // 稍微往左偏一点
		lv_slider_set_range(slider, 0, 1000);
		lv_slider_set_value(slider, g_sys_config.backlight_val, LV_ANIM_OFF);
		lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
			

		// 3. 创建百分比标签 (Label)
		brightness_label = lv_label_create(page_root);
		/* 初始显示当前百分比 */
		lv_label_set_text_fmt(brightness_label, "%d%%", g_sys_config.backlight_val / 10);
		/* 关键：将标签对齐到滑块的右侧，间距 10 像素 */
		lv_obj_align_to(brightness_label, slider, LV_ALIGN_OUT_RIGHT_MID, 15, 0);

		lv_group_add_obj(main_group, slider);   // 手动入组，让编码器能控制

		lv_obj_t * btn_back = add_back_btn_minimal(page_root);    // 调用优雅的返回键
	}

	static void page_setting_exit_logic(void) {}
		

		
	
	
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
static void page_1_enter(void) {
 
    // 1. 创建动画图片组件
    lv_obj_t * anim_heart = lv_animimg_create(page_root);
    
    // 2. 定义图片路径数组
    // 核心点：路径必须与你的 flash_res_table 中的字符串匹配
    static const char * heart_paths[] = {
         "S:frame_00.bin",
		 "S:frame_01.bin",
		 "S:frame_02.bin",
		 "S:frame_03.bin",
		 "S:frame_04.bin",
		 "S:frame_05.bin",
		 "S:frame_06.bin",
		 "S:frame_07.bin",
		 "S:frame_08.bin",
		 "S:frame_09.bin",
		 "S:frame_10.bin",
		 "S:frame_11.bin",
		 "S:frame_12.bin",
		 "S:frame_13.bin",
		 "S:frame_14.bin",
		 "S:frame_15.bin",
		 "S:frame_16.bin",
		 "S:frame_17.bin",
		 "S:frame_18.bin",
		 "S:frame_19.bin",
		 "S:frame_20.bin",
		 "S:frame_21.bin",
		 "S:frame_22.bin",
		 "S:frame_23.bin",
		 "S:frame_24.bin",
		 "S:frame_25.bin",
		 "S:frame_26.bin",
		 "S:frame_27.bin",
		 "S:frame_28.bin",
		 "S:frame_29.bin",
		 "S:frame_30.bin",
		 "S:frame_31.bin",
		 "S:frame_32.bin",
		 "S:frame_33.bin",
		 "S:frame_34.bin",
		 "S:frame_35.bin",
		 "S:frame_36.bin",
		 "S:frame_37.bin",
		 "S:frame_38.bin",
		 "S:frame_39.bin",
		 "S:frame_40.bin",
		 "S:frame_41.bin",
		 "S:frame_42.bin",
		 "S:frame_43.bin",
		 "S:frame_44.bin",
		 "S:frame_45.bin",
		 "S:frame_46.bin",
		 "S:frame_47.bin",
		 "S:frame_48.bin",
		 "S:frame_49.bin",
		 "S:frame_50.bin",
		 "S:frame_51.bin",
		 "S:frame_52.bin",
		 "S:frame_53.bin",
		 "S:frame_54.bin",
		 "S:frame_55.bin",
		 "S:frame_56.bin",
		 "S:frame_57.bin",
		 "S:frame_58.bin",
		 "S:frame_59.bin",
		 "S:frame_60.bin",
		 "S:frame_61.bin",
		 "S:frame_62.bin",
		 "S:frame_63.bin",
		 "S:frame_64.bin",
		 "S:frame_65.bin",
		 "S:frame_66.bin",
		 "S:frame_67.bin",
		 "S:frame_68.bin",
		 "S:frame_69.bin",
		 "S:frame_70.bin",
		 "S:frame_71.bin",
		 "S:frame_72.bin",
		 "S:frame_73.bin",
			};

    // 3. 配置动画：5 帧图片
    lv_animimg_set_src(anim_heart, (const void **)heart_paths, 74);
    
    // 4. 设置播放速度 ，4000为总播放时间
    lv_animimg_set_duration(anim_heart,5000); 
    
    // 5. 设置无限循环
    lv_animimg_set_repeat_count(anim_heart, LV_ANIM_REPEAT_INFINITE);     //LV_ANIM_REPEAT_INFINITE 是 LVGL 定义的宏，表示无限循环播放。也可以指定具体次数（如 3 表示播放 3 轮后停止

    lv_animimg_start(anim_heart);
    lv_obj_align(anim_heart, LV_ALIGN_CENTER, 0, 0);
 

	lv_obj_t * btn_back = add_back_btn_minimal(page_root);    // 调用优雅的返回键
}

static void page_1_exit_logic(void) {}	
	
static void page_2_enter(void) {
	lv_obj_t *img = lv_img_create(page_root);
    lv_img_set_src(img, "S:test.bin"); // 确保文件系统已挂载
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

   
    lv_obj_t * btn_back = add_back_btn_minimal(page_root);    // 调用优雅的返回键
    lv_group_focus_obj(btn_back);     // 编码器用户体验优化：进入页面直接选中返回键

}

static void page_2_exit_logic(void) {}	
/////////////////////////////////////////////////////////////////////////////////////////////////////
/* --- 外部资源引用 --- */
extern uint16_t adc_buffer[SAMPLE_SIZE]; // 硬件 DMA 缓冲区
extern uint8_t Flag;                    // 采样完成标志位

/* ==========================================================================
   Page 3: 频谱分析仪 (Spectrum Analyzer) - 琥珀色风格
   ========================================================================== */
static lv_obj_t * chart_fft = NULL;
static lv_chart_series_t * ser_fft = NULL;
static lv_obj_t * label_fft_info = NULL;
static lv_timer_t * timer_fft_ui = NULL;

/**
 * @brief FFT UI 刷新回调：仅负责展示 fft_module 算出的结果
 */
static void fft_ui_refresh_cb(lv_timer_t * t) {
    fft_result_t *res = fft_get_result();
    
    // 如果算法引擎正在忙，我们这一帧先不更新，避免画面闪烁
    if (res->is_busy) return;

    // 1. 刷新柱状图 (显示前 64 个频点)
    for (int i = 0; i < 64; i++) {
        // res->spectrum[i] 已经是归一化后的电压幅值
        ser_fft->y_points[i] = (lv_coord_t)(res->spectrum[i] );         //0到625hz
    }
    lv_chart_refresh(chart_fft);

    // 2. 刷新多行看板 (手动拆解浮点数，保证 100% 显示成功)
    int f_i = (int)res->freq_main, f_d = (int)(res->freq_main * 100) % 100;
    int t_i = (int)res->thd,       t_d = (int)(res->thd * 100) % 100;
    int p_i = (int)res->vpp,       p_d = (int)(res->vpp * 100) % 100;
    int r_i = (int)res->vrms,      r_d = (int)(res->vrms * 100) % 100;
    int a_i = (int)res->vdc,       a_d = (int)(res->vdc * 100) % 100;

	  lv_label_set_text_fmt(label_fft_info, 
		"#0000FF Freq:%d.%02dHz#  #B22222 THD:%d.%02d%%#\n"
		"#008000 Vpp:%d.%02dV#  #008080 Vrms:%d.%02dV#  #4B4B4B Vdc:%d.%02dV#", 
		f_i, f_d, t_i, t_d, p_i, p_d, r_i, r_d, a_i, a_d);
}

void page_3_enter(void) {
    // 1. 初始化 FFT 模块 (内部已处理 CCM 申请)
    fft_module_init();

    // 2. 信息看板 (支持彩色重绘)
    label_fft_info = lv_label_create(page_root);
    lv_label_set_recolor(label_fft_info, true);
    lv_obj_set_style_text_align(label_fft_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_fft_info, LV_ALIGN_TOP_MID, 0, 5);

    // 3. 频谱 Chart (柱状图风格)
    chart_fft = lv_chart_create(page_root);
    lv_obj_set_size(chart_fft, 280, 150);
    lv_obj_align(chart_fft, LV_ALIGN_CENTER, 0, 10);
    lv_chart_set_type(chart_fft, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(chart_fft, 64);
    lv_chart_set_range(chart_fft, LV_CHART_AXIS_PRIMARY_Y, 0, 1500);
    
    // 样式美化
    lv_obj_set_style_bg_color(chart_fft, lv_color_make(30, 30, 30), 0);
    lv_obj_set_style_pad_column(chart_fft, 2, 0); 
    ser_fft = lv_chart_add_series(chart_fft, lv_palette_main(LV_PALETTE_AMBER), LV_CHART_AXIS_PRIMARY_Y);

    // 4. 定时器 (50ms 刷新)
    timer_fft_ui = lv_timer_create(fft_ui_refresh_cb, 50, NULL);

	lv_obj_t * btn_back = add_back_btn_minimal(page_root);    // 调用优雅的返回键
}

void page_3_exit_logic(void) {
    if (timer_fft_ui) lv_timer_del(timer_fft_ui);
    chart_fft = NULL;
    label_fft_info = NULL;
}

/* ==========================================================================
   Page 4: 实时示波器 (Real-time Oscilloscope) - 荧光绿风格
   ========================================================================== */
static lv_obj_t * chart_osc = NULL;
static lv_chart_series_t * ser_osc = NULL;
static lv_timer_t * timer_osc_ui = NULL;

/**
 * @brief 示波器 UI 刷新：直接从硬件采样缓冲区抽吸点显示
 */
//static void osc_ui_refresh_cb(lv_timer_t * t) {
//    if (!chart_osc || !ser_osc) return;                     

//    // 清除当前波形，重新填入最新的快照
//    // 由于 adc_buffer 有 1024 点，屏幕只画 128 点，我们每 8 个点抽一个    通过 1:8 的抽稀    //   “显示采样率”变成了 10000/8=1.25 kHz
//    for (int i = 0; i < 128; i++) {
//        // adc_buffer[i*8] 范围是 0~4095
//        ser_osc->y_points[i] = (lv_coord_t)adc_buffer[i * 8];       //   一个点(1/10000*1000)=0.1ms ；0.1*128*8=102.4ms    //横向展示了 102.4 毫秒 的物理世界快照
//    }
//    lv_chart_refresh(chart_osc);
//}
static void osc_ui_refresh_cb(lv_timer_t * t) {
    if (!chart_osc || !ser_osc) return;

    // --- 1. 触发搜索配置 ---
    uint16_t threshold = 2048;      // 触发阈值 (1.65V)，你可以根据实际信号调整
    int trigger_offset = 0;         // 默认从 0 开始（如果找不到触发点）
    int search_range = 512;         // 我们在前 512 个点里找触发，留出后半段显示
    uint8_t found = 0;

    // --- 2. 搜索触发点 (上升沿) ---
    // 原理：前一个点低于阈值，当前点高于阈值
    for (int i = 1; i < search_range; i++) {
       // 增加一个 delta 判断，防止微小噪声误触发
		if (adc_buffer[i-1] < threshold && adc_buffer[i] >= threshold && (adc_buffer[i] - adc_buffer[i-1]) > 100) {
			trigger_offset = i;
			found = 1;
			break; // 抓到了，立即跳出循环
        }
    }

    // --- 3. 抽样显示逻辑 ---
    // 为了看得更细，我们把抽稀率从 1:8 改为 1:4 (展示约 50ms)
    // 这样波形在屏幕上会“放大”一倍，细节更清晰
    for (int i = 0; i < 128; i++) {
        int idx = trigger_offset + (i * 4); // 4倍采样
        
        // 防御性编程：防止数组越界（虽然 512+128*4 < 1024，但好习惯要有）
        if (idx >= SAMPLE_SIZE) idx = SAMPLE_SIZE - 1;
        
        ser_osc->y_points[i] = (lv_coord_t)adc_buffer[idx];
    }

    // --- 4. UI 视觉反馈 (可选) ---
    // 如果找到了触发，让边框亮一点，或者加个小指示
    if (found) {
        lv_obj_set_style_border_color(chart_osc, lv_palette_main(LV_PALETTE_LIGHT_GREEN), 0);
    } else {
        lv_obj_set_style_border_color(chart_osc, lv_palette_main(LV_PALETTE_GREY), 0);
    }

    lv_chart_refresh(chart_osc);
}

void page_4_enter(void) {
    // 1. 创建 Chart (折线图风格)
    chart_osc = lv_chart_create(page_root);
    lv_obj_set_size(chart_osc, 300, 180); // 示波器尽量铺满全屏
    lv_obj_align(chart_osc, LV_ALIGN_CENTER, 0, -10);
    
    lv_chart_set_type(chart_osc, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_osc, 128);
    lv_chart_set_range(chart_osc, LV_CHART_AXIS_PRIMARY_Y, 0, 4096);
    lv_obj_set_style_pad_all(chart_osc, 0, 0);

    // 2. 深度黑背景 + 极简网格线
    lv_obj_set_style_bg_color(chart_osc, lv_color_make(10, 10, 10), 0);
    lv_chart_set_div_line_count(chart_osc, 5, 0); // 增加横向网格感
    lv_obj_set_style_line_color(chart_osc, lv_color_make(40, 40, 40), LV_PART_MAIN);

    // 3. 亮绿色波形 (增加宽度增强视觉感)
    ser_osc = lv_chart_add_series(chart_osc, lv_palette_main(LV_PALETTE_LIGHT_GREEN), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_style_line_width(chart_osc, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_osc, 0, LV_PART_INDICATOR); // 隐藏圆点

    // 4. 定时器 (30ms 刷新，保证丝滑感)
    timer_osc_ui = lv_timer_create(osc_ui_refresh_cb, 30, NULL);


	lv_obj_t * btn_back = add_back_btn_minimal(page_root);    // 调用优雅的返回键
}

void page_4_exit_logic(void) {
    if (timer_osc_ui) lv_timer_del(timer_osc_ui);
    chart_osc = NULL;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
/*==============================
 * 1. 按键专用回调函数
 *==============================*/
static void btn_test_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    uintptr_t id = (uintptr_t)lv_event_get_user_data(e); // 获取区分 ID

    if(code == LV_EVENT_CLICKED) {
        // 根据 ID 执行不同逻辑
        if(id == 1) {
            // 左侧按键逻辑
			  USART6_SEND((uint8_t*)"ON\r\n", 4);
        } else {
            // 右侧按键逻辑
              USART6_SEND((uint8_t*)"OFF\r\n", 5);
        }
    }
}

static void page_5_enter(void) {
	// A. 标题
    lv_obj_t *title = lv_label_create(page_root);
    lv_obj_add_style(title, &style_24_BLUE, 0); 
    lv_label_set_text(title, "LoRa");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // B. 创建左侧按键 (确认键)
    lv_obj_t * btn_left = lv_btn_create(page_root);
    lv_obj_set_size(btn_left, 100, 45); // 工业风尺寸：宽100，高45
    // 关键点：相对于中心向左偏移 70 像素
    lv_obj_align(btn_left, LV_ALIGN_CENTER, -70, 0); 
    
    lv_obj_t * label_l = lv_label_create(btn_left);
    lv_label_set_text(label_l, "开");
    lv_obj_add_style(label_l, &style_16_Black, 0);
    lv_obj_center(label_l);

    // 绑定事件 (传 ID: 1)
    lv_obj_add_event_cb(btn_left, btn_test_event_cb, LV_EVENT_CLICKED, (void *)1);
    if(main_group) lv_group_add_obj(main_group, btn_left);

    // C. 创建右侧按键 (返回键)
    lv_obj_t * btn_right = lv_btn_create(page_root);
    lv_obj_set_size(btn_right, 100, 45);
    // 关键点：相对于中心向右偏移 70 像素
    lv_obj_align(btn_right, LV_ALIGN_CENTER, 70, 0); 
    
    lv_obj_t * label_r = lv_label_create(btn_right);
    lv_label_set_text(label_r, "关");
    lv_obj_add_style(label_r, &style_16_Black, 0);
    lv_obj_center(label_r);

    // 绑定事件 (传 ID: 2)
    lv_obj_add_event_cb(btn_right, btn_test_event_cb, LV_EVENT_CLICKED, (void *)2);
    if(main_group) lv_group_add_obj(main_group, btn_right);
	
	
	  // 1. 创建动画图片组件
    lv_obj_t * anim_heart = lv_animimg_create(page_root);
    
    // 2. 定义图片路径数组
    // 核心点：路径必须与你的 flash_res_table 中的字符串匹配
    static const char * heart_paths[] = {
        "S:gif0.bin",
        "S:gif1.bin",
        "S:gif2.bin",
        "S:gif3.bin", // 注意：如果你的表里没写路径，就直接写 S:/gif3.bin
        "S:gif4.bin"
    };

    // 3. 配置动画：5 帧图片
    lv_animimg_set_src(anim_heart, (const void **)heart_paths, 5);
    
    // 4. 设置播放速度 (5帧，建议 500ms~800ms 播完一轮，比较自然)
    lv_animimg_set_duration(anim_heart, 600); 
    
    // 5. 设置无限循环
    lv_animimg_set_repeat_count(anim_heart, LV_ANIM_REPEAT_INFINITE);

    // 6. 启动并对齐
    lv_animimg_start(anim_heart);
    lv_obj_align(anim_heart, LV_ALIGN_CENTER, 0, 0);
 

	lv_obj_t * btn_back = add_back_btn_minimal(page_root);    // 调用优雅的返回键
}

static void page_5_exit_logic(void) {}		
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	/**
 * @brief 设置屏幕亮度
 * @param brightness: 0 ~ 1000 (0为全黑，1000为最亮)
 */
void LCD_Set_Brightness(uint16_t brightness)
{
    if(brightness > 1000) brightness = 1000;
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, brightness);
}





void Menu_OnEvent(const Event_t *e)
{
    if (!e) return;

    switch (e->type)
    {

       case EVT_KEY_PRESS:
		    switch (e->param) {
				case 0:  
//					BSP_W25Q_Fast_Read(buffer, 0, 388); 
//					printf("FLASH data: ");
//					for(int i = 0; i < 388; i++) {printf("%02X ", buffer[i]);}	
					
				  custom_ccm_monitor_init();
                  custom_sys_monitor_init();
				
					break;
			    case 1:  
					  UI_Back();
				break;
				case 2:  
				      lv_touchpad_toggle( );
	
				break;
			    case 3: 
					   lv_font_ccm_reset();

				break;
			} 
            break;
			
	   case EVT_KEY_LONG:
		    switch (e->param) {
				case 0:        break;
			    case 1:  printf("EVT_KEY_LONG   1\r\n");        break;
				case 2:  printf("EVT_KEY_LONG   2\r\n");        break;
			    case 3:  printf("EVT_KEY_LONG   3\r\n");        break;
			} 
            break;
			
			  


        default:
            break;
    }
}

