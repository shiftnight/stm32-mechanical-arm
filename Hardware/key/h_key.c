/**
 * @file h_key.c
 * @brief 按键驱动实现
 *
 * 【按键检测流程】
 * 1. SysTick中断每10ms调用一次key_scan()
 * 2. 检测按键是否按下（低电平）
 * 3. 超过50ms消抖时间后，确认按键按下
 * 4. 松开后判断：短按（key_flag==1）或长按（key_flag==2）
 * 5. 设置keyx_pressing变量，供主循环读取
 *
 * 【状态机示意】
 * 未按下 → 按下(消抖中) → 确认按下 → 持续按住(可能长按) → 松开
 *                                    ↓                    ↓
 *                               key_flag=1           key_flag=2
 *                               (短按候选)           (长按确认)
 */
#include "h_key.h"

/* 按键状态：0=未按下，1=短按，2=长按 */
uint8_t key1_pressing = 0, key2_pressing = 0;

/* 内部标志：0=空闲，1=已确认按下(短按候选)，2=长按确认 */
static uint8_t key1_flag = 0, key2_flag = 0;

/* 按键按住时间计数（单位：10ms） */
static uint16_t key1_press_time = 0, key2_press_time = 0;

/**
 * @brief 初始化按键引脚
 * 配置PA8和PA11为上拉输入模式
 */
void key_init(void)
{
    RCC_APB2PeriphClockCmd(KEY1_GPIO_CLK | KEY2_GPIO_CLK, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = KEY1_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; /* 上拉输入：未按下=高电平 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(KEY1_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = KEY2_PIN;
    GPIO_Init(KEY2_GPIO_PORT, &GPIO_InitStructure);
}

/**
 * @brief 按键扫描函数，每10ms调用一次
 *
 * 检测逻辑：
 * 1. 如果按键按下（低电平），计时器+1
 * 2. 超过消抖时间(50ms)，确认按下，key_flag=1
 * 3. 超过长按时间(2s)，确认长按，key_flag=2
 * 4. 如果按键松开（高电平）：
 *    - key_flag==1 → 短按，设置keyx_pressing=1
 *    - key_flag==2 → 长按已处理
 *    - 重置所有状态
 */
void key_scan(void)
{
    /* ========== KEY1 处理 ========== */
    if (KEY1() == 0) /* 按键按下（低电平） */
    {
        key1_press_time++;
        if (key1_flag == 0) /* 还没确认按下 */
        {
            if (key1_press_time > DITHER_ELIMINATION_TIME) /* 超过消抖时间 */
            {
                key1_flag = 1; /* 确认按下 */
            }
        }
        if (key1_flag == 1) /* 已确认按下，检测是否长按 */
        {
            if (key1_press_time > LONG_PRESS_TIME) /* 超过2秒 */
            {
                key1_flag = 2;         /* 标记为长按 */
                key1_pressing = 2;     /* 设置长按状态 */
            }
        }
    }
    else /* 按键松开（高电平） */
    {
        if (key1_flag == 1) /* 短按：按下过但没到长按时间就松开了 */
        {
            key1_pressing = 1; /* 设置短按状态 */
        }
        else
        {
            key1_pressing = 0; /* 无按键 */
        }
        key1_flag = 0;
        key1_press_time = 0;
    }

    /* ========== KEY2 处理（逻辑同KEY1） ========== */
    if (KEY2() == 0)
    {
        key2_press_time++;
        if (key2_flag == 0)
        {
            if (key2_press_time > DITHER_ELIMINATION_TIME)
            {
                key2_flag = 1;
            }
        }
        if (key2_flag == 1)
        {
            if (key2_press_time > LONG_PRESS_TIME)
            {
                key2_flag = 2;
                key2_pressing = 2; /* 长按 */
            }
        }
    }
    else
    {
        if (key2_flag == 1)
        {
            key2_pressing = 1; /* 短按 */
        }
        else
        {
            key2_pressing = 0;
        }
        key2_flag = 0;
        key2_press_time = 0;
    }
}
