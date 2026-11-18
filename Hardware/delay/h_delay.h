/**
 * @file h_delay.h
 * @brief 延时函数头文件
 *
 * 提供从纳秒到秒级别的延时函数，以及开机启动信号函数。
 */
#ifndef _DELAY_H_
#define _DELAY_H_
#include "main.h"

/* 纳秒级延时（软件循环，不精确） */
void Delay_ns(u32 xns);

/* 微秒级延时（软件循环，不精确） */
void Delay_us(u32 xus);

/* 毫秒级延时（基于SysTick，精确） */
void Delay_ms(uint32_t xms);

/* 秒级延时 */
void Delay_s(uint32_t xs);

#endif
