/*********************
 *      INCLUDES
 *********************/

#include <rtdevice.h>
#include <rtthread.h>
#include "string.h"

#include "app_mem.h"
#include "gui_app_fwk.h"
#include "littlevgl2rtt.h"
#include "lv_ex_data.h"
#include "lv_ext_resource_manager.h"
#include "lvgl.h"
#include "lvsf_comp.h"

#define TIME_SCALE 1000  // ms
#define IMG_CORONA_SECOND img_corona_second_240x240
#define IMG_CORONA_MINUTE img_corona_minute_240x240

LV_IMG_DECLARE(IMG_CORONA_MINUTE);
LV_IMG_DECLARE(IMG_CORONA_SECOND);
LV_IMG_DECLARE(img_frame);

static lv_obj_t *obj_img_corona_second, *obj_img_corona_minute, *obj_img_frame, *obj_label_hour, *obj_label_minute;
static lv_img_dsc_t *img_corona_second_cache = NULL,
                    *img_corona_minute_cache = NULL;
static lv_timer_t *frush_timer = NULL;
static lv_anim_t anim_second;
static rt_timer_t my_timer = RT_NULL;
static uint8_t current_time_h, current_time_m, current_time_s;
static int32_t correct_second_value;
static char buf[8];
static bool correct_time_second = false;

static void anim_second_cb(void *obj, int32_t angle) {
    if (correct_time_second) {
        correct_second_value = current_time_s * 60 - angle;
        correct_time_second = false;
    }
    lv_img_set_angle((lv_obj_t *)obj, (angle + correct_second_value + 3600) % 3600);
}

void frush_time(bool frush_second) {
    rt_sprintf(buf, "%02d", current_time_h);
    lv_label_set_text(obj_label_hour, buf);
    rt_sprintf(buf, "%02d", current_time_m);
    lv_label_set_text(obj_label_minute, buf);
    if(frush_second)
        correct_time_second = true;
}

static void frush_timer_cb(struct _lv_timer_t *t) {
    static uint8_t second_counter = 0;
    second_counter++;

    lv_img_set_angle(obj_img_corona_minute,
                     (current_time_s + current_time_m * 60) % 3600);
    if (second_counter >= 60) {
        second_counter = 0;
        frush_time(false);
    }
}

static void setup_ui() {
    lv_obj_t *screen = lv_scr_act();

    obj_img_corona_second = lv_img_create(screen);
    lv_img_set_src(obj_img_corona_second, LV_EXT_IMG_GET(IMG_CORONA_SECOND));
    lv_img_set_zoom(obj_img_corona_second, 1.66 * 256);
    lv_obj_center(obj_img_corona_second);

    if (NULL == img_corona_second_cache) {
        img_corona_second_cache =
            app_cache_copy_alloc(LV_EXT_IMG_GET(IMG_CORONA_SECOND), ROTATE_MEM);
        rt_kprintf("used cache :%p data:%p %d\r\n", img_corona_second_cache,
                   img_corona_second_cache->data, ROTATE_MEM);
        lv_img_set_src(obj_img_corona_second, img_corona_second_cache);
    }

    obj_img_corona_minute = lv_img_create(screen);
    lv_img_set_src(obj_img_corona_minute, LV_EXT_IMG_GET(IMG_CORONA_MINUTE));
    lv_img_set_zoom(obj_img_corona_minute, 1.2 * 256);
    lv_obj_center(obj_img_corona_minute);

    if (NULL == img_corona_minute_cache) {
        img_corona_minute_cache =
            app_cache_copy_alloc(LV_EXT_IMG_GET(IMG_CORONA_MINUTE), ROTATE_MEM);
        // rt_kprintf("used cache :%p data:%p %d\r\n", img_corona_minute_cache,
        //            img_corona_minute_cache->data, ROTATE_MEM);
        lv_img_set_src(obj_img_corona_minute, img_corona_minute_cache);
    }

    obj_img_frame = lv_img_create(screen);
    lv_img_set_src(obj_img_frame, LV_EXT_IMG_GET(img_frame));
    lv_obj_set_pos(obj_img_frame, 262, 211);

    obj_label_hour = lv_label_create(screen);
    lv_obj_set_style_text_font(obj_label_hour, LV_FONT_DEFAULT, 0);
    lv_ext_set_local_font(obj_label_hour, 80, lv_color_hex(0xffffff));
    lv_obj_center(obj_label_hour);

    obj_label_minute = lv_label_create(screen);
    lv_obj_set_style_text_font(obj_label_minute, &lv_font_montserrat_42, 0);
    lv_obj_align(obj_label_minute, LV_ALIGN_CENTER, 100, 0);

    lv_anim_init(&anim_second);
    lv_anim_set_var(&anim_second, obj_img_corona_second);
    lv_anim_set_exec_cb(&anim_second, anim_second_cb);
    lv_anim_set_values(&anim_second, 0, 3600);
    lv_anim_set_time(&anim_second, 60000);
    lv_anim_set_repeat_count(&anim_second, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&anim_second, lv_anim_path_linear);
    lv_anim_start(&anim_second);

    // lv_anim_t anim_minute;
    // lv_anim_init(&anim_minute);
    // lv_anim_set_var(&anim_minute, obj_img_corona_minute);
    // lv_anim_set_exec_cb(&anim_minute, (lv_anim_exec_xcb_t)lv_img_set_angle);
    // lv_anim_set_values(&anim_minute, 0, 3600);
    // lv_anim_set_time(&anim_minute, 60000000);
    // lv_anim_set_repeat_count(&anim_minute, LV_ANIM_REPEAT_INFINITE);
    // lv_anim_set_path_cb(&anim_minute, lv_anim_path_linear);
    // lv_anim_start(&anim_minute);
    frush_timer = lv_timer_create(frush_timer_cb, TIME_SCALE, NULL);
    lv_timer_set_repeat_count(frush_timer, -1);
    frush_time(true);
}

static void on_start(void) {
    setup_ui();
    lv_img_cache_invalidate_src(NULL);
}

static void on_pause(void) {
    if (img_corona_second_cache != NULL) {
        lv_img_set_src(obj_img_corona_second,
                       LV_EXT_IMG_GET(IMG_CORONA_SECOND));
        app_cache_copy_free(img_corona_second_cache);
        img_corona_second_cache = NULL;
    }
    if (img_corona_minute_cache != NULL) {
        lv_img_set_src(obj_img_corona_minute,
                       LV_EXT_IMG_GET(IMG_CORONA_MINUTE));
        app_cache_copy_free(img_corona_minute_cache);
        img_corona_minute_cache = NULL;
    }
    // if (frush_timer) {
    //     lv_timer_del(frush_timer);
    //     frush_timer = NULL;
    // }
}

static void on_resume(void) {
    if (NULL == img_corona_second_cache) {
        img_corona_second_cache =
            app_cache_copy_alloc(LV_EXT_IMG_GET(IMG_CORONA_SECOND), ROTATE_MEM);
        lv_img_set_src(obj_img_corona_second, img_corona_second_cache);
    }
    if (NULL == img_corona_minute_cache) {
        img_corona_minute_cache =
            app_cache_copy_alloc(LV_EXT_IMG_GET(IMG_CORONA_MINUTE), ROTATE_MEM);
        lv_img_set_src(obj_img_corona_minute, img_corona_minute_cache);
    }
    frush_time(true);
    // if (frush_timer == NULL) {
    //     frush_timer = lv_timer_create(timer_callback, TIME_SCALE, NULL);
    //     lv_timer_set_repeat_count(frush_timer, -1);
    // }
}

static void on_stop(void) {
    if (img_corona_second_cache != NULL) {
        lv_img_set_src(obj_img_corona_second,
                       LV_EXT_IMG_GET(IMG_CORONA_SECOND));
        app_cache_copy_free(img_corona_second_cache);
        img_corona_second_cache = NULL;
    }
    if (img_corona_minute_cache != NULL) {
        lv_img_set_src(obj_img_corona_minute,
                       LV_EXT_IMG_GET(IMG_CORONA_MINUTE));
        app_cache_copy_free(img_corona_minute_cache);
        img_corona_minute_cache = NULL;
    }
    // if (frush_timer) {
    //     lv_timer_del(frush_timer);
    //     frush_timer = NULL;
    // }
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

LV_IMG_DECLARE(img_clock);
#define APP_ID "clock"
static int app_main(intent_t i) {
    gui_app_regist_msg_handler(APP_ID, msg_handler);

    return 0;
}

BUILTIN_APP_EXPORT(LV_EXT_STR_ID(clock), LV_EXT_IMG_GET(img_clock), APP_ID,
                   app_main);

static void timer_callback(void *parameter) {
    current_time_s++;
    if (current_time_s >= 60) {
        current_time_s = 0;
        current_time_m++;
        if (current_time_m >= 60) {
            current_time_m = 0;
            current_time_h++;
            if (current_time_h >= 24) {
                current_time_h = 0;
            }
        }
    }
    // // lv_img_set_angle(obj_img_corona_second,
    // //                  (current_time_s * 1000 + current_time_ms) * 3600 /
    // 60000); lv_img_set_angle(
    //     obj_img_corona_minute,
    //     (current_time_m * 60000 + current_time_s * 1000 + current_time_ms) *
    //         3600 / 3600000);
    // rt_kprintf("current h %d, m %d, s %d\r\n", current_time_h, current_time_m,
    //            current_time_s);
}

static int timer_init(void) {
    my_timer = rt_timer_create("my_timer",         /* 定时器名称 */
                               timer_callback,     /* 回调函数 */
                               NULL,               /* 回调函数参数 */
                               RT_TICK_PER_SECOND, /* 定时周期：1秒 */
                               RT_TIMER_FLAG_PERIODIC); /* 周期性定时器 */

    if (my_timer == RT_NULL) {
        rt_kprintf("定时器创建失败！\n");
        return -RT_ERROR;
    }

    if (rt_timer_start(my_timer) != RT_EOK) {
        rt_kprintf("定时器启动失败！\n");
        rt_timer_delete(my_timer);
        return -RT_ERROR;
    }

    rt_kprintf("定时器创建并启动成功！\n");
    return RT_EOK;
}

INIT_APP_EXPORT(timer_init);

void clock_set_time(uint8_t h, uint8_t m, uint8_t s) {
    current_time_h = h;
    current_time_m = m;
    current_time_s = s;
    frush_time(true);
}

int cmd_set_time(int argc, char *argv[]) {
    if (argc != 4) {
        rt_kprintf("Usage: cmd_set_time <hour> <minute> <second>\n");
        return -RT_ERROR;
    }
    uint8_t h = atoi(argv[1]);
    uint8_t m = atoi(argv[2]);
    uint8_t s = atoi(argv[3]);
    clock_set_time(h, m, s);
}

MSH_CMD_EXPORT(cmd_set_time, Set clock time: cmd_set_time <hour> <minute> <second>);
