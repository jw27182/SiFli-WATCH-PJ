#include "aht20.h"

#include <math.h>
#include <rtthread.h>

#include "board.h"
#include "stdlib.h"

#define DRV_DEBUG
#define LOG_TAG "drv.aht"
#include <drv_log.h>

#ifdef SENSOR_USING_AHT20

#define AHT20_STARTUP_TIME 20      // 上电启动时间
#define AHT20_CALIBRATION_TIME 40  // 初始化（校准）时间

#define AHT20_DEVICE_ADDR 0x38

#define AHT20_CMD_CALIBRATION 0xBE  // 初始化（校准）命令
#define AHT20_CMD_CALIBRATION_ARG0 0x08
#define AHT20_CMD_CALIBRATION_ARG1 0x00

/**
 * 传感器在采集时需要时间,主机发出测量指令（0xAC）后，延时75毫秒以上再读取转换后的数据并判断返回的状态位是否正常。
 * 若状态比特位[Bit7]为0代表数据可正常读取，为1时传感器为忙状态，主机需要等待数据处理完成。
 **/
#define AHT20_CMD_TRIGGER 0xAC  // 触发测量命令
#define AHT20_CMD_TRIGGER_ARG0 0x33
#define AHT20_CMD_TRIGGER_ARG1 0x00

// 用于在无需关闭和再次打开电源的情况下，重新启动传感器系统，软复位所需时间不超过20
// 毫秒
#define AHT20_CMD_RESET 0xBA  // 软复位命令

#define AHT20_CMD_STATUS 0x71  // 获取状态命令

#define AHT20_STATUS_BUSY_SHIFT 7  // bit[7] Busy indication
#define AHT20_STATUS_BUSY_MASK (0x1 << AHT20_STATUS_BUSY_SHIFT)
#define AHT20_STATUS_BUSY(status) \
    ((status & AHT20_STATUS_BUSY_MASK) >> AHT20_STATUS_BUSY_SHIFT)

#define AHT20_STATUS_CALI_SHIFT 3  // bit[3] CAL Enable
#define AHT20_STATUS_CALI_MASK (0x1 << AHT20_STATUS_CALI_SHIFT)
#define AHT20_STATUS_CALI(status) \
    ((status & AHT20_STATUS_CALI_MASK) >> AHT20_STATUS_CALI_SHIFT)

#define AHT20_STATUS_RESPONSE_MAX 7

#define AHT20_RESOLUTION (1 << 20)  // 2^20

#define AHT20_MAX_RETRY 10

static struct rt_i2c_bus_device *i2cbus = NULL;

static uint32_t AHT20_Read(uint8_t *buffer, uint32_t buffLen) {
#ifdef RT_USING_I2C
    struct rt_i2c_msg msgs[1];
    uint32_t res;

    if (i2cbus) {
        msgs[0].addr = AHT20_DEVICE_ADDR; /* Slave address */
        msgs[0].flags = RT_I2C_RD;        /* Write flag */
        msgs[0].buf = buffer;             /* Slave register address */
        msgs[0].len = buffLen;            /* Number of bytes sent */

        res = rt_i2c_transfer(i2cbus, msgs, 1);
        if (res != 1) return -RT_ERROR;
    }
    return RT_EOK;
#endif
}

static uint32_t AHT20_Write(uint8_t *buffer, uint32_t buffLen) {
#ifdef RT_USING_I2C
    struct rt_i2c_msg msgs[1];
    uint32_t res;

    if (i2cbus) {
        msgs[0].addr = AHT20_DEVICE_ADDR; /* Slave address */
        msgs[0].flags = RT_I2C_WR;        /* Write flag */
        msgs[0].buf = buffer;             /* Slave register address */
        msgs[0].len = buffLen;            /* Number of bytes sent */

        res = rt_i2c_transfer(i2cbus, msgs, 1);
        if (res != 1) return -RT_ERROR;
    }
    return RT_EOK;
#endif
}

// 发送获取状态命令
static uint32_t AHT20_StatusCommand(void) {
    uint8_t statusCmd[] = {AHT20_CMD_STATUS};
    return AHT20_Write(statusCmd, sizeof(statusCmd));
}

// 发送软复位命令
static uint32_t AHT20_ResetCommand(void) {
    uint8_t resetCmd[] = {AHT20_CMD_RESET};
    return AHT20_Write(resetCmd, sizeof(resetCmd));
}

// 发送初始化校准命令
static uint32_t AHT20_CalibrateCommand(void) {
    uint8_t clibrateCmd[] = {AHT20_CMD_CALIBRATION, AHT20_CMD_CALIBRATION_ARG0,
                             AHT20_CMD_CALIBRATION_ARG1};
    return AHT20_Write(clibrateCmd, sizeof(clibrateCmd));
}

// 读取温湿度值之前， 首先要看状态字的校准使能位Bit[3]是否为
// 1(通过发送0x71可以获取一个字节的状态字)，
// 如果不为1，要发送0xBE命令(初始化)，此命令参数有两个字节，
// 第一个字节为0x08，第二个字节为0x00。
uint32_t AHT20_Calibrate(void) {
    uint32_t ret = 0;
    uint8_t buffer[AHT20_STATUS_RESPONSE_MAX];
    rt_memset(&buffer, 0x0, sizeof(buffer));

    ret = AHT20_StatusCommand();
    if (ret != RT_EOK) {
        return ret;
    }

    ret = AHT20_Read(buffer, sizeof(buffer));
    if (ret != RT_EOK) {
        return ret;
    }

    if (AHT20_STATUS_BUSY(buffer[0]) || !AHT20_STATUS_CALI(buffer[0])) {
        ret = AHT20_ResetCommand();
        if (ret != RT_EOK) {
            return ret;
        }
        rt_thread_mdelay(AHT20_STARTUP_TIME);
        ret = AHT20_CalibrateCommand();
        rt_thread_mdelay(AHT20_CALIBRATION_TIME);
        return ret;
    }

    return RT_EOK;
}

// 发送 触发测量 命令，开始测量
uint32_t AHT20_StartMeasure(void) {
    uint8_t triggerCmd[] = {AHT20_CMD_TRIGGER, AHT20_CMD_TRIGGER_ARG0,
                            AHT20_CMD_TRIGGER_ARG1};
    return AHT20_Write(triggerCmd, sizeof(triggerCmd));
}

static uint8_t CheckCrc8(uint8_t *pDat, uint8_t Lenth) {
    uint8_t crc = 0xff, i, j;

    for (i = 0; i < Lenth; i++) {
        crc = crc ^ *pDat;
        for (j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
        pDat++;
    }
    return crc;
}

// 接收测量结果，拼接转换为标准值
uint32_t AHT20_GetMeasureResult(float *temp, float *humi) {
    uint32_t ret = 0, i = 0;
    if (temp == NULL || humi == NULL) {
        return -RT_ERROR;
    }

    uint8_t buffer[AHT20_STATUS_RESPONSE_MAX];
    ret = AHT20_Read(buffer, sizeof(buffer));
    if (ret != RT_EOK) {
        return ret;
    }
    LOG_I("status=%x\r\n", buffer[0]);
    for (i = 0;
         (AHT20_STATUS_BUSY(buffer[0]) || CheckCrc8(buffer, 6) != buffer[6]) &&
         i < AHT20_MAX_RETRY;
         i++) {
        if (AHT20_STATUS_BUSY(buffer[0]))
            LOG_D("AHT20 device busy, retry %d/%d!\r\n", i, AHT20_MAX_RETRY);
        else
            LOG_D("AHT20 crc error, retry %d/%d!\r\n", i, AHT20_MAX_RETRY);
        rt_thread_mdelay(AHT20_MEASURE_TIME);
        ret = AHT20_Read(buffer, sizeof(buffer));
        if (ret != RT_EOK) {
            return ret;
        }
        LOG_I("status=%x\r\n", buffer[0]);
    }
    if (i >= AHT20_MAX_RETRY) {
        LOG_E("AHT20 device always busy!\r\n");
        return -RT_ERROR;
    }

    uint32_t humiRaw = buffer[1];
    humiRaw = (humiRaw << 8) | buffer[2];
    humiRaw = (humiRaw << 4) | ((buffer[3] & 0xF0) >> 4);
    *humi = humiRaw / (float)AHT20_RESOLUTION * 100;

    uint32_t tempRaw = buffer[3] & 0x0F;
    tempRaw = (tempRaw << 8) | buffer[4];
    tempRaw = (tempRaw << 8) | buffer[5];
    *temp = tempRaw / (float)AHT20_RESOLUTION * 200 - 50;
    LOG_I("humi = %05X, %f, temp= %05X, %f\r\n", humiRaw, *humi, tempRaw,
          *temp);
    return RT_EOK;
}

uint8_t AHT20_Init() {
    /* get i2c bus device */
    i2cbus = rt_i2c_bus_device_find(AHT20_I2C_BUS);
    if (i2cbus) {
        LOG_D("Find i2c bus device %s\n", AHT20_I2C_BUS);
        rt_i2c_open(i2cbus, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_INT_TX);
    } else {
        LOG_E("Can not found i2c bus %s, AHT20_Init fail\n", AHT20_I2C_BUS);
        return -RT_ERROR;
    }

    return RT_EOK;
}

#define DRV_AHT20_TEST

#ifdef DRV_AHT20_TEST
#include <string.h>

int cmd_ahtt(int argc, char *argv[]) {
    float temperature = 0.0f, humidity = 0.0f;
    uint32_t ret = 0;

    ret = AHT20_Init();
    if (ret != RT_EOK) {
        LOG_E("AHT20_Init failed! ret=%d", ret);
        return -1;
    }

    ret = AHT20_Calibrate();
    LOG_I("AHT20_Calibrate: %d\r\n", ret);

    while (1) {
        float temp = 0.0, humi = 0.0;

        ret = AHT20_StartMeasure();
        LOG_I("AHT20_StartMeasure: %d\r\n", ret);
        rt_thread_mdelay(AHT20_MEASURE_TIME);

        ret = AHT20_GetMeasureResult(&temperature, &humidity);

        rt_thread_mdelay(1);
    }

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_ahtt, __cmd_ahtt, Test driver aht20);

#endif  // DRV_AHT20_TEST

#endif  // SENSOR_USING_AHT20
