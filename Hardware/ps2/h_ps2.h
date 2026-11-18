/**
 * @file h_ps2.h
 * @brief PS2手柄驱动头文件
 *
 * 【PS2手柄基础知识】
 * PS2手柄通过SPI-like协议与单片机通信，需要4根线：
 * - DAT：数据线，手柄→单片机（PA15）
 * - CMD：命令线，单片机→手柄（PA14）
 * - CS：片选线，低电平选中手柄（PA13）
 * - CLK：时钟线，单片机产生时钟（PA12）
 *
 * 手柄有两种模式：
 * - 绿灯模式(0x41)：数字模式，只读取按键
 * - 红灯模式(0x73)：模拟模式，可读取摇杆模拟量
 */
#ifndef __H_PS2_H__
#define __H_PS2_H__
#include "main.h"

/* PS2手柄数据缓冲区，9字节 */
extern u8 psx_buf[9];

/* PS2手柄连接状态标志 */
extern u8 ps2_isConnected;

/* DAT引脚：PA15（手柄数据输出） */
#define PS2_DAT_PIN        GPIO_Pin_15
#define PS2_DAT_GPIO_PORT  GPIOA
#define PS2_DAT_GPIO_CLK   RCC_APB2Periph_GPIOA

/* CMD引脚：PA14（单片机命令输出） */
#define PS2_CMD_PIN        GPIO_Pin_14
#define PS2_CMD_GPIO_PORT  GPIOA
#define PS2_CMD_GPIO_CLK   RCC_APB2Periph_GPIOA

/* CS引脚：PA13（片选，低电平有效） */
#define PS2_CS_PIN         GPIO_Pin_13
#define PS2_CS_GPIO_PORT   GPIOA
#define PS2_CS_GPIO_CLK    RCC_APB2Periph_GPIOA

/* CLK引脚：PA12（时钟，由单片机产生） */
#define PS2_CLK_PIN        GPIO_Pin_12
#define PS2_CLK_GPIO_PORT  GPIOA
#define PS2_CLK_GPIO_CLK   RCC_APB2Periph_GPIOA

/* PS2指令表 */
#define START_CMD   0x01   /* 开始指令 */
#define ASK_DAT_CMD 0x42   /* 询问数据指令 */

/* PS2模式数据 */
#define PS2_MODE_GRN 0x41  /* 绿灯模式（数字模式） */
#define PS2_MODE_RED 0x73  /* 红灯模式（模拟模式） */

/* PS2引脚控制宏 */
#define PS2_DAT()  GPIO_ReadInputDataBit(PS2_DAT_GPIO_PORT, PS2_DAT_PIN) /* 读取DAT */
#define PS2_CMD(X) GPIO_WriteBit(PS2_CMD_GPIO_PORT, PS2_CMD_PIN, X)     /* 设置CMD */
#define PS2_CS(X)  GPIO_WriteBit(PS2_CS_GPIO_PORT, PS2_CS_PIN, X)       /* 设置CS */
#define PS2_CLK(X) GPIO_WriteBit(PS2_CLK_GPIO_PORT, PS2_CLK_PIN, X)     /* 设置CLK */

/* PS2按键数量 */
#define PSX_BUTTON_NUM 16

/* PS2相关函数声明 */
void ps2_init(void);        /* 初始化PS2引脚 */
void ps2_write_read(void);  /* 发送指令并读取手柄数据 */
u8 ps2_transfer(u8 data);   /* SPI-like传输一个字节 */
void parse_psx_buf(u8 *buf, u8 mode); /* 解析手柄按键数据 */

#endif
