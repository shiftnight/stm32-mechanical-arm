/**
 * @file LED.h
 * @brief LED驱动头文件
 *
 * LED连接在PB13引脚，低电平点亮，高电平熄灭。
 * 这是因为LED的正极接VCC，负极接引脚（灌电流驱动）。
 */
#ifndef __LED_H
#define __LED_H
#include "main.h"

/* LED引脚配置：PB13 */
#define LED_PIN         GPIO_Pin_13
#define LED_GPIO_PORT   GPIOB
#define LED_GPIO_CLK    RCC_APB2Periph_GPIOB

/* LED控制宏 */
#define LED_GET_LEVEL() GPIO_ReadOutputDataBit(LED_GPIO_PORT, LED_PIN) /* 读取输出状态 */
#define LED_ON()        GPIO_ResetBits(LED_GPIO_PORT, LED_PIN)         /* 低电平=点亮 */
#define LED_OFF()       GPIO_SetBits(LED_GPIO_PORT, LED_PIN)           /* 高电平=熄灭 */
#define LED_TOGGLE      GPIO_WriteBit(LED_GPIO_PORT, LED_PIN, (BitAction)(1 - LED_GET_LEVEL())) /* 翻转 */

/* 初始化LED引脚 */
void led_init(void);

#endif
