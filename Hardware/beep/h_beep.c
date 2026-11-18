/**
 * @file h_beep.c
 * @brief 蜂鸣器驱动实现
 *
 * 【蜂鸣器基础知识】
 * 蜂鸣器分两种：
 * - 有源蜂鸣器：内部有振荡器，给电就响，简单但音调固定
 * - 无源蜂鸣器：需要PWM驱动，可以改变频率发出不同音调
 * 本项目使用有源蜂鸣器，只需控制开关即可。
 */
#include "h_beep.h"

/**
 * @brief 初始化蜂鸣器引脚
 * 配置PB5为推挽输出模式
 */
void beep_init(void)
{
    /* GPIOB挂在APB2总线上，必须使用RCC_APB2PeriphClockCmd */
    RCC_APB2PeriphClockCmd(BEEP_GPIO_CLK, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = BEEP_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; /* 推挽输出：能输出高低电平 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BEEP_GPIO_PORT, &GPIO_InitStructure);
}

/**
 * @brief 蜂鸣器鸣叫指定次数
 * @param times 鸣叫次数
 * @param delay 每次鸣叫/静音的时长(ms)
 *
 * 例如 beep_on_times(3, 100) 会：响100ms→停100ms→响100ms→停100ms→响100ms→停100ms
 */
void beep_on_times(int times, int delay)
{
    int i;
    for (i = 0; i < times; i++)
    {
        BEEP_ON();
        Delay_ms(delay);
        BEEP_OFF();
        Delay_ms(delay);
    }
}
