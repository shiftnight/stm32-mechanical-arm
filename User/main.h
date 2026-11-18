/**
 * @file main.h
 * @brief 主头文件 —— 整个工程的"总枢纽"
 *
 * 【作用】
 * 几乎所有的 .c 文件都会 #include "main.h"，因此这里集中包含了：
 * 1. 标准C库头文件（stdio.h 等）
 * 2. STM32系统头文件
 * 3. 所有硬件驱动头文件
 * 4. 全局函数声明
 *
 * 【为什么要把所有头文件集中在这里？】
 * 这样每个 .c 文件只需要 #include "main.h" 就够了，不用重复写很多 #include。
 * 虽然会增加一些编译时间，但对小项目来说更方便管理。
 */
#ifndef _MAIN_H_
#define _MAIN_H_

/*============================ 标准C库 ============================*/
/* stdio.h  - 标准输入输出，提供 printf、sprintf 等 */
#include <stdio.h>
/* string.h - 字符串操作，提供 strlen、memset 等 */
#include <string.h>
/* stdarg.h - 可变参数，提供 va_list 等，UsartPrintf函数需要 */
#include <stdarg.h>
/* stdint.h - 标准整数类型，提供 uint8_t、uint32_t 等 */
#include <stdint.h>
/* math.h   - 数学函数，提供 sin、cos、sqrt 等 */
#include <math.h>
/* misc.h   - STM32中断相关头文件，定义了IRQn（中断号） */
/* misc.h   - STM32中断相关头文件，定义了IRQn（中断号） */
#include "stm32f10x.h"  /* 如果找不到此文件，请检查Keil的Include路径是否包含STM32标准库目录 */

/*============================ STM32系统文件 ============================*/
/* system_stm32f10x.h - 系统初始化相关，定义了 SystemCoreClock 等 */
#include "system_stm32f10x.h"
/* stm32f10x_conf.h - 外设配置文件，决定包含哪些外设头文件 */
#include "stm32f10x_conf.h"

/*============================ 硬件驱动头文件 ============================*/
/* 时钟初始化：配置芯片主频为72MHz */
#include "h_rcc.h"
/* 定时器：SysTick(1ms计时) + TIM2(舵机PWM) */
#include "h_timer.h"
/* 延时函数：Delay_ms、Delay_us 等 */
#include "h_delay.h"
/* LED驱动：控制PB13引脚的LED灯 */
#include "LED.h"
/* 按键驱动：PA8/PA11两个按键，支持短按/长按 */
#include "h_key.h"
/* 蜂鸣器驱动：控制PB5引脚的蜂鸣器 */
#include "h_beep.h"
/* 串口驱动：USART1(调试) + USART3(总线) */
#include "h_usart.h"
/* SPI Flash驱动：W25Q64外部存储芯片 */
#include "h_flash.h"
/* 舵机驱动：6路舵机控制 */
#include "h_servo.h"
/* 应用层GPIO：LED闪烁、按键处理等 */
#include "app_gpio.h"
#include "app_servo.h"  
#include "app_ps2.h"
#include "h_kinematics.h"

/*============================ 全局函数声明 ============================*/
/* SWJ引脚配置：禁用JTAG调试接口，释放PB3/PB4引脚给舵机使用 */
void SWJ_gpio_init(void);
void other_init(void);
#endif
