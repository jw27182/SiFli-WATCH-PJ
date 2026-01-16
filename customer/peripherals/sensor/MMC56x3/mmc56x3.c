#include "mmc56x3.h"

#include <math.h>
#include <rtthread.h>

#include "board.h"

#define DRV_DEBUG
#define LOG_TAG "drv.mag"
#include <drv_log.h>

#define MMC56x3_addr 0x30

typedef struct {
    float offset_x;
    float offset_y;
    float offset_z;
} mmc56x3_calib_t;

/* Indicate working mode of sensor */
static uint8_t sensor_state = 1;
static struct rt_i2c_bus_device *MMC56x3_bus;
static mmc56x3_calib_t g_mmc56x3_calib = {0};

static int MMC56x3_I2C_Init(const char *name) {
    /* get i2c bus device */
    MMC56x3_bus = rt_i2c_bus_device_find(name);
    if (MMC56x3_bus) {
        LOG_D("Find i2c bus device %s\n", name);
    } else {
        LOG_E("Can not found i2c bus %s, init fail\n", name);
        return -1;
    }

    return 0;
}

/**
 * @brief Factory test mode
 */
int MMC56x3_Factory_Test_Mode(void);

/**
 * @brief SET operation
 */
void MMC56x3_SET(void);

/**
 * @brief RESET operation
 */
void MMC56x3_RESET(void);

/**
 * @brief OTP read done check
 */
int MMC56x3_Check_OTP(void);

/**
 * @brief Check Product ID
 */
int MMC56x3_CheckID(void);

/**
 * @brief Auto self-test registers configuration
 */
void MMC56x3_Auto_SelfTest_Configuration(void);

/**
 * @brief Auto self-test
 */
int MMC56x3_Auto_SelfTest(void);

void mmc56x3_Delay_ms(uint16_t delay_time) { rt_thread_mdelay(delay_time); }

void MMC56x3_Write_Reg(uint8_t regAddr, uint8_t data) {
    RT_ASSERT(
        rt_i2c_mem_write(MMC56x3_bus, MMC56x3_addr, regAddr, 8, &data, 1) > 0);
}

void MMC56x3_Read_Reg(uint8_t regAddr, uint8_t *buf) {
    RT_ASSERT(rt_i2c_mem_read(MMC56x3_bus, MMC56x3_addr, regAddr, 8, buf, 1) >
              0);
}

void MMC56x3_MultiRead_Reg(uint8_t regAddr, uint8_t *buf, uint16_t len) {
    RT_ASSERT(rt_i2c_mem_read(MMC56x3_bus, MMC56x3_addr, regAddr, 8, buf, len) >
              0);
}

/*********************************************************************************
 * decription: Factory test mode
 *********************************************************************************/
int MMC56x3_Factory_Test_Mode(void) {
    int i;
    uint8_t data_reg[6] = {0};
    uint16_t data_set[3] = {0};
    uint16_t data_reset[3] = {0};
    uint32_t delta_data[3] = {0};

    const uint16_t thr_srst_low = 100;

    /* Write reg 0x1D */
    /* Set Cmm_en bit '0', Disable continuous mode */
    MMC56x3_Write_Reg(MMC56x3_REG_CTRL2, 0x00);

    mmc56x3_Delay_ms(20);

    /* Write reg 0x1B */
    /* Set Auto_SR_en bit '0', Disable the function of automatic set/reset */
    MMC56x3_Write_Reg(MMC56x3_REG_CTRL0, 0x00);

    /* Write reg 0x1C, Set BW<1:0> = 00 */
    MMC56x3_Write_Reg(MMC56x3_REG_CTRL1, 0x00);

    /* Do RESET operation */
    MMC56x3_RESET();
    /* Write 0x01 to register 0x1B, set Take_meas_M bit '1' */
    MMC56x3_Write_Reg(MMC56x3_REG_CTRL0, MMC56x3_CMD_TMM);
    /* Delay 10 ms to finish the TM operation */
    mmc56x3_Delay_ms(10);
    /* Read register data */
    MMC56x3_MultiRead_Reg(MMC56x3_REG_DATA, data_reg, 6);
    /* Get high 16bits data */
    data_reset[0] = (uint16_t)(data_reg[0] << 8 | data_reg[1]);  // X axis
    data_reset[1] = (uint16_t)(data_reg[2] << 8 | data_reg[3]);  // Y axis
    data_reset[2] = (uint16_t)(data_reg[4] << 8 | data_reg[5]);  // Z axis

    /* Do SET operation */
    MMC56x3_SET();
    /* Write 0x01 to register 0x1B, set Take_meas_M bit '1' */
    MMC56x3_Write_Reg(MMC56x3_REG_CTRL0, MMC56x3_CMD_TMM);
    /* Delay 10 ms to finish the TM operation */
    mmc56x3_Delay_ms(10);
    /* Read register data */
    MMC56x3_MultiRead_Reg(MMC56x3_REG_DATA, data_reg, 6);
    /* Get high 16bits data */
    data_set[0] = (uint16_t)(data_reg[0] << 8 | data_reg[1]);  // X axis
    data_set[1] = (uint16_t)(data_reg[2] << 8 | data_reg[3]);  // Y axis
    data_set[2] = (uint16_t)(data_reg[4] << 8 | data_reg[5]);  // Z axis

    for (i = 0; i < 3; i++) {
        if (data_set[i] >= data_reset[i])
            delta_data[i] = data_set[i] - data_reset[i];
        else
            delta_data[i] = data_reset[i] - data_set[i];
    }

    /* If output < 100lsb, fail*/
    if (delta_data[0] < thr_srst_low && delta_data[1] < thr_srst_low &&
        delta_data[2] < thr_srst_low)
        return -1;  // fail

    return 1;  // pass
}

/*********************************************************************************
 * decription: SET operation
 *********************************************************************************/
void MMC56x3_SET(void) {
    /* Write 0x08 to register 0x1B, set SET bit high */
    MMC56x3_Write_Reg(MMC56x3_REG_CTRL0, MMC56x3_CMD_SET);

    /* Delay to finish the SET operation */
    mmc56x3_Delay_ms(1);
}

/*********************************************************************************
 * decription: RESET operation
 *********************************************************************************/
void MMC56x3_RESET(void) {
    /* Write 0x10 to register 0x1B, set RESET bit high */
    MMC56x3_Write_Reg(MMC56x3_REG_CTRL0, MMC56x3_CMD_RESET);

    /* Delay to finish the RESET operation */
    mmc56x3_Delay_ms(1);
}

/*********************************************************************************
 * decription: Product ID check
 *********************************************************************************/
int MMC56x3_CheckID(void) {
    unsigned char pro_id = 0;

    /* Read register 0x39, check product ID */
    MMC56x3_Read_Reg(MMC56x3_REG_PRODUCTID1, &pro_id);
    LOG_D("pro_id:%d \n", pro_id);
    if (pro_id != MMC56x3_PRODUCT_ID) return -1;

    return 1;
}

/*********************************************************************************
 * decription: Auto self-test registers configuration
 *********************************************************************************/
void MMC56x3_Auto_SelfTest_Configuration(void) {
    int i;
    uint8_t reg_value[3];
    int16_t st_thr_data[3] = {0};
    int16_t st_thr_new[3] = {0};

    int16_t st_thd[3] = {0};
    uint8_t st_thd_reg[3];

    /* Read trim data from reg 0x27-0x29 */
    MMC56x3_MultiRead_Reg(MMC56x3_REG_ST_X_VAL, reg_value, 3);
    for (i = 0; i < 3; i++) {
        st_thr_data[i] = (int16_t)(reg_value[i] - 128) * 32;
        if (st_thr_data[i] < 0) st_thr_data[i] = -st_thr_data[i];
        st_thr_new[i] = st_thr_data[i] - st_thr_data[i] / 5;

        st_thd[i] = st_thr_new[i] / 8;
        if (st_thd[i] > 255)
            st_thd_reg[i] = 0xFF;
        else
            st_thd_reg[i] = (uint8_t)st_thd[i];
    }
    /* Write threshold into the reg 0x1E-0x20 */
    MMC56x3_Write_Reg(MMC56x3_REG_X_THD, st_thd_reg[0]);
    MMC56x3_Write_Reg(MMC56x3_REG_Y_THD, st_thd_reg[1]);
    MMC56x3_Write_Reg(MMC56x3_REG_Z_THD, st_thd_reg[2]);
}

/*********************************************************************************
 * decription: Auto self-test
 *********************************************************************************/
int MMC56x3_Auto_SelfTest(void) {
    uint8_t reg_status = 0;

    /* Write 0x40 to register 0x1B, set Auto_st_en bit high */
    MMC56x3_Write_Reg(MMC56x3_REG_CTRL0, MMC56x3_CMD_AUTO_ST_EN);

    /* Delay 15ms to finish the selftest process */
    mmc56x3_Delay_ms(15);

    /* Read register 0x18, check Sat_sensor bit */
    MMC56x3_Read_Reg(MMC56x3_REG_STATUS1, &reg_status);
    if ((reg_status & MMC56x3_SAT_SENSOR)) return -1;

    return 1;
}

/*********************************************************************************
 * decription: Continuous mode configuration with auto set and reset
 *********************************************************************************/
void MMC56x3_Continuous_Mode_With_Auto_SR(uint8_t bandwith,
                                          uint8_t sampling_rate) {
    /* Write reg 0x1C, Set BW<1:0> = bandwith */
    MMC56x3_Write_Reg(MMC56x3_REG_CTRL1, bandwith);

    /* Write reg 0x1A, set ODR<7:0> = sampling_rate */
    MMC56x3_Write_Reg(MMC56x3_REG_ODR, sampling_rate);

    /* Write reg 0x1B */
    /* Set Auto_SR_en bit '1', Enable the function of automatic set/reset */
    /* Set Cmm_freq_en bit '1', Start the calculation of the measurement period
     * according to the ODR*/
    MMC56x3_Write_Reg(MMC56x3_REG_CTRL0,
                      MMC56x3_CMD_CMM_FREQ_EN | MMC56x3_CMD_AUTO_SR_EN);

    /* Write reg 0x1D */
    /* Set Cmm_en bit '1', Enter continuous mode */
    MMC56x3_Write_Reg(MMC56x3_REG_CTRL2, MMC56x3_CMD_CMM_EN);
}

/*********************************************************************************
 * decription: Do selftest operation periodically
 *********************************************************************************/
int MMC56x3_Saturation_Checking(void) {
    int ret = 0;  // 1 pass, -1 fail, 0 elapsed time is less 5 seconds

    /* If sampling rate is 50Hz, then do saturation checking every 250 loops,
     * i.e. 5 seconds */
    static int NumOfSamples = 250;
    static int cnt = 0;

    if ((cnt++) >= NumOfSamples) {
        cnt = 0;
        ret = MMC56x3_Auto_SelfTest();
        if (ret == -1) {
            /* Sensor is saturated, need to do SET operation */
            MMC56x3_SET();
        }

        /* Do TM_M after selftest operation */
        MMC56x3_Write_Reg(MMC56x3_REG_CTRL0, MMC56x3_CMD_TMM);
        mmc56x3_Delay_ms(8);
    }
    return 1;
}

/*********************************************************************************
 * decription: Auto switch the working mode between Auto_SR and SETonly
 *********************************************************************************/
void MMC56x3_Auto_Switch(uint16_t *mag) {
    float mag_out[3];

    mag_out[0] =
        ((float)mag[0] - MMC56x3_16BIT_OFFSET) / MMC56x3_16BIT_SENSITIVITY;
    mag_out[1] =
        ((float)mag[1] - MMC56x3_16BIT_OFFSET) / MMC56x3_16BIT_SENSITIVITY;
    mag_out[2] =
        ((float)mag[2] - MMC56x3_16BIT_OFFSET) / MMC56x3_16BIT_SENSITIVITY;

    if (sensor_state == 1) {
        /* If X or Y axis output exceed 10 Gauss, then switch to single mode */
        if ((fabs(mag_out[0]) > 10.0f) || (fabs(mag_out[1]) > 10.0f)) {
            sensor_state = 2;

            /* Disable continuous mode */
            MMC56x3_Write_Reg(MMC56x3_REG_CTRL2, 0x00);
            mmc56x3_Delay_ms(15);  // Delay 15ms to finish the last sampling

            /* Do SET operation */
            MMC56x3_Write_Reg(MMC56x3_REG_CTRL0, MMC56x3_CMD_SET);
            mmc56x3_Delay_ms(1);  // Delay 1ms to finish the SET operation

            /* Do TM_M before next data reading */
            MMC56x3_Write_Reg(MMC56x3_REG_CTRL0, MMC56x3_CMD_TMM);
            mmc56x3_Delay_ms(8);  // Delay 8ms to finish the TM_M operation
        }
    } else if (sensor_state == 2) {
        /* If both of X and Y axis output less than 8 Gauss, then switch to
         * continuous mode with Auto_SR */
        if ((fabs(mag_out[0]) < 8.0f) && (fabs(mag_out[1]) < 8.0f)) {
            sensor_state = 1;

            /* Enable continuous mode with Auto_SR */
            MMC56x3_Write_Reg(MMC56x3_REG_CTRL0,
                              MMC56x3_CMD_CMM_FREQ_EN | MMC56x3_CMD_AUTO_SR_EN);
            MMC56x3_Write_Reg(MMC56x3_REG_CTRL2, MMC56x3_CMD_CMM_EN);
        } else {
            /* Sensor checking */
            if (MMC56x3_Saturation_Checking() == 0) {
                /* Do TM_M before next data reading */
                MMC56x3_Write_Reg(MMC56x3_REG_CTRL0, MMC56x3_CMD_TMM);
            }
        }
    }
}

/*********************************************************************************
 * decription: Disable sensor continuous mode
 *********************************************************************************/
void MMC56x3_Disable(void) {
    /* Write reg 0x1D */
    /* Set Cmm_en bit '0', Disable continuous mode */
    MMC56x3_Write_Reg(MMC56x3_REG_CTRL2, 0x00);

    mmc56x3_Delay_ms(20);
}

/*********************************************************************************
 * decription: Enable sensor
 *********************************************************************************/
void MMC56x3_Enable(void) {
    int ret = 0;

    /* Inite the sensor state */
    sensor_state = 1;

    /* Check product ID */
    ret = MMC56x3_CheckID();
    if (ret < 0) return;

    /* Auto self-test registers configuration */
    MMC56x3_Auto_SelfTest_Configuration();

    /* Do SET operation */
    MMC56x3_SET();

    /* Work mode setting */
    MMC56x3_Continuous_Mode_With_Auto_SR(MMC56x3_CMD_BW00, 50);

    mmc56x3_Delay_ms(20);
}

/*********************************************************************************
 * decription: Read the data register and convert to magnetic field
 *********************************************************************************/
void MMC56x3_GetData(float *mag_out) {
    uint8_t data_reg[6] = {0};
    uint16_t data_temp[3] = {0};

    /* Read register data */
    MMC56x3_MultiRead_Reg(MMC56x3_REG_DATA, data_reg, 6);

    /* Get high 16bits data */
    data_temp[0] = (uint16_t)(data_reg[0] << 8 | data_reg[1]);
    data_temp[1] = (uint16_t)(data_reg[2] << 8 | data_reg[3]);
    data_temp[2] = (uint16_t)(data_reg[4] << 8 | data_reg[5]);

    /* Transform to unit Gauss */
    mag_out[0] = ((float)data_temp[0] - MMC56x3_16BIT_OFFSET) /
                 MMC56x3_16BIT_SENSITIVITY;
    mag_out[1] = ((float)data_temp[1] - MMC56x3_16BIT_OFFSET) /
                 MMC56x3_16BIT_SENSITIVITY;
    mag_out[2] = ((float)data_temp[2] - MMC56x3_16BIT_OFFSET) /
                 MMC56x3_16BIT_SENSITIVITY;

    MMC56x3_Auto_Switch(data_temp);
}

mmc56x3_data_t MMC56x3_ReadData(void) {
    mmc56x3_data_t data = {0};
    /* Magnetic field vector, unit is gauss */
    float mag_raw_data[3] = {0.0};
    /* Get the MMC56x3 data, unit is gauss */
    MMC56x3_GetData(mag_raw_data);

    data.x = mag_raw_data[0] - g_mmc56x3_calib.offset_x;
    data.y = mag_raw_data[1] - g_mmc56x3_calib.offset_y;
    data.z = mag_raw_data[2] - g_mmc56x3_calib.offset_z;
    return data;
}

rt_err_t MMC56x3_Init(struct rt_sensor_config *cfg) {
    if (MMC56x3_I2C_Init(cfg->intf.dev_name) != 0) {
        return -RT_ERROR;
    }

    MMC56x3_Enable();
    return RT_EOK;
}

void MMC56x3_CalibrationCollect(uint16_t samples) {
    float x, y, z;
    float x_min = 1e6, y_min = 1e6, z_min = 1e6;
    float x_max = -1e6, y_max = -1e6, z_max = -1e6;

    for (uint16_t i = 0; i < samples; i++) {
        mmc56x3_data_t data = MMC56x3_ReadData();
        x = data.x;
        y = data.y;
        z = data.z;

        if (x < x_min) x_min = x;
        if (x > x_max) x_max = x;
        if (y < y_min) y_min = y;
        if (y > y_max) y_max = y;
        if (z < z_min) z_min = z;
        if (z > z_max) z_max = z;

        rt_thread_mdelay(20);
    }

    g_mmc56x3_calib.offset_x = (x_max + x_min) / 2.0f;
    g_mmc56x3_calib.offset_y = (y_max + y_min) / 2.0f;
    g_mmc56x3_calib.offset_z = (z_max + z_min) / 2.0f;

    LOG_I("MMC56x3 calib done:");
    LOG_I("offset x=%.2f y=%.2f z=%.2f", g_mmc56x3_calib.offset_x,
          g_mmc56x3_calib.offset_y, g_mmc56x3_calib.offset_z);
}

static void MMC56x3(int argc, char **argv) {
    uint16_t samples = 100;
    if (argc >= 2) {
        samples = (uint16_t)atoi(argv[1]);
    }
    MMC56x3_CalibrationCollect(samples);
}

MSH_CMD_EXPORT(MMC56x3, MMC56x3_CalibrationCollect);