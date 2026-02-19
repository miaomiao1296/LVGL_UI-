#include "lv_port_fs_w25q.h"
#include "spi_flash.h" 
#include "resource_map.h"
#include "stdio.h"
#include "lcd.h"

/* ================= 资源表映射 ================= */
typedef struct {
    const char * name;
    uint32_t offset;
    uint32_t size;
} FlashResource_t;

// 根据你提供的最新信息更新
const FlashResource_t flash_res_table[TOTAL_RESOURCE_COUNT] = {
    {"test.bin", RES_IMG_TEST_OFFSET, RES_IMG_TEST_SIZE},
	{"huahai.bin", RES_IMG_huahai_OFFSET, RES_IMG_huahai_SIZE},
    {"font_cn_24.bin", font_cn_24_OFFSET, font_cn_24_SIZE},
	{"font_cn_16.bin", font_cn_16_OFFSET, font_cn_16_SIZE},
	{"gif0.bin", gif0_OFFSET, gif0_SIZE},
	{"gif1.bin", gif1_OFFSET, gif1_SIZE},
	{"gif2.bin", gif2_OFFSET, gif2_SIZE},
	{"gif3.bin", gif3_OFFSET, gif3_SIZE},
	{"gif4.bin", gif4_OFFSET, gif4_SIZE},  
{"frame_00.bin", frame_00_OFFSET, frame_00_SIZE},
{"frame_01.bin", frame_01_OFFSET, frame_01_SIZE},
{"frame_02.bin", frame_02_OFFSET, frame_02_SIZE},
{"frame_03.bin", frame_03_OFFSET, frame_03_SIZE},
{"frame_04.bin", frame_04_OFFSET, frame_04_SIZE},
{"frame_05.bin", frame_05_OFFSET, frame_05_SIZE},
{"frame_06.bin", frame_06_OFFSET, frame_06_SIZE},
{"frame_07.bin", frame_07_OFFSET, frame_07_SIZE},
{"frame_08.bin", frame_08_OFFSET, frame_08_SIZE},
{"frame_09.bin", frame_09_OFFSET, frame_09_SIZE},
{"frame_10.bin", frame_10_OFFSET, frame_10_SIZE},
{"frame_11.bin", frame_11_OFFSET, frame_11_SIZE},
{"frame_12.bin", frame_12_OFFSET, frame_12_SIZE},
{"frame_13.bin", frame_13_OFFSET, frame_13_SIZE},
{"frame_14.bin", frame_14_OFFSET, frame_14_SIZE},
{"frame_15.bin", frame_15_OFFSET, frame_15_SIZE},
{"frame_16.bin", frame_16_OFFSET, frame_16_SIZE},
{"frame_17.bin", frame_17_OFFSET, frame_17_SIZE},
{"frame_18.bin", frame_18_OFFSET, frame_18_SIZE},
{"frame_19.bin", frame_19_OFFSET, frame_19_SIZE},
{"frame_20.bin", frame_20_OFFSET, frame_20_SIZE},
{"frame_21.bin", frame_21_OFFSET, frame_21_SIZE},
{"frame_22.bin", frame_22_OFFSET, frame_22_SIZE},
{"frame_23.bin", frame_23_OFFSET, frame_23_SIZE},
{"frame_24.bin", frame_24_OFFSET, frame_24_SIZE},
{"frame_25.bin", frame_25_OFFSET, frame_25_SIZE},
{"frame_26.bin", frame_26_OFFSET, frame_26_SIZE},
{"frame_27.bin", frame_27_OFFSET, frame_27_SIZE},
{"frame_28.bin", frame_28_OFFSET, frame_28_SIZE},
{"frame_29.bin", frame_29_OFFSET, frame_29_SIZE},
{"frame_30.bin", frame_30_OFFSET, frame_30_SIZE},
{"frame_31.bin", frame_31_OFFSET, frame_31_SIZE},
{"frame_32.bin", frame_32_OFFSET, frame_32_SIZE},
{"frame_33.bin", frame_33_OFFSET, frame_33_SIZE},
{"frame_34.bin", frame_34_OFFSET, frame_34_SIZE},
{"frame_35.bin", frame_35_OFFSET, frame_35_SIZE},
{"frame_36.bin", frame_36_OFFSET, frame_36_SIZE},
{"frame_37.bin", frame_37_OFFSET, frame_37_SIZE},
{"frame_38.bin", frame_38_OFFSET, frame_38_SIZE},
{"frame_39.bin", frame_39_OFFSET, frame_39_SIZE},
{"frame_40.bin", frame_40_OFFSET, frame_40_SIZE},
{"frame_41.bin", frame_41_OFFSET, frame_41_SIZE},
{"frame_42.bin", frame_42_OFFSET, frame_42_SIZE},
{"frame_43.bin", frame_43_OFFSET, frame_43_SIZE},
{"frame_44.bin", frame_44_OFFSET, frame_44_SIZE},
{"frame_45.bin", frame_45_OFFSET, frame_45_SIZE},
{"frame_46.bin", frame_46_OFFSET, frame_46_SIZE},
{"frame_47.bin", frame_47_OFFSET, frame_47_SIZE},
{"frame_48.bin", frame_48_OFFSET, frame_48_SIZE},
{"frame_49.bin", frame_49_OFFSET, frame_49_SIZE},
{"frame_50.bin", frame_50_OFFSET, frame_50_SIZE},
{"frame_51.bin", frame_51_OFFSET, frame_51_SIZE},
{"frame_52.bin", frame_52_OFFSET, frame_52_SIZE},
{"frame_53.bin", frame_53_OFFSET, frame_53_SIZE},
{"frame_54.bin", frame_54_OFFSET, frame_54_SIZE},
{"frame_55.bin", frame_55_OFFSET, frame_55_SIZE},
{"frame_56.bin", frame_56_OFFSET, frame_56_SIZE},
{"frame_57.bin", frame_57_OFFSET, frame_57_SIZE},
{"frame_58.bin", frame_58_OFFSET, frame_58_SIZE},
{"frame_59.bin", frame_59_OFFSET, frame_59_SIZE},
{"frame_60.bin", frame_60_OFFSET, frame_60_SIZE},
{"frame_61.bin", frame_61_OFFSET, frame_61_SIZE},
{"frame_62.bin", frame_62_OFFSET, frame_62_SIZE},
{"frame_63.bin", frame_63_OFFSET, frame_63_SIZE},
{"frame_64.bin", frame_64_OFFSET, frame_64_SIZE},
{"frame_65.bin", frame_65_OFFSET, frame_65_SIZE},
{"frame_66.bin", frame_66_OFFSET, frame_66_SIZE},
{"frame_67.bin", frame_67_OFFSET, frame_67_SIZE},
{"frame_68.bin", frame_68_OFFSET, frame_68_SIZE},
{"frame_69.bin", frame_69_OFFSET, frame_69_SIZE},
{"frame_70.bin", frame_70_OFFSET, frame_70_SIZE},
{"frame_71.bin", frame_71_OFFSET, frame_71_SIZE},
{"frame_72.bin", frame_72_OFFSET, frame_72_SIZE},
{"frame_73.bin", frame_73_OFFSET, frame_73_SIZE},

};

/* 内部文件句柄 */
typedef struct {
    uint32_t start_addr;
    uint32_t current_pos;
    uint32_t file_size;
} w25q_file_t;

//extern volatile uint8_t SPI1_Bus_Owner;

/* 利用 DMA 的同步等待读取 --- */
static void W25Q_Read_DMA_Blocking(uint8_t* pBuffer, uint32_t addr, uint32_t size) {
	
	// 如果 LCD 正在发，这里必须等！
  //  while(SPI1_Bus_Owner == 1); 
    
  //  SPI1_Bus_Owner = 2; // 标记：Flash 占领总线
    // 1. 启动异步 DMA 读取
    BSP_W25Q_Read_DMA(pBuffer, addr, size, NULL);
    
    // 2. 阻塞等待，直到 DMA 完成 (由回调函数清空忙标志)
    // 这里的阻塞是合理的，因为 LVGL 需要拿到数据才能继续画
    while(BSP_W25Q_Is_DMA_Busy()); 
}

/* --- LVGL 回调实现 --- */

static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
	
    for(int i = 0; i < sizeof(flash_res_table)/sizeof(FlashResource_t); i++) {
        if(strcmp(path, flash_res_table[i].name) == 0) {
            w25q_file_t * f = lv_mem_alloc(sizeof(w25q_file_t));
            f->start_addr = flash_res_table[i].offset;
            f->file_size = flash_res_table[i].size;
            f->current_pos = 0;
            return f;
        }
    }
    return NULL; 
}



extern SPI_HandleTypeDef hspi1;

static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
    w25q_file_t * f = (w25q_file_t *)file_p;
    
    if(f->current_pos + btr > f->file_size) btr = f->file_size - f->current_pos;
    if(btr == 0) { if(br) *br = 0; return LV_FS_RES_OK; }

    // --- 阈值判断：图片通常很大（>2KB），字库通常很小 ---
    if (btr < 2048) {
        // 小于 2KB 走阻塞读，保证字库 RLE 解码的绝对稳定
        BSP_W25Q_Normal_Read(buf, f->start_addr + f->current_pos, btr);
    }
    else {
        // 大于 2KB 走 DMA（图片数据），发挥 F407 的速度优势
        // 这里的阻塞是为了防止 LVGL 在 DMA 还没完时就去读下一段
        W25Q_Read_DMA_Blocking(buf, f->start_addr + f->current_pos, btr);
    }
	
    f->current_pos += btr;
    if(br) *br = btr;
    return LV_FS_RES_OK;
}


static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
    w25q_file_t * f = (w25q_file_t *)file_p;
    if(whence == LV_FS_SEEK_SET) f->current_pos = pos;
    if(whence == LV_FS_SEEK_CUR) f->current_pos += pos;
    if(whence == LV_FS_SEEK_END) f->current_pos = f->file_size - pos;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
    w25q_file_t * f = (w25q_file_t *)file_p;
    *pos_p = f->current_pos;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p) {
    lv_mem_free(file_p);
    return LV_FS_RES_OK;
}

/* 注册函数：在 lv_init() 后调用 */
void lv_port_fs_init(void) {
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);

    fs_drv.letter = 'S'; // 驱动器盘符：S
    fs_drv.open_cb = fs_open;
    fs_drv.read_cb = fs_read;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;
    fs_drv.close_cb = fs_close;

    lv_fs_drv_register(&fs_drv);
}

