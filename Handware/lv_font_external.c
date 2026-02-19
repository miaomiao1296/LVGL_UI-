#include "lv_font_external.h"
#include <string.h>
#include "stdio.h"

/* ================= 调试与内存池配置 ================= */
#define FONT_LOG(fmt, ...) printf("[FontLoad] " fmt "\n", ##__VA_ARGS__)
#define FONT_ERR(fmt, ...) printf("[FontErr] !! " fmt " !!\n", ##__VA_ARGS__)

// STM32F407 CCM RAM (64KB)
#define CCM_POOL_SIZE (60 * 1024)
__attribute__((section(".ccmram"))) static uint8_t ccm_pool[CCM_POOL_SIZE];
static uint32_t ccm_ptr = 0;

static void * ccm_alloc(uint32_t size) {
    size = (size + 3) & ~3; // 4字节对齐
    if (ccm_ptr + size > CCM_POOL_SIZE) {
        FONT_ERR("CCM Overflow! Need:%u Free:%u", (unsigned int)size, (unsigned int)(CCM_POOL_SIZE - ccm_ptr));
        return NULL;
    }
    void * p = &ccm_pool[ccm_ptr];
    ccm_ptr += size;
    return p;
}

/* ================= 二进制解析辅助函数 ================= */
typedef struct {
    lv_fs_file_t * fp;
    int8_t bit_pos;
    uint8_t byte_value;
} bit_iterator_t;

static unsigned int read_bits(bit_iterator_t * it, int n_bits, lv_fs_res_t * res) {
    unsigned int value = 0;
    while(n_bits--) {
        it->byte_value = it->byte_value << 1;
        it->bit_pos--;
        if(it->bit_pos < 0) {
            it->bit_pos = 7;
            *res = lv_fs_read(it->fp, &(it->byte_value), 1, NULL);
            if(*res != LV_FS_RES_OK) return 0;
        }
        value |= ((it->byte_value & 0x80) ? 1 : 0) << n_bits;
    }
    *res = LV_FS_RES_OK;
    return value;
}

static int read_bits_signed(bit_iterator_t * it, int n_bits, lv_fs_res_t * res) {
    unsigned int value = read_bits(it, n_bits, res);
    if(value & (1 << (n_bits - 1))) value |= ~0u << n_bits;
    return (int)value;
}

/* ================= 映射查找与点阵流读 ================= */

static uint32_t get_glyph_id(const lv_font_t * font, uint32_t unicode) {
    lv_font_fmt_txt_dsc_t * dsc = (lv_font_fmt_txt_dsc_t *)font->dsc;
    lv_font_fmt_txt_cmap_t * cmaps = (lv_font_fmt_txt_cmap_t *)dsc->cmaps;
    for(int i = 0; i < dsc->cmap_num; i++) {
        if(unicode < cmaps[i].range_start) continue;
        if(unicode > (uint32_t)(cmaps[i].range_start + cmaps[i].range_length - 1)) continue;
        uint32_t utmp = unicode - cmaps[i].range_start;
        if(cmaps[i].type == LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY) return cmaps[i].glyph_id_start + utmp;
        if(cmaps[i].type == LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL) {
            const uint8_t * id_list = (const uint8_t *)cmaps[i].glyph_id_ofs_list;
            return cmaps[i].glyph_id_start + id_list[utmp];
        }
        if(cmaps[i].type == LV_FONT_FMT_TXT_CMAP_SPARSE_TINY || cmaps[i].type == LV_FONT_FMT_TXT_CMAP_SPARSE_FULL) {
            uint32_t low = 0, high = cmaps[i].list_length - 1;
            const uint16_t * u_list = cmaps[i].unicode_list;
            while(low <= high) {
                uint32_t mid = (low + high) / 2;
                if(u_list[mid] == (uint16_t)unicode) {
                    if(cmaps[i].type == LV_FONT_FMT_TXT_CMAP_SPARSE_TINY) return cmaps[i].glyph_id_start + mid;
                    else {
                        const uint16_t * id_list = (const uint16_t *)cmaps[i].glyph_id_ofs_list;
                        return cmaps[i].glyph_id_start + id_list[mid];
                    }
                }
                if(u_list[mid] < (uint16_t)unicode) low = mid + 1;
                else high = mid - 1;
            }
        }
    }
    return 0;
}

static const uint8_t * custom_get_glyph_bitmap(const lv_font_t * font, uint32_t unicode_letter) {
    lv_font_ext_dsc_t * ext_dsc = (lv_font_ext_dsc_t *)font->dsc;
    uint32_t gid = get_glyph_id(font, unicode_letter);
    if(gid == 0 && unicode_letter != ' ') return NULL;
    lv_font_fmt_txt_glyph_dsc_t * g_curr = (lv_font_fmt_txt_glyph_dsc_t *)&ext_dsc->lv_dsc.glyph_dsc[gid];
    if(g_curr->box_w == 0 || g_curr->box_h == 0) return NULL;
    static uint8_t bmp_cache[1024]; 
    uint32_t bpp = ext_dsc->lv_dsc.bpp;
    uint32_t size = (g_curr->box_w * g_curr->box_h * bpp + 7) / 8;
    if(size > sizeof(bmp_cache)) return NULL;
    uint32_t addr = ext_dsc->flash_glyf_start + g_curr->bitmap_index;
    extern void BSP_W25Q_Normal_Read(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t Size);
    BSP_W25Q_Normal_Read(bmp_cache, addr, size);
    return bmp_cache;
}

/* ================= 核心加载函数 ================= */

lv_font_t * lv_font_external_load(const char * font_name) {
    lv_fs_file_t f;
    if(lv_fs_open(&f, font_name, LV_FS_MODE_RD) != LV_FS_RES_OK) return NULL;

    FONT_LOG("Entry: %s", font_name);

    // --- 1. 解析 Header (48 字节) ---
    uint32_t block_len; char tag[4];
    lv_fs_read(&f, &block_len, 4, NULL);
    lv_fs_read(&f, tag, 4, NULL);
    
    // 正确的 Body 长度 = 块总长 - 4字节Tag
    uint8_t h[64]; 
    lv_fs_read(&f, h, block_len - 4, NULL); 

    lv_font_t * font = lv_mem_alloc(sizeof(lv_font_t));
    lv_font_ext_dsc_t * ext_dsc = lv_mem_alloc(sizeof(lv_font_ext_dsc_t));
    memset(font, 0, sizeof(lv_font_t));
    memset(ext_dsc, 0, sizeof(lv_font_ext_dsc_t));

    font->dsc = ext_dsc;
    font->get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt;
    font->get_glyph_bitmap = custom_get_glyph_bitmap;
    font->subpx = h[34]; // V1 Spec
    font->line_height = (h[4]|h[5]<<8) - (int16_t)(h[6]|h[7]<<8);
    font->base_line = -(int16_t)(h[6]|h[7]<<8);
    ext_dsc->lv_dsc.bpp = h[29]; // V1 Spec

    FONT_LOG("Header: BPP:%d Height:%d", ext_dsc->lv_dsc.bpp, font->line_height);

    // --- 2. 解析 CMap ---
    lv_fs_read(&f, &block_len, 4, NULL);
    lv_fs_read(&f, tag, 4, NULL); // "cmap"
    uint32_t subtables;
    lv_fs_read(&f, &subtables, 4, NULL);
    
    ext_dsc->lv_dsc.cmap_num = subtables;
    ext_dsc->lv_dsc.cmaps = ccm_alloc(subtables * sizeof(lv_font_fmt_txt_cmap_t));
    
    uint32_t cmap_base_pos; lv_fs_tell(&f, &cmap_base_pos);

    for(uint32_t i=0; i<subtables; i++) {
        uint32_t off, start; uint16_t range_len, id_start, entry_cnt; uint8_t type;
        lv_fs_read(&f, &off, 4, NULL);
        lv_fs_read(&f, &start, 4, NULL);
        lv_fs_read(&f, &range_len, 2, NULL);
        lv_fs_read(&f, &id_start, 2, NULL);
        lv_fs_read(&f, &entry_cnt, 2, NULL);
        lv_fs_read(&f, &type, 1, NULL);
        lv_fs_read(&f, tag, 1, NULL); // padding

        lv_font_fmt_txt_cmap_t * c = (lv_font_fmt_txt_cmap_t *)&ext_dsc->lv_dsc.cmaps[i];
        c->range_start = start;
        c->range_length = range_len;
        c->glyph_id_start = id_start;
        c->type = type;
        c->list_length = entry_cnt;

        uint32_t saved_pos; lv_fs_tell(&f, &saved_pos);
        lv_fs_seek(&f, cmap_base_pos - 4 + off, LV_FS_SEEK_SET); 

        if(type == LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL) {
            c->glyph_id_ofs_list = ccm_alloc(range_len);
            lv_fs_read(&f, (void*)c->glyph_id_ofs_list, range_len, NULL);
        }
        else if(type == LV_FONT_FMT_TXT_CMAP_SPARSE_TINY || type == LV_FONT_FMT_TXT_CMAP_SPARSE_FULL) {
            c->unicode_list = ccm_alloc(entry_cnt * 2);
            lv_fs_read(&f, (void*)c->unicode_list, entry_cnt * 2, NULL);
            if(type == LV_FONT_FMT_TXT_CMAP_SPARSE_FULL) {
                c->glyph_id_ofs_list = ccm_alloc(entry_cnt * 2);
                lv_fs_read(&f, (void*)c->glyph_id_ofs_list, entry_cnt * 2, NULL);
            }
        }
        lv_fs_seek(&f, saved_pos, LV_FS_SEEK_SET);
    }
    FONT_LOG("CMap: OK Tables:%d", (int)subtables);

    // --- 3. 解析 Loca ---
    lv_fs_read(&f, &block_len, 4, NULL);
    lv_fs_read(&f, tag, 4, NULL); // "loca"
    uint32_t loca_cnt;
    lv_fs_read(&f, &loca_cnt, 4, NULL);
    
    uint32_t * offsets = lv_mem_alloc(loca_cnt * 4);
    if(h[26] == 0) { // index_to_loc_format
        for(uint32_t i=0; i<loca_cnt; i++) { uint16_t tmp; lv_fs_read(&f, &tmp, 2, NULL); offsets[i] = tmp; }
    } else {
        lv_fs_read(&f, offsets, loca_cnt * 4, NULL);
    }

    // --- 4. 解析 Glyf ---
    lv_fs_read(&f, &block_len, 4, NULL);
    lv_fs_read(&f, tag, 4, NULL); // "glyf"
    uint32_t glyf_base_pos; lv_fs_tell(&f, &glyf_base_pos);
    ext_dsc->flash_glyf_start = glyf_base_pos;

    ext_dsc->lv_dsc.glyph_dsc = ccm_alloc(loca_cnt * sizeof(lv_font_fmt_txt_glyph_dsc_t));
    
    uint32_t cur_bmp_idx = 0;
    uint8_t adv_bits = h[32], xy_bits = h[30], wh_bits = h[31];
    uint16_t def_adv = h[22]|h[23]<<8;

    for(uint32_t i=0; i<loca_cnt; i++) {
        lv_font_fmt_txt_glyph_dsc_t * gdsc = (lv_font_fmt_txt_glyph_dsc_t *)&ext_dsc->lv_dsc.glyph_dsc[i];
        lv_fs_seek(&f, glyf_base_pos + offsets[i], LV_FS_SEEK_SET);
        bit_iterator_t it = {&f, -1, 0}; lv_fs_res_t res;
        gdsc->adv_w = (adv_bits == 0) ? def_adv : read_bits(&it, adv_bits, &res);
        if(h[28] == 0) gdsc->adv_w *= 16;
        gdsc->ofs_x = read_bits_signed(&it, xy_bits, &res);
        gdsc->ofs_y = read_bits_signed(&it, xy_bits, &res);
        gdsc->box_w = read_bits(&it, wh_bits, &res);
        gdsc->box_h = read_bits(&it, wh_bits, &res);
        gdsc->bitmap_index = cur_bmp_idx;
        int nbits = adv_bits + 2*xy_bits + 2*wh_bits;
        int next_off = (i < loca_cnt - 1) ? offsets[i+1] : (block_len - 4);
        cur_bmp_idx += (next_off - offsets[i] - nbits/8);
    }

    lv_mem_free(offsets);
    lv_fs_close(&f);
    FONT_LOG("SUCCESS: CCM Used:%d Glyphs:%d", (int)ccm_ptr, (int)loca_cnt);
    return font;
}

void lv_font_external_free(lv_font_t * font) {
    if(font) {
        if(font->dsc) lv_mem_free((void *)font->dsc);
        lv_mem_free(font);
    }
}