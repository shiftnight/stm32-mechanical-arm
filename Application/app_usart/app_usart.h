/**
 * @file app_usart.h
 * @brief 串口应用层头文件
 *
 * 提供串口初始化和串口指令处理功能。
 * 指令处理会调用app_servo中的parse_action/parse_cmd/save_action函数。
 */
#ifndef _APP_UART_H_
#define _APP_UART_H_
#include "main.h"

void app_usart_init(void);

void app_usart_run(void);

#endif
