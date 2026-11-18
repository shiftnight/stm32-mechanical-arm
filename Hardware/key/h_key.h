/**
 * @file h_key.h
 * @brief 按键驱动头文件
 *
 * 两个按键：KEY1(PA8)、KEY2(PA11)
 * 支持短按和长按检测，10ms中断扫描消抖。
 *
 * 【按键基础知识】
 * 机械按键按下/松开时，触点会弹跳（抖动），产生多次通断。
 * 消抖方法：检测到按键状态变化后，等50ms再确认。
 *
 * 【上拉输入模式】
 * 按键一端接引脚，一端接地。上拉输入时：
 * - 未按下：引脚读到高电平(1)
 * - 按下时：引脚读到低电平(0)
 */
#ifndef _H_KEY_H_
#define _H_KEY_H_
#include "main.h"

/* 按键扫描时间定义（基于10ms中断周期） */
#define KEY_50MS  5    /* 50ms = 5次×10ms */
#define KEY_100MS 10
#define KEY_200MS 20
#define KEY_500MS 50
#define KEY_1S    100  /* 1秒 = 100次×10ms */
#define KEY_2S    200
#define KEY_3S    300

/* 消抖时间：50ms，超过此时间才确认按键按下 */
#define DITHER_ELIMINATION_TIME KEY_50MS
/* 长按时间：2秒，超过此时间判定为长按 */
#define LONG_PRESS_TIME KEY_2S

/* KEY1引脚配置：PA8 */
#define KEY1_PIN        GPIO_Pin_8
#define KEY1_GPIO_PORT  GPIOA
#define KEY1_GPIO_CLK   RCC_APB2Periph_GPIOA

/* KEY2引脚配置：PA11 */
#define KEY2_PIN        GPIO_Pin_11
#define KEY2_GPIO_PORT  GPIOA
#define KEY2_GPIO_CLK   RCC_APB2Periph_GPIOA

/* 读取按键电平的宏：0=按下，1=未按下 */
#define KEY1() GPIO_ReadInputDataBit(KEY1_GPIO_PORT, KEY1_PIN)
#define KEY2() GPIO_ReadInputDataBit(KEY2_GPIO_PORT, KEY2_PIN)

/* 按键状态变量（其他文件可读取）：
 * 0=未按下，1=短按，2=长按
 * 短按需要手动清零，防止重复处理 */
extern uint8_t key1_pressing, key2_pressing;

/* 初始化按键引脚（上拉输入模式） */
void key_init(void);

/* 按键扫描函数，由SysTick中断每10ms调用一次 */
void key_scan(void);

#endif
