/**
 ******************************************************************************
 * @file   SPL06.c
 * @author Sifli software development team
 * @brief   This file includes the SPL06 driver functions
 *
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

#include "SPL06.h"

#include <math.h>
#include <rtthread.h>

#include "board.h"
#include "stdlib.h"

#define DRV_DEBUG
#define LOG_TAG "drv.spl06"
#include <drv_log.h>

#ifdef SENSOR_USING_SPL06

/* SPL06 calibration coefficients */
static float _kT, _kP;
static int16_t _c0, _c1, _c01, _c11, _c20, _c21, _c30;
static int32_t _c10, _c00;

SPL06_HandleTypeDef spl06;
static struct rt_i2c_bus_device *i2cbus = NULL;
uint8_t spl06_dev_addr = SPL06_ADDR;

static void I2C_WriteOneByte(uint8_t DevAddr, uint8_t RegAddr, uint8_t Data) {
#ifdef RT_USING_I2C
    struct rt_i2c_msg msgs[1];
    uint8_t value[2];
    uint32_t res;

    if (i2cbus) {
        value[0] = RegAddr;
        value[1] = Data;

        msgs[0].addr = DevAddr;    /* Slave address */
        msgs[0].flags = RT_I2C_WR; /* Write flag */
        msgs[0].buf = value;       /* Data buffer */
        msgs[0].len = 2;           /* Number of bytes sent */

        res = rt_i2c_transfer(i2cbus, msgs, 1);
        if (res == 1) {
            // LOG_D("I2C_WriteOneByte OK\n");
        } else {
            LOG_D("I2C_WriteOneByte FAIL %d\n", res);
        }
    }
#endif
}

void SPL06_ReadReg(uint8_t RegAddr, uint8_t Num, uint8_t *pBuffer) {
#ifdef RT_USING_I2C
    struct rt_i2c_msg msgs[2];
    uint32_t res;

    if (i2cbus) {
        msgs[0].addr = spl06_dev_addr; /* Slave address */
        msgs[0].flags = RT_I2C_WR;     /* Write flag */
        msgs[0].buf = &RegAddr;        /* Slave register address */
        msgs[0].len = 1;               /* Number of bytes sent */

        msgs[1].addr = spl06_dev_addr; /* Slave address */
        msgs[1].flags = RT_I2C_RD;     /* Read flag */
        msgs[1].buf = pBuffer;         /* Read data pointer */
        msgs[1].len = Num;             /* Number of bytes read */

        res = rt_i2c_transfer(i2cbus, msgs, 2);
        if (res == 2) {
            // LOG_D("SPL06_ReadReg OK\n");
        } else {
            LOG_D("SPL06_ReadReg FAIL %d\n", res);
        }
    }
#endif
}

void SPL06_WriteReg(uint8_t RegAddr, uint8_t Val) {
    I2C_WriteOneByte(spl06_dev_addr, RegAddr, Val);
}

static void SPL06_Pressure_Config(uint8_t rate, uint8_t oversampling) {
    uint8_t regval = 0;

    switch (oversampling) {
        case PM_PRC_1:
            _kP = 524288;
            break;
        case PM_PRC_2:
            _kP = 1572864;
            break;
        case PM_PRC_4:
            _kP = 3670016;
            break;
        case PM_PRC_8:
            _kP = 7864320;
            break;
        case PM_PRC_16:
            _kP = 253952;
            break;
        case PM_PRC_32:
            _kP = 516096;
            break;
        case PM_PRC_64:
            _kP = 1040384;
            break;
        case PM_PRC_128:
            _kP = 2088960;
            break;
        default:
            break;
    }

    regval = rate | oversampling;
    SPL06_WriteReg(SP06_PSR_CFG, regval);

    if (oversampling > PM_PRC_8) {
        SPL06_ReadReg(SP06_CFG_REG, 1, &regval);
        regval |= SPL06_CFG_P_SHIFT;
        SPL06_WriteReg(SP06_CFG_REG, regval);
    }
}

static void SPL06_Temperature_Config(uint8_t rate, uint8_t oversampling) {
    uint8_t regval = 0;

    switch (oversampling) {
        case TMP_PRC_1:
            _kT = 524288;
            break;
        case TMP_PRC_2:
            _kT = 1572864;
            break;
        case TMP_PRC_4:
            _kT = 3670016;
            break;
        case TMP_PRC_8:
            _kT = 7864320;
            break;
        case TMP_PRC_16:
            _kT = 253952;
            break;
        case TMP_PRC_32:
            _kT = 516096;
            break;
        case TMP_PRC_64:
            _kT = 1040384;
            break;
        case TMP_PRC_128:
            _kT = 2088960;
            break;
        default:
            break;
    }

    regval = rate | oversampling | 0x80; /* External sensor */
    SPL06_WriteReg(SP06_TMP_CFG, regval);

    if (oversampling > TMP_PRC_8) {
        SPL06_ReadReg(SP06_CFG_REG, 1, &regval);
        regval |= SPL06_CFG_T_SHIFT;
        SPL06_WriteReg(SP06_CFG_REG, regval);
    }
}

void SPL06_Read_Calibration(void) {
    uint8_t coef[18] = {0};

    SPL06_ReadReg(SP06_COEF, 18, coef);

    /* Parse calibration coefficients */
    _c0 = ((int16_t)coef[0] << 4) | ((coef[1] & 0xF0) >> 4);
    _c0 = (_c0 & 0x0800) ? (0xF000 | _c0) : _c0;
    _c1 = ((int16_t)(coef[1] & 0x0F) << 8) | coef[2];
    _c1 = (_c1 & 0x0800) ? (0xF000 | _c1) : _c1;
    _c00 = ((int32_t)coef[3] << 12) | ((uint32_t)coef[4] << 4) | (coef[5] >> 4);
    _c00 = (_c00 & 0x080000) ? (0xFFF00000 | _c00) : _c00;
    _c10 =
        ((int32_t)(coef[5] & 0x0F) << 16) | ((uint32_t)coef[6] << 8) | coef[7];
    _c10 = (_c10 & 0x080000) ? (0xFFF00000 | _c10) : _c10;
    _c01 = ((int16_t)coef[8] << 8) | coef[9];
    _c11 = ((int16_t)coef[10] << 8) | coef[11];
    _c20 = ((int16_t)coef[12] << 8) | coef[13];
    _c21 = ((int16_t)coef[14] << 8) | coef[15];
    _c30 = ((int16_t)coef[16] << 8) | coef[17];
}

void SPL06_Get_Raw_Data(void) {
    uint8_t regval[3] = {0};
    uint8_t status = 0;
    int retry = 5;

    /* Check measurement ready status before reading */
    while(retry-- > 0)
    {
        SPL06_ReadReg(SP06_MEAS_CFG, 1, &status);
        
        /* Check if both pressure and temperature data are ready */
        if((status & 0x30) == 0x30)  /* bit4: Pressure ready, bit5: Temperature ready */
        {
            break;
        }
        LOG_D("SPL06_Get_Raw_Data: Measurement not ready\n");
        rt_thread_mdelay(200);
    }

    if(retry <= 0)
    {
        LOG_D("SPL06_Get_Raw_Data: Measurement not ready too much times\n");
        SPL06_WriteReg(SP06_MEAS_CFG, MEAS_CTRL_Standby);
        rt_thread_mdelay(10);
        SPL06_Pressure_Config(PM_RATE_4, PM_PRC_32);
        SPL06_Temperature_Config(PM_RATE_4, TMP_PRC_32);
        SPL06_WriteReg(SP06_MEAS_CFG, MEAS_CTRL_ContinuousPressTemp);
        return;
    }

    SPL06_ReadReg(SP06_PSR_B2, 3, regval);
    spl06.Praw = (int32_t)regval[0] << 16 | (int32_t)regval[1] << 8 | regval[2];
    spl06.Praw =
        (spl06.Praw & 0x00800000) ? (0xFF000000 | spl06.Praw) : spl06.Praw;

    SPL06_ReadReg(SP06_TMP_B2, 3, regval);
    spl06.Traw = (int32_t)regval[0] << 16 | (int32_t)regval[1] << 8 | regval[2];
    spl06.Traw =
        (spl06.Traw & 0x00800000) ? (0xFF000000 | spl06.Traw) : spl06.Traw;
}

void SPL06_Compensate_Data(void) {
    float Praw_sc = spl06.Praw / _kP;
    float Traw_sc = spl06.Traw / _kT;

    spl06.Pcomp = _c00 + Praw_sc * (_c10 + Praw_sc * (_c20 + Praw_sc * _c30)) +
                  Traw_sc * _c01 + Traw_sc * Praw_sc * (_c11 + Praw_sc * _c21);

    spl06.Tcomp = _c0 * 0.5 + _c1 * Traw_sc;
}

extern uint8_t SPL06_Init() {
    uint8_t u8Ret = SPL06_RET_OK;
    uint8_t u8ChipID;

    /* get i2c bus device */
    i2cbus = rt_i2c_bus_device_find(SPL06_I2C_BUS);
    if (i2cbus) {
        LOG_D("Find i2c bus device %s\n", SPL06_I2C_BUS);
        rt_i2c_open(i2cbus, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_INT_TX);
    } else {
        LOG_E("Can not found i2c bus %s, SPL06_Init fail\n", SPL06_I2C_BUS);
        return SPL06_RET_NG;
    }

    /* check spl06 slave address */
    spl06_dev_addr = SPL06_ADDR;
    u8ChipID = 0;

    /* Soft reset first */
    uint8_t regval = 0x89;
    SPL06_WriteReg(SP06_RESET, regval);

    /* Read ChipID after reset */
    SPL06_ReadReg(SP06_ID, 1, &u8ChipID);

    if (u8ChipID == SPL06_CHIP_ID) {
        LOG_D("SPL06 I2C slave address = 0x%x\n", spl06_dev_addr);
    } else {
        LOG_D("SPL06 init fail: ChipID mismatch, got 0x%x\n", u8ChipID);
        u8Ret = SPL06_RET_NG;
        return u8Ret;
    }

    /* Wait for sensor to be ready */
    rt_thread_delay(rt_tick_from_millisecond(50));

    /* Read calibration coefficients */
    SPL06_Read_Calibration();

    /* Configure pressure and temperature */
    SPL06_Pressure_Config(PM_RATE_4, PM_PRC_32);
    SPL06_Temperature_Config(PM_RATE_4, TMP_PRC_32);

    /* Start continuous measurement */
    regval = MEAS_CTRL_ContinuousPressTemp;
    SPL06_WriteReg(SP06_MEAS_CFG, regval);

    LOG_D("SPL06 init successful : ChipID [0x%x]\r\n", u8ChipID);

    return u8Ret;
}

void SPL06_open(void) {
    uint8_t regval = MEAS_CTRL_ContinuousPressTemp;
    SPL06_WriteReg(SP06_MEAS_CFG, regval);
}

void SPL06_close(void) {
    uint8_t regval = MEAS_CTRL_Standby;
    SPL06_WriteReg(SP06_MEAS_CFG, regval);
}

void SPL06_Get_Pressure_And_Temperature(float *temperature, float *pressure) {
    SPL06_Get_Raw_Data();
    SPL06_Compensate_Data();

    *temperature = spl06.Tcomp;
    *pressure = spl06.Pcomp;
}

void *SPL06GetBus(void) { return (void *)i2cbus; }

uint8_t SPL06GetDevAddr(void) { return spl06_dev_addr; }

uint8_t SPL06GetDevId(void) { return SPL06_CHIP_ID; }

int SPL06_SelfCheck(void) {
    uint8_t u8ChipID = 0;

    SPL06_ReadReg(SP06_ID, 1, &u8ChipID);
    if (u8ChipID != SPL06_CHIP_ID) return -1;

    return 0;
}

#define DRV_SPL06_TEST

#ifdef DRV_SPL06_TEST
#include <string.h>

int cmd_spl06(int argc, char *argv[]) {
    float temp, pres;

    if (argc < 2) {
        LOG_I("Invalid parameter!\n");
        return 1;
    }

    if (strcmp(argv[1], "-open") == 0) {
        uint8_t res = SPL06_Init();
        if (SPL06_RET_OK == res) {
            SPL06_open();
            LOG_I("Open spl06 success\n");
        } else
            LOG_I("open spl06 fail\n");
    }

    if (strcmp(argv[1], "-close") == 0) {
        SPL06_close();
        LOG_I("SPL06 closed\n");
    }

    if (strcmp(argv[1], "-r") == 0) {
        uint8_t rega = atoi(argv[2]) & 0xff;
        uint8_t value;
        SPL06_ReadReg(rega, 1, &value);
        LOG_I("Reg 0x%x value 0x%x\n", rega, value);
    }

    if (strcmp(argv[1], "-tp") == 0) {
        temp = 0;
        pres = 0;
        SPL06_Get_Pressure_And_Temperature(&temp, &pres);
        LOG_I("Get temperature = %.2f C\n", temp);
        LOG_I("Get pressure = %.2f Pa\n", pres);
    }

    if (strcmp(argv[1], "-bps") == 0) {
        struct rt_i2c_configuration cfg;
        int bps = atoi(argv[2]);
        cfg.addr = 0;
        cfg.max_hz = bps;
        cfg.mode = 0;
        cfg.timeout = 5000;
        rt_i2c_configure(i2cbus, &cfg);
        LOG_I("Config SPL06 I2C speed to %d\n", bps);
    }

    if (strcmp(argv[1], "-l") == 0) {
        temp = 0;
        pres = 0;
        uint32_t loop = atoi(argv[2]);
        float prev1 = 0, prev2 = 0;

        if (loop > 36000) loop = 36000;

        uint8_t res = SPL06_Init();
        if (SPL06_RET_OK == res) {
            SPL06_open();
            LOG_I("Open spl06 success\n");
        } else
            LOG_I("open spl06 fail\n");

        do {
            SPL06_Get_Pressure_And_Temperature(&temp, &pres);
            if (prev1 != temp || prev2 != pres) {
                prev1 = temp;
                prev2 = pres;
                LOG_I("Get temperature = %.2f C\n", temp);
                LOG_I("Get pressure = %.2f Pa\n", pres);
            }
            loop--;
            rt_thread_delay(100);
        } while (loop > 0);

        SPL06_close();
        LOG_I("SPL06 closed\n");

    }

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_spl06, __cmd_spl06, Test driver spl06);

#endif /* DRV_SPL06_TEST */

#endif /* SENSOR_USING_SPL06 */

/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
