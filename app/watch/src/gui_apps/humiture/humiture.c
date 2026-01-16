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
#include "aht20.h"

static float temp = 0.0, humi = 0.0;
static lv_obj_t* label = NULL;
static lv_timer_t* frush_timer = NULL;

static void timer_callback(lv_timer_t* timer) {
    static char buf[32];
    AHT20_StartMeasure();
    rt_thread_mdelay(AHT20_MEASURE_TIME);
    AHT20_GetMeasureResult(&temp, &humi);
    rt_sprintf(buf, "%.2f  %.2f", temp, humi);
    if (label) lv_label_set_text(label, buf);
}

static void create_ui(void) {
    lv_obj_t* screen = lv_scr_act();

    label = lv_label_create(lv_scr_act());
    lv_obj_center(label);

    frush_timer = lv_timer_create(timer_callback, 1000, NULL);
    lv_timer_set_repeat_count(frush_timer, -1);
}

static void on_start(void) {
    AHT20_Init();
    AHT20_Calibrate();

    create_ui();

    lv_img_cache_invalidate_src(NULL);
}

static void on_pause(void) {
    if (frush_timer) {
        lv_timer_del(frush_timer);
        frush_timer = NULL;
    }
}

static void on_resume(void) {
    if (frush_timer == NULL) {
        frush_timer = lv_timer_create(timer_callback, 1000, NULL);
        lv_timer_set_repeat_count(frush_timer, -1);
    }
}

static void on_stop(void) {
    if (frush_timer) {
        lv_timer_del(frush_timer);
        frush_timer = NULL;
    }
}

static void msg_handler(gui_app_msg_type_t msg, void* param) {
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

LV_IMG_DECLARE(img_humiture);
#define APP_ID "humiture"
static int app_main(intent_t i) {
    gui_app_regist_msg_handler(APP_ID, msg_handler);

    return 0;
}

BUILTIN_APP_EXPORT(LV_EXT_STR_ID(humiture), LV_EXT_IMG_GET(img_humiture),
                   APP_ID, app_main);
