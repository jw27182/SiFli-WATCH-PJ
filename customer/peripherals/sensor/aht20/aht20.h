#ifndef __AHT20_H
#define __AHT20_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AHT20_MEASURE_TIME 75 

/*
 *  extern interface
 */
uint32_t AHT20_Calibrate(void);
uint32_t AHT20_StartMeasure(void);
uint32_t AHT20_GetMeasureResult(float *temp, float *humi);
uint8_t AHT20_Init();

#ifdef __cplusplus
}
#endif

#endif /* __AHT20_H */

/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
