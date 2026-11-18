/**
 * @file app_ps2.h
 * @brief PS2手柄应用层头文件
 *
 * 【PS2手柄模式】
 * - 红灯模式(0x73)：模拟模式，可读取摇杆模拟量，适合控制舵机角度
 * - 绿灯模式(0x41)：数字模式，只读取按键，适合控制动作组
 *
 * 【按键映射】
 * 16个按键对应psx_buf[3]和psx_buf[4]的16个bit位
 * bit0=L2, bit1=R2, bit2=L1, bit3=R1, bit4=RU, bit5=RR,
 * bit6=RD, bit7=RL, bit8=SE, bit9=AL, bit10=AR, bit11=ST,
 * bit12=LU, bit13=LR, bit14=LD, bit15=LL
 * 0=按下，1=未按下
 */
#ifndef __APP_PS2_H__
#define __APP_PS2_H__
#include "main.h"

#define PS2_LED_RED 0X73
#define PS2_LED_GRN 0X41

/* PS2手柄增量控制参数
 * 增量值越大，按住按键时舵机移动越快
 * 时间值越小，舵机响应越快
 * 移动速度 = PS2_SERVO_STEP / PS2_SERVO_TIME (us/ms)
 * 当前速度 = 2000 / 2000 = 1us/ms = 1000us/s
 * 如需调整速度，修改这两个值即可 */
#define PS2_SERVO_STEP  2000
#define PS2_SERVO_TIME  2000

void app_ps2_init(void);
void app_ps2_run(void);
void app_ps2_buf(unsigned char *buf, unsigned char mode);

#endif
