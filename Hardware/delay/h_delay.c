/**
 * @file h_delay.c
 * @brief 延时函数实现
 *
 * 【延时原理】
 * - 纳秒/微秒延时：用空循环"浪费时间"，不精确但简单
 * - 毫秒延时：用SysTick计时器精确计时
 * - 秒延时：调用1000次毫秒延时
 *
 * 【为什么Delay_ms用SysTick而不是循环？】
 * 循环延时不准确，因为编译器优化会影响循环次数。
 * SysTick是硬件计时器，不受编译器影响，精确度很高。
 */
#include "h_delay.h"

/**
 * @brief 纳秒级延时（不精确）
 * @param xns 延时纳秒数
 * 用空循环实现，实际延时取决于时钟频率和编译器优化
 */
void Delay_ns(u32 xns)
{
    while (xns--)
        ;
}

/**
 * @brief 微秒级延时（不精确）
 * @param xus 延时微秒数
 * 内层循环10次约1微秒@72MHz
 */
void Delay_us(u32 xus)
{
    u32 i = 0;
    while (xus--)
    {
        i = 10;
        while (i--)
            ;
    }
}

/**
 * @brief 毫秒级延时（精确）
 * @param xms 延时毫秒数，范围：0~4294967295
 *
 * 原理：记录当前millis()值，然后等待millis()增长xms
 * 这样即使中途被中断打断，延时仍然准确。
 */
void Delay_ms(uint32_t xms)
{
    u32 systick_temp = millis(); /* 记录开始时间 */
    while (xms > (millis() - systick_temp))
    {
        /* 处理millis()溢出的情况（约49天后溢出） */
        if (systick_temp > millis())
        {
            xms = xms - (0xffffffff - systick_temp);
            systick_temp = 0;
        }
    }
}

/**
 * @brief 秒级延时
 * @param xs 延时秒数
 */
void Delay_s(uint32_t xs)
{
    while (xs--)
    {
        Delay_ms(1000);
    }
}
