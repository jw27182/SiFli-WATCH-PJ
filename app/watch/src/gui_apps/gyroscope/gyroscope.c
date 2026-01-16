/*********************
 *      INCLUDES
 *********************/

#include <math.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <stdbool.h>
#include <stdlib.h>

#include "app_mem.h"
#include "gui_app_fwk.h"
#include "littlevgl2rtt.h"
#include "lv_ex_data.h"
#include "lv_ext_resource_manager.h"
#include "lvgl.h"
#include "lvsf_comp.h"
#include "sensor.h"

#define ALPHA 0.3
#define FRUSH_FREQ 10
#define PROPORTION 0.6
#define ANGLE_COMPENSATE 0.9
#define LIMIT_OFFSET_X (205 - 20)
#define LIMIT_OFFSET_Y (251 - 20)

#define OFFSET_THRESHOLD 10
#define OFFSET_TIMEOUT_MS 500
#define OFFSET_TIMEOUT_TICKS ((OFFSET_TIMEOUT_MS + FRUSH_FREQ - 1) / FRUSH_FREQ)

static lv_obj_t* static_ring = NULL, *ring_outside = NULL;
static lv_obj_t* label = NULL;
static lv_timer_t* frush_timer = NULL;
static rt_device_t acce_sensor_dev = RT_NULL;
static rt_int32_t x_before = 0, y_before = 0;
static int over_threshold_count = 0;
static bool ring_is_green = false;

static int calculate_plane_tilt_angle(int angle_x_deg, int angle_y_deg) {
    float angle_x_rad = angle_x_deg * 3.14 / 1800.0;
    float angle_y_rad = angle_y_deg * 3.14 / 1800.0;

    float cos_product = cos(angle_x_rad) * cos(angle_y_rad);
    if (cos_product > 1.0) cos_product = 1.0;
    if (cos_product < -1.0) cos_product = -1.0;

    float tilt_angle_rad = acos(cos_product);
    int tilt_angle_deg = tilt_angle_rad * 180.0 / 3.14;

    return tilt_angle_deg;
}

static void timer_callback(lv_timer_t* timer) {
    static struct rt_sensor_data sensor_data;
    static char buf[32];

    if (acce_sensor_dev == RT_NULL) {
        return;
    }

    rt_device_read(acce_sensor_dev, 0, &sensor_data, 1);
    if (static_ring) {
        int x_offset =
            (float)sensor_data.data.acce.y / 2 * ALPHA + x_before * (1 - ALPHA);
        int y_offset =
            (float)sensor_data.data.acce.x / 2 * ALPHA + y_before * (1 - ALPHA);
        x_offset *= PROPORTION;
        y_offset *= PROPORTION;
        if (x_offset > LIMIT_OFFSET_X) x_offset = LIMIT_OFFSET_X;
        if (x_offset < -LIMIT_OFFSET_X) x_offset = -LIMIT_OFFSET_X;
        if (y_offset > LIMIT_OFFSET_Y) y_offset = LIMIT_OFFSET_Y;
        if (y_offset < -LIMIT_OFFSET_Y) y_offset = -LIMIT_OFFSET_Y;
        lv_obj_align(static_ring, LV_ALIGN_CENTER, x_offset, y_offset);
        int abs_x = x_offset < 0 ? -x_offset : x_offset;
        int abs_y = y_offset < 0 ? -y_offset : y_offset;
        if (abs_x < OFFSET_THRESHOLD && abs_y < OFFSET_THRESHOLD) {
            over_threshold_count++;
            if (!ring_is_green &&
                over_threshold_count >= OFFSET_TIMEOUT_TICKS) {
                lv_obj_set_style_arc_color(static_ring, lv_color_hex(0x00ff00),
                                           LV_PART_MAIN);
                lv_obj_set_style_arc_width(ring_outside, 5, LV_PART_MAIN);
                ring_is_green = true;
            }
        } else {
            over_threshold_count = 0;
            if (ring_is_green) {
                lv_obj_set_style_arc_color(static_ring, lv_color_hex(0xffffff),
                                           LV_PART_MAIN);
                lv_obj_set_style_arc_width(ring_outside, 2, LV_PART_MAIN);
                ring_is_green = false;
            }
        }

        x_before = x_offset;
        y_before = y_offset;
    }
    rt_sprintf(
        buf, "%d degree",
        (int)(ANGLE_COMPENSATE * calculate_plane_tilt_angle(sensor_data.data.acce.y,
                                                sensor_data.data.acce.x)));
    if (label) lv_label_set_text(label, buf);
}

static void create_ui(void) {
    static_ring = lv_arc_create(lv_scr_act());
    lv_obj_set_size(static_ring, 40, 40);
    lv_arc_set_bg_angles(static_ring, 0, 360);
    lv_obj_set_style_arc_width(static_ring, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(static_ring, lv_color_hex(0xffffff),
                               LV_PART_MAIN);
    lv_obj_remove_style(static_ring, NULL, LV_PART_INDICATOR);
    lv_obj_remove_style(static_ring, NULL, LV_PART_KNOB);
    lv_obj_center(static_ring);

    ring_outside = lv_arc_create(lv_scr_act());
    lv_obj_set_size(ring_outside, 60, 60);
    lv_arc_set_bg_angles(ring_outside, 0, 360);
    lv_obj_set_style_arc_width(ring_outside, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring_outside, lv_color_hex(0xffffff),
                               LV_PART_MAIN);
    lv_obj_remove_style(ring_outside, NULL, LV_PART_INDICATOR);
    lv_obj_remove_style(ring_outside, NULL, LV_PART_KNOB);
    lv_obj_center(ring_outside);

    label = lv_label_create(lv_scr_act());
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -60);

    frush_timer = lv_timer_create(timer_callback, FRUSH_FREQ, NULL);
    lv_timer_set_repeat_count(frush_timer, -1);
}

static void on_start(void) {
    if (acce_sensor_dev == RT_NULL) {
        acce_sensor_dev = rt_device_find("acce_lsm6dsl");
        if (acce_sensor_dev == RT_NULL) {
            rt_kprintf("find lsm6dsl sensor device failed!\n");
            return;
        }
    }

    rt_err_t ret = rt_device_open(acce_sensor_dev, RT_DEVICE_FLAG_RDONLY);
    if (ret != RT_EOK) {
        rt_kprintf("open acce_sensor_dev failed! err: %d\n", ret);
        return;
    }

    create_ui();

    lv_img_cache_invalidate_src(NULL);
}

static void on_pause(void) {
    if (frush_timer) {
        lv_timer_del(frush_timer);
        frush_timer = NULL;
    }
    rt_device_close(acce_sensor_dev);
}

static void on_resume(void) {
    if (frush_timer == NULL) {
        frush_timer = lv_timer_create(timer_callback, FRUSH_FREQ, NULL);
        lv_timer_set_repeat_count(frush_timer, -1);
    }
    rt_device_open(acce_sensor_dev, RT_DEVICE_FLAG_RDONLY);
}

static void on_stop(void) {
    if (frush_timer) {
        lv_timer_del(frush_timer);
        frush_timer = NULL;
    }
    rt_device_close(acce_sensor_dev);
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

LV_IMG_DECLARE(img_gradienter_line);
#define APP_ID "gyroscope"
static int app_main(intent_t i) {
    gui_app_regist_msg_handler(APP_ID, msg_handler);

    return 0;
}

BUILTIN_APP_EXPORT(LV_EXT_STR_ID(gyroscope),
                   LV_EXT_IMG_GET(img_gradienter_line), APP_ID, app_main);
