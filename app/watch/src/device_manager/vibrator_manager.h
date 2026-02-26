#ifndef __VIBRATOR_MANAGER_H__
#define __VIBRATOR_MANAGER_H__

#include <rtdevice.h>

int vibrator_send(rt_uint32_t duration, rt_uint32_t intensity);

#endif