/**
 * @file h_timer.h
 * @brief 定时器驱动头文件
 *
 * 本文件包含两个定时器的功能：
 * 1. SysTick —— 系统滴答计时器（Cortex-M3内核自带）
 * 2. TIM2 —— 通用定时器，用于生成舵机PWM信号
 */
#ifndef _H_TIMER_H_
#define _H_TIMER_H_
#include "main.h"

/* 初始化SysTick，每1ms中断一次 */
void SysTick_Init(void);

/* 获取当前毫秒数（从开机开始计时） */
u32 millis(void);

/* 初始化定时器2，用于生成舵机PWM信号
 * @param arr 自动重装载值，决定定时周期
 * @param psc 预分频系数，决定计数频率 */
void TIM2_Init(u16 arr, u16 psc);

#endif
