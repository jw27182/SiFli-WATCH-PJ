/**
 ******************************************************************************
 * @file   um_gps_if.c
 * @author Sifli software development team
 ******************************************************************************
 */
/**
 * @attention
 * Copyright (c) 2019 - 2022,  Sifli Technology
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Sifli integrated
 * circuit in a product or a software update for such product, must reproduce
 * the above copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3. Neither the name of Sifli nor the names of its contributors may be used to
 * endorse or promote products derived from this software without specific prior
 * written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Sifli integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be
 * reverse engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY SIFLI TECHNOLOGY "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SIFLI TECHNOLOGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "um_gps_if.h"

#include <math.h>
#include <rtthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "um_gps_hal.h"
#include "um_gps_nmea.h"

// #define DRV_DEBUG
#define LOG_TAG "drv.gps"
#include <drv_log.h>

static char *g_cmd = NULL;
GpsLocation g_location_data;
GnssSvStatus g_sv_data;
GpsData g_gps_data;
GpsCallbacks g_gps_cb;

static int um_gps_power_set(rt_bool_t enable) {
#ifndef GPS_POWER_BIT
    return -1;
#endif  // GPS_POWER_BIT
    struct rt_device_pin_status st = {};
    struct rt_device_pin_mode m = {};
    rt_device_t device = rt_device_find("pin");
    if (!device) {
        LOG_E("GPIO pin device not found\n");
        return -1;
    }

    rt_device_open(device, RT_DEVICE_OFLAG_RDWR);
    m.pin = GPS_POWER_BIT;
    m.mode = PIN_MODE_OUTPUT;
    rt_device_control(device, 0, &m);

    if (enable) {
        st.pin = GPS_POWER_BIT;
        st.status = 1;
        rt_device_write(device, 0, &st, sizeof(struct rt_device_pin_status));
    } else {
        st.pin = GPS_POWER_BIT;
        st.status = 1;
        rt_device_write(device, 0, &st, sizeof(struct rt_device_pin_status));
    }

    rt_device_close(device);

    LOG_D("Set gps power %s\n", enable ? "ON" : "OFF");

    return 0;
}

int um_gps_start(GpsCallbacks *callbacks) {
    LOG_I("enter 0 %s", __FUNCTION__);

    int ret = 0;

    if (g_cmd != NULL)  // g_cmd alloced, it opened before?
        return -1;

    um_gps_power_set(true);

    if (callbacks)
        ret = gps_hal_start(callbacks);
    else
        LOG_E("callbacks is NULL");

    gps_status_update(GPS_HAL_STATUS_ON);

    g_cmd = (char *)malloc(GPS_CMD_MAXLEN);
    if (g_cmd == NULL) ret = -2;

    LOG_I("leave %s, %d", __FUNCTION__, ret);

    return ret;
}

int um_gps_stop() {
    int ret = 0;
    um_gps_power_set(false);
    ret = gps_hal_stop();
    gps_status_update(GPS_HAL_STATUS_OFF);

    if (g_cmd) {
        free(g_cmd);
        g_cmd = NULL;
    }

    LOG_I("%s", __FUNCTION__);

    return ret;
}

uint8_t bd_check_eor(char *buf, int len) {
    uint8_t csum = 0;

    while (len--) csum ^= buf[len];

    return csum;
}

int um_gps_set_freq(uint16_t freq) {
    int ret = -1;
    uint8_t sum;
    uint8_t length = 0;

    sprintf(g_cmd, "$PCAS02,%d", 1000 / freq);
    length = strlen(g_cmd);
    sum = bd_check_eor(g_cmd + 1, length - 1);
    sprintf(g_cmd + length, "*%02X\r\n", sum);
    ret = gps_send_cmd(MSG_SET_INTERVAL, g_cmd, strlen(g_cmd));
    if (ret == 0) {
        sprintf(g_cmd, "$CFGSAVE\r\n");
        ret = gps_send_cmd(MSG_SAVE_CFG, g_cmd, strlen(g_cmd));
        if (ret != 0) LOG_I("Set g_cmd MSG_SAVE_CFG fail with %d\n", ret);
    } else
        LOG_I("Set g_cmd MSG_SET_INTERVAL fail with %d\n", ret);

    return ret;
}

int um_gps_get_freq() {
    int ret = -1;
    uint8_t sum;
    uint8_t length = 0;

    sprintf(g_cmd, "$CFGNAV\r\n");
    ret = gps_send_cmd(MSG_SET_INTERVAL, g_cmd, strlen(g_cmd));
    if (ret != 0) LOG_I("SET command MSG_SET_INTERVAL fail %d\n", ret);

    return ret;
}

int um_gps_set_nmea_version(uint8_t version) {
    int ret = -1;
    uint8_t sum;
    uint8_t length = 0;

    sprintf(g_cmd, "$CFGNMEA,h%02X", version);
    length = strlen(g_cmd);
    sum = bd_check_eor(g_cmd + 1, length - 1);
    sprintf(g_cmd + length, "*%02X\r\n", sum);
    ret = gps_send_cmd(MSG_NMEA_VER, g_cmd, strlen(g_cmd));
    if (ret == 0) {
        sprintf(g_cmd, "$CFGSAVE\r\n");
        ret = gps_send_cmd(MSG_SAVE_CFG, g_cmd, strlen(g_cmd));
        if (ret != 0) LOG_I("Set g_cmd MSG_SAVE_CFG fail with %d\n", ret);
    } else
        LOG_I("Set g_cmd MSG_NMEA_VER fail with %d\n", ret);

    return ret;
}

int um_gps_set_position_mode(uint32_t mode) {
    int ret = -1;
    uint8_t sum;
    uint8_t length = 0;

    // BD: $CFGSYS,h10*5E
    // GN: $CFGSYS,h11*5F
    if (mode) {
        sprintf(g_cmd, "$CFGSYS,h%02X", mode);
        length = strlen(g_cmd);
        sum = bd_check_eor(g_cmd + 1, length - 1);
        sprintf(g_cmd + length, "*%02X\r\n", sum);
    } else {
        sprintf(g_cmd, "$CFGSYS\r\n");
    }

    ret = gps_send_cmd(MSG_SET_POS_MODE, g_cmd, strlen(g_cmd));
    if (ret == 0) {
        sprintf(g_cmd, "$CFGSAVE\r\n");
        ret = gps_send_cmd(MSG_SAVE_CFG, g_cmd, strlen(g_cmd));
        if (ret != 0) LOG_I("Set g_cmd MSG_SAVE_CFG fail with %d\n", ret);
    } else
        LOG_I("Set g_cmd MSG_SET_POS_MODE fail with %d\n", ret);

    return ret;
}

int um_gps_set_start_mode(uint16_t mode) {
    int ret = -1;
    uint8_t sum;
    uint8_t length = 0;
    uint8_t bits = START_MODE_HOT;  // default

    if (mode == START_MODE_COLD) {
        bits |= 0xFF;
    } else if (mode == START_MODE_WARM) {
        bits |= 0x01;
    }

    memset(&g_cmd, 0, sizeof(g_cmd));
    sprintf(g_cmd, "$RESET,0,h%02X", bits);
    length = strlen(g_cmd);
    sum = bd_check_eor(g_cmd + 1, length - 1);
    sprintf(g_cmd + length, "*%02X\r\n", sum);
    ret = gps_send_cmd(MSG_DELETE_AIDING, g_cmd, strlen(g_cmd));
    if (ret != 0) LOG_I("Set g_cmd MSG_DELETE_AIDING fail with %d\n", ret);

    return ret;
}

int um_gps_req_assist(uint8_t type) {
    int ret = -1;

    sprintf(g_cmd, "ASSIST");
    ret = gps_send_cmd(MSG_REQ_ASSIST, g_cmd, strlen(g_cmd));
    if (ret != 0) LOG_I("Set g_cmd MSG_REQ_ASSIST fail with %d\n", ret);

    return ret;
}

int um_gps_fw_info() {
    int ret = -1;

    sprintf(g_cmd, "$PDTINFO\r\n");
    ret = gps_send_cmd(MSG_GET_FWINFO, g_cmd, strlen(g_cmd));
    if (ret != 0) LOG_I("Set g_cmd MSG_GET_FWINFO fail with %d\n", ret);

    return ret;
}

int um_gps_set_baud(uint32_t baud) {
    int ret = -1;
    uint8_t sum;
    uint8_t len = 0;

    sprintf(g_cmd, "$CFGPRT,,0,%d,129,3", baud);
    len = strlen(g_cmd);
    sum = bd_check_eor(g_cmd + 1, len - 1);
    sprintf(g_cmd + len, "*%02X\r\n", sum);
    ret = gps_send_cmd(MSG_SET_BAUD, g_cmd, strlen(g_cmd));
    if (ret != 0) {
        return ret;
    }
    // add a delay here to make sure new baud work
    rt_thread_delay(10);
    sprintf(g_cmd, "$CFGSAVE\r\n");
    ret = gps_send_cmd(MSG_SAVE_CFG, g_cmd, strlen(g_cmd));
    if (ret == 0) {
        // make sure update local serial baud to new value !!!!
        gps_hal_set_local_baud(baud);
    } else
        LOG_I("Set g_cmd MSG_SAVE_CFG fail with %d\n", ret);

    return ret;
}

int um_gps_set_nmea_output(uint8_t type, uint8_t flag, uint8_t freq) {
    int ret = -1;
    uint8_t sum;
    uint8_t length = 0;

    if (flag >= 8) {
        sprintf(g_cmd, "$CFGMSG,%d,,%d", type, freq);
    } else {
        sprintf(g_cmd, "$CFGMSG,%d,%d,%d", type, flag, freq);
    }
    length = strlen(g_cmd);
    sum = bd_check_eor(g_cmd + 1, length - 1);
    sprintf(g_cmd + length, "*%02X\r\n", sum);

    ret = gps_send_cmd(MSG_NMEA_OUTPUT, g_cmd, strlen(g_cmd));

    if (ret != 0) {
        LOG_I("Set g_cmd MSG_NMEA_OUTPUT fail with %d\n", ret);
        return ret;
    }

    sprintf(g_cmd, "$CFGSAVE\r\n");
    ret = gps_send_cmd(MSG_SAVE_CFG, g_cmd, strlen(g_cmd));
    if (ret != 0) LOG_I("Set g_cmd MSG_SAVE_CFG fail with %d\n", ret);

    return ret;
}

static void gps_loc_cb(GpsLocation *param) {
    if (param != NULL) {
        // LOG_I("loc: longit = %.6f, latitude=%.6f, altitude=%f, speed = %f\n",
        //       param->longitude, param->latitude, param->altitude,
        //       param->speed);
        memcpy(&g_location_data, param, sizeof(GpsLocation));
        g_gps_data.location_data = g_location_data;
    }
}

static void gps_sta_cb(GpsStatus *param) {
    // if(param != NULL)
    //    LOG_I("sta_cb: size = %d, status = 0x%x\n",param->size,param->status);
}

static void gps_gnss_sv_cb(GnssSvStatus *param) {
    if (param != NULL) {
        memcpy(&g_sv_data, param, sizeof(GnssSvStatus));
        g_gps_data.sv_data = g_sv_data;
    }
}

static void gps_nmea_cb(GpsUtcTime timestamp, const char *nmea, int length) {
    // if (nmea != NULL)
    // LOG_I("nmea_cb: %s, timestamp = %d, len=%d\n", nmea, timestamp, length);
}

int um_gps_open(void) {
    int res = 0;
    res = um_gps_start(&g_gps_cb);

    // res = um_gps_set_freq(GPS_OUTPUT_FREQ);

    return res;
}

int um_gps_close(void) { return um_gps_stop(); }

int um_gps_init(void) {
    int res = 0;

    setenv("TZ", "Asia/Shanghai", 1);
    tzset();

    g_gps_cb.gnss_sv_status_cb = gps_gnss_sv_cb;
    g_gps_cb.location_cb = gps_loc_cb;
    g_gps_cb.nmea_cb = gps_nmea_cb;
    g_gps_cb.status_cb = gps_sta_cb;
    g_gps_cb.size = sizeof(GpsCallbacks);
    return res;
}

int um_gps_get_data(double *lati, double *longiti, double *alti, void *detail) {
    // if(g_location_data.size == 0)
    //    return 1;

    if (lati) *lati = g_location_data.latitude;
    if (longiti) *longiti = g_location_data.longitude;
    if(alti) *alti = g_location_data.altitude;
    if (detail) memcpy(detail, &g_gps_data, sizeof(GpsData));

    return 0;
}

static int gps_init(void) {
    um_gps_init();
    return um_gps_open();
}

INIT_COMPONENT_EXPORT(gps_init);

#define GPS_CMD_TEST
#ifdef GPS_CMD_TEST

#include <time.h>

int cmd_gps(int argc, char *argv[]) {
    double lat, lon, alt;
    GpsData gps_data;
    if (argc < 2) {
        LOG_I("Invalid parameter\n");
        return 0;
    }

    if (strcmp(argv[1], "-ver") == 0) {
        um_gps_fw_info();
    } else if (strcmp(argv[1], "-frq") == 0) {
        um_gps_get_freq();
    } else if (strcmp(argv[1], "-bd") == 0) {
        int res;
        int value = atoi(argv[2]);
        res = um_gps_set_baud(value);
        if (res == 0)
            LOG_I("Set baud to %d done\n", value);
        else
            LOG_I("Set baud to %d fail\n", value);
    } else {
        LOG_I("Invalid parameter\n");
    }
    return 0;
}

static void gps_thread_entry(void *param) {
    double lat, lon, alt;
    GpsData gps_data;
    while (1) {
        if (um_gps_get_data(&lat, &lon, &alt, &gps_data) == 0) {
            LOG_I(
                "=============================================================="
                "===================");
            LOG_I("Get location North %.6f, East %.6f, high %f, accuracy %f",
                  lat, lon, alt, gps_data.location_data.accuracy);
            LOG_I("speed %f, bearing %f, flags 0x%x",
                  gps_data.location_data.speed, gps_data.location_data.bearing,
                  gps_data.location_data.flags);

            LOG_I("Time: %04d-%02d-%02d %02d:%02d:%02d",
                  gps_data.location_data.timestamp.tm_year,
                  gps_data.location_data.timestamp.tm_mon,
                  gps_data.location_data.timestamp.tm_mday,
                  gps_data.location_data.timestamp.tm_hour,
                  gps_data.location_data.timestamp.tm_min,
                  gps_data.location_data.timestamp.tm_sec);
            LOG_I(
                "--------------------------------------------------------------"
                "---------------------");

            for (int i = 0; i < gps_data.sv_data.num_svs; i++) {
                GnssSvInfo sv_info = gps_data.sv_data.gnss_sv_list[i];
                LOG_I(
                    "SV %d: azimuth=%.1f, elevation = %.1f, constellation=%d, "
                    "svid=%d, c_n0_dbhz=%.5f, Flags=0x%x",
                    i + 1, sv_info.azimuth, sv_info.elevation,
                    sv_info.constellation, sv_info.svid, sv_info.c_n0_dbhz,
                    sv_info.flags);
            }
            LOG_I(
                "=============================================================="
                "===================");
        } else
            LOG_I("Get location fail\n");
        rt_thread_mdelay(1000);
    }
}

static void gps_test_thread() {
    rt_thread_t gps_thread = rt_thread_create(
        "gps_test", gps_thread_entry, NULL, 40960, RT_MAIN_THREAD_PRIORITY, 10);
    if (gps_thread != NULL) rt_thread_startup(gps_thread);
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_gps, __cmd_gps, Test hw gps);
MSH_CMD_EXPORT(gps_test_thread, Start gps test thread);

#endif  // GPS_CMD_TEST
/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
