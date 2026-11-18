/**
 * @file app_gpio.h
 * @brief 应用层GPIO头文件
 *
 * 包含LED、蜂鸣器、按键的应用层逻辑。
 * "应用层"意味着这里不直接操作硬件寄存器，
 * 而是调用底层驱动函数来实现具体功能。
 */
#ifndef _APP_GPIO_H_
#define _APP_GPIO_H_
#include "main.h"

/* 初始化所有GPIO设备（LED、蜂鸣器、按键） */
void app_gpio_init(void);

/* LED闪烁任务，在主循环中调用，每1秒翻转一次 */
void app_led_run(void);

/* 按键处理任务，在主循环中调用，处理按键事件 */
void app_key_run(void);

#endif
