#ifndef __TIME_MANAGER_H__
#define __TIME_MANAGER_H__

#include "stdio.h"
#include "string.h"
#include "time.h"
#include <rtdevice.h>

/**
 * @brief Convert local time to {dm_date_time_t}
 */
#define LOCAL_TIME_2_DATE_TIME_T(DT, TM)      \
    (DT)->year = (TM)->tm_year + 1900;        \
    (DT)->month = (TM)->tm_mon + 1;           \
    (DT)->day = (TM)->tm_mday;                \
    (DT)->hour = (TM)->tm_hour;               \
    (DT)->minute = (TM)->tm_min;              \
    (DT)->second = (TM)->tm_sec;              \

/* struct date_time */
typedef struct date_time
{
    int year;     /* eg: 2024 */
    int month;    /* eg:  1 (1-12) */
    int day;      /* eg: 31 (1~xx) */
    int hour;     /* eg:  8 (0~23) */
    int minute;   /* eg: 30 (0-59) */
    int second;   /* eg:  0 (0~59) */
} dm_date_time_t;

typedef void (*alarm_callback_t)(rt_alarm_t alarm, time_t timestamp);

rt_err_t dm_set_date_time(dm_date_time_t dt);
dm_date_time_t *dm_get_date_time(dm_date_time_t *dt);
void dm_set_alarm(int hour, int min, int sec, rt_alarm_callback_t cb);
void dm_print_time(const char *tag, dm_date_time_t dt);

#endif