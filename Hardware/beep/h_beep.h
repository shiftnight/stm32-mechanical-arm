/**
 * @file h_beep.h
 * @brief 蜂鸣器驱动头文件
 *
 * 蜂鸣器连接在PB5引脚，高电平鸣叫，低电平静音。
 * 使用推挽输出模式驱动。
 */
#ifndef _H_BEEP_H_
#define _H_BEEP_H_
#include "main.h"

/* 蜂鸣器引脚配置：PB5 */
#define BEEP_PIN         GPIO_Pin_5
#define BEEP_GPIO_PORT   GPIOB
#define BEEP_GPIO_CLK    RCC_APB2Periph_GPIOB

/* 蜂鸣器控制宏 */
#define BEEP_GET_LEVEL() GPIO_ReadInputDataBit(BEEP_GPIO_PORT, BEEP_PIN) /* 读取状态 */
#define BEEP_OFF()       GPIO_ResetBits(BEEP_GPIO_PORT, BEEP_PIN)        /* 关闭 */
#define BEEP_ON()        GPIO_SetBits(BEEP_GPIO_PORT, BEEP_PIN)          /* 打开 */
#define BEEP_TOGGLE()    GPIO_WriteBit(BEEP_GPIO_PORT, BEEP_PIN, (BitAction)(1 - BEEP_GET_LEVEL())) /* 翻转 */

/* 别名宏 */
#define beep_on()  BEEP_ON()
#define beep_off() BEEP_OFF()

/* 初始化蜂鸣器引脚 */
void beep_init(void);

/* 蜂鸣器鸣叫指定次数
 * @param times 鸣叫次数
 * @param delay 每次鸣叫的时长(ms) */
void beep_on_times(int times, int delay);

#endif
