/**
 * @file h_rcc.c
 * @brief 时钟初始化实现
 *
 * 【时钟系统知识】
 * STM32F103的时钟来源有4种：
 * - HSI：内部高速RC振荡器，8MHz，精度低但不需要外部元件
 * - HSE：外部高速晶振，本板使用8MHz晶振，精度高
 * - LSI：内部低速RC振荡器，约40kHz，给看门狗用
 * - LSE：外部低速晶振，32.768kHz，给RTC用
 *
 * 【时钟树路径】
 * HSE(8MHz) → PLL倍频9倍 → SYSCLK(72MHz)
 *   ├── AHB(72MHz) → DMA、GPIO等
 *   ├── APB1(36MHz) → USART2/3、TIM2-7、SPI2/3等
 *   └── APB2(72MHz) → USART1、TIM1、SPI1、ADC、GPIO等
 *
 * 【为什么APB1是36MHz？】
 * APB1最大只支持36MHz，所以需要2分频。APB2支持72MHz，不需要分频。
 */
#include "h_rcc.h"
#include <stm32f10x.h>

/**
 * @brief 初始化系统时钟为72MHz
 *
 * 配置流程：
 * 1. 复位RCC寄存器
 * 2. 开启外部晶振HSE(8MHz)
 * 3. 等待HSE就绪
 * 4. 配置PLL：HSE × 9 = 72MHz
 * 5. 等待PLL就绪
 * 6. 将PLL切换为系统时钟
 */
void rcc_init(void)
{
    ErrorStatus HSEStartUpStatus;

    /* 复位RCC，恢复默认配置 */
    RCC_DeInit();

    /* 开启外部高速晶振HSE */
    RCC_HSEConfig(RCC_HSE_ON);

    /* 等待HSE就绪，如果晶振坏了会一直卡在这里 */
    HSEStartUpStatus = RCC_WaitForHSEStartUp();
    while (HSEStartUpStatus == ERROR)
        ; /* HSE启动失败，死等 */

    /* 配置各总线分频：
     * AHB = SYSCLK / 1 = 72MHz（高速总线，给DMA等用）
     * APB1 = HCLK / 2 = 36MHz（低速外设，最大36MHz）
     * APB2 = HCLK / 1 = 72MHz（高速外设，最大72MHz） */
    RCC_HCLKConfig(RCC_SYSCLK_Div1);
    RCC_PCLK1Config(RCC_HCLK_Div2);
    RCC_PCLK2Config(RCC_HCLK_Div1);

    /* 配置PLL：时钟源=HSE，不分频，倍频9倍
     * 8MHz × 9 = 72MHz */
    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);

    /* 使能PLL */
    RCC_PLLCmd(ENABLE);

    /* 等待PLL就绪 */
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
        ;

    /* 将PLL切换为系统时钟源 */
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

    /* 确认PLL已被选为系统时钟（0x08表示PLL） */
    while (RCC_GetSYSCLKSource() != 0x08)
        ;
}
