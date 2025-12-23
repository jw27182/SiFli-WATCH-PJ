#include "board.h"
#include "drv_io.h"
#include "drv_touch.h"
#include <rtdevice.h>
#include <rtthread.h>

#define LOG_TAG "drv.cst9217"
#define DBG_LEVEL DBG_INFO
#include <drv_log.h>

#define TOUCH_SLAVE_ADDRESS (0x5A)

#define FTS_REG_DATA 0xD000
#define FTS_REG_PROJECT_ID 0xD204
#define FTS_REG_CMD_MODE 0xD101
#define FTS_REG_CHECKCODE 0xD1FC
#define FTS_REG_RESOLUTION 0xD1F8

#define CST9217_ACK_VALUE 0xAB
// #define CST9217_MAX_TOUCH_POINTS 1
// #define CST9217_DATA_LENGTH (CST9217_MAX_TOUCH_POINTS * 5 + 5)
#define CST9217_DATA_LENGTH 7
#define CST9217_X_OFFSET 0
#define CST9217_Y_OFFSET 0

static struct rt_i2c_bus_device *ft_bus = NULL;
static struct touch_drivers cst9217_driver;

static rt_err_t i2c_base_write(rt_uint8_t *buf, rt_uint16_t len)
{
    rt_int8_t res = 0;
    struct rt_i2c_msg msgs;

    msgs.addr = TOUCH_SLAVE_ADDRESS; /* slave address */
    msgs.flags = RT_I2C_WR;          /* write flag */
    msgs.buf = buf;                  /* Send data pointer */
    msgs.len = len;

    if (rt_i2c_transfer(ft_bus, &msgs, 1) == 1)
    {
        res = RT_EOK;
    }
    else
    {
        res = -RT_ERROR;
    }
    return res;
}

static rt_err_t i2c_base_read(rt_uint8_t *buf, rt_uint16_t len)
{
    rt_int8_t res = 0;
    struct rt_i2c_msg msgs;

    msgs.addr = TOUCH_SLAVE_ADDRESS; /* Slave address */
    msgs.flags = RT_I2C_RD;          /* Read flag */
    msgs.buf = buf;                  /* Read data pointer */
    msgs.len = len;                  /* Number of bytes read */

    if (rt_i2c_transfer(ft_bus, &msgs, 1) == 1)
    {
        res = RT_EOK;
    }
    else
    {
        res = -RT_ERROR;
    }
    return res;
}

static rt_err_t cst9217_i2c_read_reg16(uint16_t reg, uint8_t *p_data,
                                       uint8_t len)
{
    rt_int8_t res = 0;
    rt_uint8_t buf[2];
    struct rt_i2c_msg msgs[2];

    buf[0] = reg >> 8;
    buf[1] = reg;

    msgs[0].addr = TOUCH_SLAVE_ADDRESS; /* Slave address */
    msgs[0].flags = RT_I2C_WR;          /* Read flag */
    msgs[0].buf = buf;                  /* Read data pointer */
    msgs[0].len = 2;                    /* Number of bytes read */

    msgs[1].addr = TOUCH_SLAVE_ADDRESS; /* Slave address */
    msgs[1].flags = RT_I2C_RD;          /* Read flag */
    msgs[1].buf = p_data;               /* Read data pointer */
    msgs[1].len = len;                  /* Number of bytes read */

    if (rt_i2c_transfer(ft_bus, msgs, 2) == 2)
    {
        res = RT_EOK;
    }
    else
    {
        LOG_E("i2c transfer failed");
        res = -RT_ERROR;
    }
    return res;
}

static void cst9217_irq_handler(void *arg)
{
    rt_err_t ret = RT_ERROR;
    rt_touch_irq_pin_enable(0);

    ret = rt_sem_release(cst9217_driver.isr_sem);
    RT_ASSERT(RT_EOK == ret);
}

static rt_err_t read_point(touch_msg_t p_msg)
{
    rt_err_t ret = RT_ERROR;

    LOG_D("cst816 read_point");
    rt_touch_irq_pin_enable(1);

    uint8_t data[CST9217_DATA_LENGTH] = {0};
    cst9217_i2c_read_reg16(FTS_REG_DATA, data, sizeof(data));

    if (data[6] != CST9217_ACK_VALUE)
    {
        LOG_E("Invalid ACK: 0x%02X vs 0x%02X\n", data[6], CST9217_ACK_VALUE);
        return -RT_ERROR;
    }

    uint8_t *p = &data[0];
    uint8_t status = p[0] & 0x0F;

    if (status == 0x06)
    {
        p_msg->x = ((p[1] << 4) | (p[3] >> 4));
        p_msg->y = ((p[2] << 4) | (p[3] & 0x0F));
        p_msg->event = TOUCH_EVENT_DOWN;
        LOG_I("Point: X=%d, Y=%d", p_msg->x, p_msg->y);
    }
    else
    {
        p_msg->event = TOUCH_EVENT_UP;
        LOG_I("Touch released");
    }

    return RT_EEMPTY;
}

static rt_err_t init(void)
{
    BSP_TP_Reset(0);
    rt_thread_mdelay(30);
    BSP_TP_Reset(1);
    rt_thread_mdelay(30);

    rt_pin_mode(TOUCH_IRQ_PIN, PIN_MODE_INPUT);
    rt_touch_irq_pin_attach(PIN_IRQ_MODE_FALLING, cst9217_irq_handler, NULL);
    rt_touch_irq_pin_enable(1);
    return RT_EOK;
}

static rt_err_t deinit(void)
{
    rt_touch_irq_pin_enable(0);

    return RT_EOK;
}

static rt_bool_t probe(void)
{
    rt_err_t err;

    ft_bus = (struct rt_i2c_bus_device *)rt_device_find(TOUCH_DEVICE_NAME);
    if (RT_Device_Class_I2CBUS != ft_bus->parent.type)
    {
        ft_bus = NULL;
    }
    if (ft_bus)
    {
        rt_device_open((rt_device_t)ft_bus, RT_DEVICE_FLAG_RDWR |
                                                RT_DEVICE_FLAG_INT_TX |
                                                RT_DEVICE_FLAG_INT_RX);
    }
    else
    {
        LOG_I("bus not find\n");
        return RT_FALSE;
    }

    {
        struct rt_i2c_configuration configuration = {
            .mode = 0,
            .addr = 0,
            .timeout = 5000,
            .max_hz = 400000,
        };

        rt_i2c_configure(ft_bus, &configuration);
    }

    LOG_I("cst9217 probe OK");

    return RT_TRUE;
}

static struct touch_ops ops = {read_point, init, deinit};

static int rt_cst9217_register(void)
{
    cst9217_driver.probe = probe;
    cst9217_driver.ops = &ops;
    cst9217_driver.user_data = RT_NULL;
    cst9217_driver.isr_sem = rt_sem_create("cst9217", 0, RT_IPC_FLAG_FIFO);

    rt_touch_drivers_register(&cst9217_driver);
    return 0;
}

INIT_COMPONENT_EXPORT(rt_cst9217_register);

static void i2c_scan_command(int argc, char **argv)
{
    rt_device_t dev;
    struct rt_i2c_msg msgs[2];
    uint8_t buf = 0;
    rt_uint32_t i;

    dev = rt_device_find(argv[1]);
    if (!dev)
    {
        LOG_E("I2C bus device '%s' not found!", argv[1]);
        return;
    }

    if (rt_device_open(dev, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        LOG_E("Failed to open I2C bus device!");
        return;
    }

    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf = &buf;
    msgs[0].len = 1;

    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = &buf;
    msgs[1].len = 1;

    for (i = 0; i <= 0x7F; i++)
    {
        int len;

        msgs[0].addr = i;
        msgs[1].addr = i;

        len = rt_i2c_transfer((struct rt_i2c_bus_device *)dev, msgs, 2);

        if (len == 2)
        {
            rt_kprintf("\n================== %02x ====================\n", i);
        }

        rt_thread_mdelay(1);
    }

    rt_kprintf("\n");

    rt_device_close(dev);
}

MSH_CMD_EXPORT(i2c_scan_command, scan I2C devices on touch bus);