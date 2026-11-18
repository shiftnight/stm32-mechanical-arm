/**
 * @file h_rcc.h
 * @brief 时钟初始化头文件
 *
 * RCC = Reset and Clock Control（复位和时钟控制）
 * 时钟就像单片机的"心跳"，每个外设都需要时钟才能工作。
 */
#ifndef _H_RCC_H_
#define _H_RCC_H_
#include "stm32f10x_rcc.h"

/* 初始化系统时钟，配置为72MHz */
void rcc_init(void);

#endif
