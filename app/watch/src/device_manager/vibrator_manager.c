#include "vibrator_manager.h"

#define DBG_TAG "dm.vibrator"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/* 配置参数 */
#define PWM_DEV_NAME        "pwm2"
#define PWM_CH              1
#define PWM_PERIOD_NS 10000
#define ENABLE_PIN 1
#define VIBRATOR_GAP_MS     50  /* 两次震动之间的最小间隔，防止“糊在一起” */

/* 震动消息结构体 */
struct vibrator_msg {
    rt_uint32_t duration;   /* 持续时间 (ms) */
    rt_uint32_t intensity;  /* 震动强度 (0-100) */
};

static struct rt_device_pwm *pwm_dev;
static struct rt_messagequeue vibrator_mq;
static rt_uint8_t mq_pool[256];
static struct rt_thread vibrator_thread;
static rt_uint8_t thread_stack[1024];

/**
 * 震动处理线程入口
 */
static void vibrator_thread_entry(void *parameter) {
    struct vibrator_msg msg;
    struct vibrator_msg new_msg;
    rt_bool_t interrupted = RT_FALSE;
    static rt_uint32_t task_count = 0;

    while (1) {
        /* 1. 等待震动请求 (阻塞模式) */
        LOG_I("Waiting for message...\n");
        if (rt_mq_recv(&vibrator_mq, &msg, sizeof(msg), RT_WAITING_FOREVER) == RT_EOK) {
            task_count++;
            LOG_I("Task #%d received: intensity=%d%%, duration=%dms\n", 
                      task_count, msg.intensity, msg.duration);
            
            do {
                interrupted = RT_FALSE;

                /* 2. 开始震动：设置PWM占空比并使能 */
                rt_uint32_t pulse = (PWM_PERIOD_NS * msg.intensity) / 100;
                LOG_I("Start vibration: period=%dns, pulse=%dns\n", 
                          PWM_PERIOD_NS, pulse);
                rt_pwm_set(pwm_dev, PWM_CH, PWM_PERIOD_NS, pulse);
                rt_pwm_enable(pwm_dev, PWM_CH);
                rt_pin_write(ENABLE_PIN, 1);

                /* 3. 等待震动完成，同时监听是否有新的打断信号 */
                LOG_I("Waiting for %dms (listening for interruption)\n", 
                          msg.duration);
                if (rt_mq_recv(&vibrator_mq, &new_msg, sizeof(new_msg), 
                               rt_tick_from_millisecond(msg.duration)) == RT_EOK) {
                    /* 监听到新消息 -> 触发打断机制 */
                    LOG_I(
                        "Vibration INTERRUPTED! New task: intensity=%d%%, "
                        "duration=%dms\n",
                        new_msg.intensity, new_msg.duration);
                    rt_pin_write(ENABLE_PIN, 0);
                    rt_pwm_disable(pwm_dev, PWM_CH);
                    rt_thread_mdelay(VIBRATOR_GAP_MS);
                    
                    msg = new_msg;      /* 更新为新任务 */
                    interrupted = RT_TRUE; /* 标记为打断，继续循环处理新任务 */
                    LOG_I("Processing interrupted task immediately\n");
                } else {
                    /* 震动正常结束 */
                    LOG_I("Vibration completed normally (%dms)\n",
                          msg.duration);
                    rt_pin_write(ENABLE_PIN, 0);
                    rt_pwm_disable(pwm_dev, PWM_CH);
                    rt_thread_mdelay(VIBRATOR_GAP_MS);
                    LOG_I("Gap period finished, ready for next task\n");
                }
            } while (interrupted);
        } else {
            LOG_I("ERROR: Failed to receive message from MQ\n");
        }
    }
}
/**
 * 发送震动请求 (API)
 * @param duration 持续时间 (ms)
 * @param intensity 强度 (0-100)
 */
int vibrator_send(rt_uint32_t duration, rt_uint32_t intensity) {
    struct vibrator_msg msg = {duration, intensity};
    
    /* 清空队列中积压的旧任务 (如果需要立即响应最新指令) */
    /* 这里直接发送，线程逻辑会自动处理打断 */
    return rt_mq_send(&vibrator_mq, &msg, sizeof(msg));
}

/**
 * 初始化震动管理器
 */
static int dm_vibrator_init(void) {
    /* 1. 查找PWM设备 */
    pwm_dev = (struct rt_device_pwm *)rt_device_find(PWM_DEV_NAME);
    if (!pwm_dev) {
        LOG_I("Vibrator Error: PWM device %s not found\\n", PWM_DEV_NAME);
        return -RT_ERROR;
    }
    rt_pwm_disable(pwm_dev, PWM_CH);
    rt_pin_mode(ENABLE_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(ENABLE_PIN, 0);

    /* 2. 初始化消息队列 */
    rt_mq_init(&vibrator_mq, "vib_mq", 
               mq_pool, sizeof(struct vibrator_msg), 
               sizeof(mq_pool), RT_IPC_FLAG_FIFO);

    /* 3. 创建并启动处理线程 */
    rt_thread_init(&vibrator_thread, "vibrator", 
                   vibrator_thread_entry, RT_NULL, 
                   thread_stack, sizeof(thread_stack), 
                   15, 20); /* 优先级设为中等偏高 */
    rt_thread_startup(&vibrator_thread);

    LOG_I("Vibrator Manager initialized successfully.\\n");
    return RT_EOK;
}
INIT_COMPONENT_EXPORT(dm_vibrator_init);