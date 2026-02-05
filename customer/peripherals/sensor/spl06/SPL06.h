/**
  ******************************************************************************
  * @file   SPL06.h
  * @author Sifli software development team
  * @brief   Header file for SPL06.c module.
  *
  ******************************************************************************
*/
/**
 * @attention
 * Copyright (c) 2019 - 2022,  Sifli Technology
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Sifli integrated circuit
 *    in a product or a software update for such product, must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Sifli nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Sifli integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY SIFLI TECHNOLOGY "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SIFLI TECHNOLOGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __SPL06_H
#define __SPL06_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define SPL06_RET_OK   0
#define SPL06_RET_NG   1

/*
 *  SPL06 I2C address
 */
#define SPL06_ADDR      0x76

/*
 *  SPL06 chip id
 */
#define SPL06_CHIP_ID          (0x10)

/*
 *  SPL06 register address
 */
#define SP06_PSR_B2     0x00    /* Pressure MSB Register */
#define SP06_PSR_B1     0x01    /* Pressure LSB Register */
#define SP06_PSR_B0     0x02    /* Pressure XLSB Register */

#define SP06_TMP_B2     0x03    /* Temperature MSB Register */
#define SP06_TMP_B1     0x04    /* Temperature LSB Register */
#define SP06_TMP_B0     0x05    /* Temperature XLSB Register */

#define SP06_PSR_CFG    0x06    /* Pressure Configuration Register */
#define SP06_TMP_CFG    0x07    /* Temperature Configuration Register */
#define SP06_MEAS_CFG   0x08    /* Measurement Configuration Register */

#define SP06_CFG_REG    0x09    /* Configuration Register */
#define SP06_INT_STS    0x0A    /* Interrupt Status Register */
#define SP06_FIFO_STS   0x0B    /* FIFO Status Register */

#define SP06_RESET      0x0C    /* Soft Reset Register */
#define SP06_ID         0x0D    /* Chip ID Register */

#define SP06_COEF       0x10    /* Calibration Coefficients Start Address */
#define SP06_COEF_SRCE  0x28    /* Calibration Coefficients Source */

/*
 *  SPL06 Pressure measurement rate (sample/sec), Background mode
 */
#define PM_RATE_1           (0<<4)      /* 1 measurements pr. sec.   */
#define PM_RATE_2           (1<<4)      /* 2 measurements pr. sec.   */
#define PM_RATE_4           (2<<4)      /* 4 measurements pr. sec.   */
#define PM_RATE_8           (3<<4)      /* 8 measurements pr. sec.   */
#define PM_RATE_16          (4<<4)      /* 16 measurements pr. sec.  */
#define PM_RATE_32          (5<<4)      /* 32 measurements pr. sec.  */
#define PM_RATE_64          (6<<4)      /* 64 measurements pr. sec.  */
#define PM_RATE_128         (7<<4)      /* 128 measurements pr. sec. */

/*
 *  SPL06 Pressure oversampling rate (times), Background mode
 */
#define PM_PRC_1            0           /* Single       kP=524288,  3.6ms   */
#define PM_PRC_2            1           /* 2 times      kP=1572864, 5.2ms   */
#define PM_PRC_4            2           /* 4 times      kP=3670016, 8.4ms   */
#define PM_PRC_8            3           /* 8 times      kP=7864320, 14.8ms  */
#define PM_PRC_16           4           /* 16 times     kP=253952,  27.6ms  */
#define PM_PRC_32           5           /* 32 times     kP=516096,  53.2ms  */
#define PM_PRC_64           6           /* 64 times     kP=1040384, 104.4ms */
#define PM_PRC_128          7           /* 128 times    kP=2088960, 206.8ms */

/*
 *  SPL06 Temperature measurement rate (sample/sec), Background mode
 */
#define TMP_RATE_1          (0<<4)      /* 1 measurements pr. sec.   */
#define TMP_RATE_2          (1<<4)      /* 2 measurements pr. sec.   */
#define TMP_RATE_4          (2<<4)      /* 4 measurements pr. sec.   */
#define TMP_RATE_8          (3<<4)      /* 8 measurements pr. sec.   */
#define TMP_RATE_16         (4<<4)      /* 16 measurements pr. sec.  */
#define TMP_RATE_32         (5<<4)      /* 32 measurements pr. sec.  */
#define TMP_RATE_64         (6<<4)      /* 64 measurements pr. sec.  */
#define TMP_RATE_128        (7<<4)      /* 128 measurements pr. sec. */

/*
 *  SPL06 Temperature oversampling rate (times), Background mode
 */
#define TMP_PRC_1           0           /* Single */
#define TMP_PRC_2           1           /* 2 times   */
#define TMP_PRC_4           2           /* 4 times   */
#define TMP_PRC_8           3           /* 8 times   */
#define TMP_PRC_16          4           /* 16 times  */
#define TMP_PRC_32          5           /* 32 times  */
#define TMP_PRC_64          6           /* 64 times  */
#define TMP_PRC_128         7           /* 128 times */

/*
 *  SPL06 Measurement Control Register Bits
 */
#define MEAS_COEF_RDY                   0x80    /* Calibration coefficients ready */
#define MEAS_SENSOR_RDY                 0x40    /* Sensor initialization complete */
#define MEAS_TMP_RDY                    0x20    /* New temperature data available */
#define MEAS_PRS_RDY                    0x10    /* New pressure data available */

#define MEAS_CTRL_Standby               0x00    /* Standby mode */
#define MEAS_CTRL_PressMeasure          0x01    /* Single pressure measurement */
#define MEAS_CTRL_TempMeasure           0x02    /* Single temperature measurement */
#define MEAS_CTRL_ContinuousPress       0x05    /* Continuous pressure measurement */
#define MEAS_CTRL_ContinuousTemp        0x06    /* Continuous temperature measurement */
#define MEAS_CTRL_ContinuousPressTemp   0x07    /* Continuous pressure and temperature measurement */

/*
 *  SPL06 FIFO Status Register Bits
 */
#define SPL06_FIFO_FULL                 0x02    /* FIFO full flag */
#define SPL06_FIFO_EMPTY                0x01    /* FIFO empty flag */

/*
 *  SPL06 Interrupt Status Register Bits
 */
#define SPL06_INT_FIFO_FULL             0x04    /* FIFO full interrupt */
#define SPL06_INT_TMP                   0x02    /* Temperature interrupt */
#define SPL06_INT_PRS                   0x01    /* Pressure interrupt */

/*
 *  SPL06 Configuration Register Bits
 */
#define SPL06_CFG_T_SHIFT               0x08    /* Temperature shift for oversampling > 8 */
#define SPL06_CFG_P_SHIFT               0x04    /* Pressure shift for oversampling > 8 */

typedef struct
{
    int32_t Praw;   /* Raw pressure value */
    int32_t Traw;   /* Raw temperature value */
    float   Pcomp;  /* Compensated pressure value */
    float   Tcomp;  /* Compensated temperature value */
} SPL06_HandleTypeDef;

/*
 *  extern interface
 */
extern uint8_t SPL06_Init(void);
extern void SPL06_Get_Pressure_And_Temperature(float *temperature, float *pressure);
void *SPL06GetBus(void);
uint8_t SPL06GetDevAddr(void);
uint8_t SPL06GetDevId(void);

void SPL06_open(void);
void SPL06_close(void);

int SPL06_SelfCheck(void);

#ifdef __cplusplus
}
#endif

#endif /* __SPL06_H */

/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
