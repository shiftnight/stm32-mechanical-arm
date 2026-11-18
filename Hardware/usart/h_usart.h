/**
 * @file h_usart.h
 * @brief 串口驱动头文件
 *
 * 本文件定义了USART1（调试串口）和USART3（总线串口）的相关配置。
 *
 * 【串口基础知识】
 * USART = Universal Synchronous/Asynchronous Receiver/Transmitter（通用同步/异步收发器）
 * 串口是单片机与外部设备通信最常用的方式之一，就像两个人通过电话线交流。
 * - TX（Transmit）：发送引脚，单片机的"嘴巴"
 * - RX（Receive）：接收引脚，单片机的"耳朵"
 * - 波特率：通信速度，双方必须一致才能正确通信，如115200表示每秒传输115200位
 *
 * 【本项目的串口用途】
 * - USART1：连接电脑，用于调试打印和接收控制指令
 * - USART3：连接总线设备，用于与其他模块通信
 */
#ifndef _H_USART_H_
#define _H_USART_H_
#include "main.h"

/*============================ 宏定义 ============================*/

/* 调试打印使用的串口组，修改这里可以切换调试串口 */
#define USART_DEBUG USART1

/* 接收缓冲区大小，单位：字节 */
#define UART_BUF_SIZE 1024

/*============================ 全局变量声明 ============================*/

/* 接收缓冲区，串口收到的数据会存入这个数组 */
extern u8 uart_receive_buf[UART_BUF_SIZE];

/* 接收完成标志：0=未完成，1=接收完成（收到结束符） */
extern uint16_t uart1_get_ok;

/* 指令模式：
 * 0 = 等待新指令
 * 1 = 命令模式（以$开头，!结尾）
 * 2 = 单舵机调试模式（以#开头，!结尾）
 * 3 = 多舵机调试模式（以{开头，!结尾）
 * 4 = 存储模式（以<开头，>结尾）
 */
extern u8 uart1_mode;

/*============================ 中断控制宏 ============================*/

/* 开启全局中断 */
#define interrupt_open()  { __enable_irq();  }
/* 关闭全局中断（一般不用，除非需要原子操作） */
#define interrupt_close() { __disable_irq(); }

/* 开启/关闭 USART1 接收中断 */
#define usart1_open()  { USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);  }
#define usart1_close() { USART_ITConfig(USART1, USART_IT_RXNE, DISABLE); }

/* 开启/关闭 USART3 接收中断 */
#define usart3_open()  { USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);  }
#define usart3_close() { USART_ITConfig(USART3, USART_IT_RXNE, DISABLE); }

/*============================ 函数声明 ============================*/

/* 初始化串口1（调试串口），参数baud为波特率 */
void Usart1_init(u32 baud);

/* 初始化串口3（总线串口），参数baud为波特率 */
void Usart3_init(u32 baud);

/* 发送字符串，如 Usart_SendString(USART1, "hello") */
void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str);

/* 发送整数，如 Usart_SendInt(USART1, 123) 会发送字符串"123" */
void Usart_SendInt(USART_TypeDef *USARTx, int num);

/* 格式化打印（类似printf），如 UsartPrintf(USART1, "val=%d\r\n", 100) */
void UsartPrintf(USART_TypeDef *USARTx, char *fmt, ...);

#endif
