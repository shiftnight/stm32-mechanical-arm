/**
 * @file LED.c
 * @brief LED驱动实现
 *
 * 【LED驱动方式】
 * 本项目采用灌电流驱动：LED正极接VCC(3.3V)，负极接PB13。
 * - PB13输出低电平 → LED两端有电压差 → 点亮
 * - PB13输出高电平 → LED两端无电压差 → 熄灭
 *
 * 为什么不用拉电流（正极接引脚）？
 * STM32的灌电流能力(20mA)比拉电流能力更强，驱动LED更可靠。
 */
#include "stm32f10x.h"
#include "LED.h"

/**
 * @brief 初始化LED引脚
 * 配置PB13为推挽输出模式，默认熄灭
 */
void led_init(void)
{
    /* 使能GPIOB时钟 */
    RCC_APB2PeriphClockCmd(LED_GPIO_CLK, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = LED_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; /* 推挽输出 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_GPIO_PORT, &GPIO_InitStructure);
}
