/*********************
 *      INCLUDES
 *********************/

#include <rtdevice.h>
#include <rtthread.h>

#include "app_mem.h"
#include "gui_app_fwk.h"
#include "littlevgl2rtt.h"
#include "lv_ex_data.h"
#include "lv_ext_resource_manager.h"
#include "lvgl.h"
#include "lvsf_comp.h"
#include "screens.h"
#include "um_gps_if.h"

#define DBG_TAG "gui_apps.gps"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define lv_obj_set_style_text_font(obj, size, selector) \
    lv_ext_set_local_font(obj, size, lv_obj_get_style_text_color(obj, selector))
#define ui_font_mi_sans_20 FONT_NORMAL
#define ui_font_mi_sans_30 FONT_BIGL
#define ui_font_mi_sans_42 FONT_HUGE
#define ui_font_mi_sans_80 FONT_SUPER

typedef struct {
    uint16_t constellation;
    uint16_t svid;
} sv_id_t;

// 辅助结构体，方便在更新时找到 Label
typedef struct {
    sv_id_t id;
    lv_obj_t *label_azimuth;
    lv_obj_t *label_elevation;
    lv_obj_t *label_constellation;
    lv_obj_t *label_svid;
    lv_obj_t *label_c_n0;
    lv_obj_t *label_flag;
    bool updated;  // 用于标记本轮是否已更新
} sv_bar_ctx_t;

static lv_timer_t *frush_timer = NULL;
static char buf[32];

static void create_ui(void) {
    create_screen_location();

    lv_obj_clear_flag(lv_obj_get_parent(objects.location),
                      LV_OBJ_FLAG_SCROLLABLE);
}

static void create_sv_bar(lv_obj_t *parent_obj, GnssSvInfo *info) {
    lv_obj_t *obj = lv_obj_create(parent_obj);
    lv_obj_set_size(obj, 410, 32);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff006524),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID,
                           LV_PART_MAIN | LV_STATE_DEFAULT);

    sv_bar_ctx_t *ctx = rt_malloc(sizeof(sv_bar_ctx_t));
    ctx->id.constellation = info->constellation;
    ctx->id.svid = info->svid;
    ctx->updated = true;
    lv_obj_set_user_data(obj, ctx);

    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, -17, 0);
            lv_obj_set_size(obj, 61, 32);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 16,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 16,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_column(obj, 10,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_LEFT_MID,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // label_sv_azimuth
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    ctx->label_azimuth = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(
                        obj, lv_color_hex(0xff00ff00),
                        LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_mi_sans_20,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER,
                                           LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "---");
                }
            }
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 60, 0);
            lv_obj_set_size(obj, 61, 32);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 16,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 16,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_column(obj, 10,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_LEFT_MID,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // label_sv_elevation
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    ctx->label_elevation = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(
                        obj, lv_color_hex(0xff00ff00),
                        LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_mi_sans_20,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER,
                                           LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "---");
                }
            }
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 138, 0);
            lv_obj_set_size(obj, 61, 32);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 16,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 16,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_column(obj, 10,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_LEFT_MID,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // label_sv_constellation
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    ctx->label_constellation = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(
                        obj, lv_color_hex(0xff00ff00),
                        LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_mi_sans_20,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER,
                                           LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "----");
                }
            }
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 209, 0);
            lv_obj_set_size(obj, 61, 32);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 16,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 16,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_column(obj, 10,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_LEFT_MID,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // label_sv_svid
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    ctx->label_svid = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(
                        obj, lv_color_hex(0xff00ff00),
                        LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_mi_sans_20,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER,
                                           LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "----");
                }
            }
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 277, 0);
            lv_obj_set_size(obj, 61, 32);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 16,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 16,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_column(obj, 10,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_LEFT_MID,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // label_sv_c_n0_dbhz
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    ctx->label_c_n0 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(
                        obj, lv_color_hex(0xff00ff00),
                        LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_mi_sans_20,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER,
                                           LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "---");
                }
            }
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 353, 0);
            lv_obj_set_size(obj, 41, 32);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 16,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 16,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_column(obj, 10,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_LEFT_MID,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // label_sv_flag
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    ctx->label_flag = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(
                        obj, lv_color_hex(0xff00ff00),
                        LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_mi_sans_20,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER,
                                           LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "--");
                }
            }
        }
    }
}

static void update_sv_bar_data(lv_obj_t *bar_obj, GnssSvInfo *info) {
    sv_bar_ctx_t *ctx = lv_obj_get_user_data(bar_obj);
    ctx->updated = true;  // 标记为活跃

    rt_sprintf(buf, "%.1f", info->azimuth);
    lv_label_set_text(ctx->label_azimuth, buf);
    rt_sprintf(buf, "%.1f", info->elevation);
    lv_label_set_text(ctx->label_elevation, buf);
    const char *constellation_str = "Unknown";
    switch (info->constellation) {
        case 1:
            constellation_str = "GPS";
            break;
        case 2:
            constellation_str = "SBAS";
            break;
        case 3:
            constellation_str = "GLONASS";
            break;
        case 4:
            constellation_str = "QZSS";
            break;
        case 5:
            constellation_str = "BEIDOU";
            break;
        case 6:
            constellation_str = "GALILEO";
            break;
    }
    lv_label_set_text(ctx->label_constellation, constellation_str);
    lv_label_set_text_fmt(ctx->label_svid, "%d", info->svid);
    lv_label_set_text_fmt(ctx->label_c_n0, "%d", (int)info->c_n0_dbhz);
    rt_sprintf(buf, "%02x", (int)info->flags);
    lv_label_set_text(ctx->label_flag, buf);
}

static void sync_sv_bars(lv_obj_t *panel, GpsData *gps_data) {
    // 1. 将所有现有的 Bar 标记为未更新（准备删除）
    uint32_t child_cnt = lv_obj_get_child_cnt(panel);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(panel, i);
        sv_bar_ctx_t *ctx = lv_obj_get_user_data(child);
        if (ctx) ctx->updated = false;
    }

    // 2. 遍历 GPS 数据
    for (int i = 0; i < gps_data->sv_data.num_svs; i++) {
        GnssSvInfo *sv_info = &gps_data->sv_data.gnss_sv_list[i];
        lv_obj_t *target_bar = NULL;

        // 查找是否已有该卫星的 Bar
        for (uint32_t j = 0; j < lv_obj_get_child_cnt(panel); j++) {
            lv_obj_t *child = lv_obj_get_child(panel, j);
            sv_bar_ctx_t *ctx = lv_obj_get_user_data(child);
            if (ctx && ctx->id.constellation == sv_info->constellation &&
                ctx->id.svid == sv_info->svid) {
                target_bar = child;
                break;
            }
        }

        if (target_bar) {
            // 已存在，更新
            update_sv_bar_data(target_bar, sv_info);
        } else {
            // 不存在，新建
            create_sv_bar(panel, sv_info);
        }
    }

    // 3. 删除所有本轮未更新的 Bar
    // 注意：从后往前删，避免索引失效
    int i = lv_obj_get_child_cnt(panel) - 1;
    while (i >= 0) {
        lv_obj_t *child = lv_obj_get_child(panel, i);
        sv_bar_ctx_t *ctx = lv_obj_get_user_data(child);
        if (ctx && !ctx->updated) {
            rt_free(ctx);       // 释放自定义内存
            lv_obj_del(child);  // 删除对象，Flex 布局会自动收缩
        }
        i--;
    }
}

static void timer_callback(lv_timer_t *timer) {
    static double lat, lon, alt;
    static GpsData gps_data;
    if (um_gps_get_data(&lat, &lon, &alt, &gps_data) == 0) {
        sync_sv_bars(objects.panel_sv, &gps_data);

        rt_sprintf(buf, "%.6f", lat);
        lv_label_set_text(objects.label_gps_north, buf);

        rt_sprintf(buf, "%.6f", lon);
        lv_label_set_text(objects.label_gps_east, buf);

        rt_sprintf(buf, "%.1f", alt);
        lv_label_set_text(objects.label_gps_altitude, buf);

        rt_sprintf(buf, "%.2f", gps_data.location_data.speed);
        lv_label_set_text(objects.label_gps_speed, buf);

        rt_sprintf(buf, "%.1f", gps_data.location_data.accuracy);
        lv_label_set_text(objects.label_gps_precision, buf);

        lv_label_set_text_fmt(objects.label_gps_utc_time,
                              "%04d-%02d-%02d %02d:%02d:%02d",
                              gps_data.location_data.timestamp.tm_year,
                              gps_data.location_data.timestamp.tm_mon,
                              gps_data.location_data.timestamp.tm_mday,
                              gps_data.location_data.timestamp.tm_hour,
                              gps_data.location_data.timestamp.tm_min,
                              gps_data.location_data.timestamp.tm_sec);
        lv_label_set_text_fmt(objects.label_gps_satellites_num, "%d",
                              gps_data.sv_data.num_svs);

        switch (gps_data.location_data.flags) {
            case 0:
                lv_label_set_text(objects.label_gps_status, "定位无效");
                break;
            case 1:
                lv_label_set_text(objects.label_gps_status, "SPS模式");
                break;
            case 2:
                lv_label_set_text(objects.label_gps_status, "差分SPS模式");
                break;
            case 3:
                lv_label_set_text(objects.label_gps_status, "PPS模式");
                break;
            case 4:
            case 5:
                lv_label_set_text(objects.label_gps_status, "RTK模式");
                break;
            case 6:
                lv_label_set_text(objects.label_gps_status, "估算模式");
                break;
        }


        // LOG_I(
        //         "=============================================================="
        //         "===================");
        //     LOG_I("Get location North %.6f, East %.6f, high %f, accuracy %f",
        //           lat, lon, alt, gps_data.location_data.accuracy);
        //     LOG_I("speed %f, bearing %f, flags 0x%x",
        //           gps_data.location_data.speed, gps_data.location_data.bearing,
        //           gps_data.location_data.flags);

        //     LOG_I("Time: %04d-%02d-%02d %02d:%02d:%02d",
        //           gps_data.location_data.timestamp.tm_year,
        //           gps_data.location_data.timestamp.tm_mon,
        //           gps_data.location_data.timestamp.tm_mday,
        //           gps_data.location_data.timestamp.tm_hour,
        //           gps_data.location_data.timestamp.tm_min,
        //           gps_data.location_data.timestamp.tm_sec);
        //     LOG_I(
        //         "--------------------------------------------------------------"
        //         "---------------------");

        //     for (int i = 0; i < gps_data.sv_data.num_svs; i++) {
        //         GnssSvInfo sv_info = gps_data.sv_data.gnss_sv_list[i];
        //         LOG_I(
        //             "SV %d: azimuth=%.1f, elevation = %.1f, constellation=%d, "
        //             "svid=%d, c_n0_dbhz=%.5f, Flags=0x%x",
        //             i + 1, sv_info.azimuth, sv_info.elevation,
        //             sv_info.constellation, sv_info.svid, sv_info.c_n0_dbhz,
        //             sv_info.flags);
        //     }
        //     LOG_I(
        //         "=============================================================="
        //         "===================");
    }
}

static void on_start(void) {
    create_ui();

    frush_timer = lv_timer_create(timer_callback, 1000, NULL);
    lv_timer_set_repeat_count(frush_timer, -1);

    lv_img_cache_invalidate_src(NULL);
}

static void on_pause(void) {}

static void on_resume(void) {}

static void on_stop(void) {
    if (frush_timer) {
        lv_timer_del(frush_timer);
        frush_timer = NULL;
    }
}

static void msg_handler(gui_app_msg_type_t msg, void *param) {
    switch (msg) {
        case GUI_APP_MSG_ONSTART:
            on_start();
            break;

        case GUI_APP_MSG_ONRESUME:
            on_resume();
            break;

        case GUI_APP_MSG_ONPAUSE:
            on_pause();
            break;

        case GUI_APP_MSG_ONSTOP:
            on_stop();
            break;
        default:
            break;
    }
}

LV_IMG_DECLARE(img_location);
#define APP_ID "location"
static int app_main(intent_t i) {
    gui_app_regist_msg_handler(APP_ID, msg_handler);

    return 0;
}

BUILTIN_APP_EXPORT(LV_EXT_STR_ID(location), LV_EXT_IMG_GET(img_location),
                   APP_ID, app_main);
