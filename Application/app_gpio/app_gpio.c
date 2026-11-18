/**
 * @file app_gpio.c
 * @brief 应用层GPIO实现
 *
 * 本文件实现了LED、蜂鸣器、按键的应用层逻辑。
 * 这些函数在main()的主循环中被调用。
 */
#include "app_gpio.h"

/**
 * @brief 初始化所有GPIO设备
 * 依次初始化LED、蜂鸣器、按键，并设为默认关闭状态
 */
void app_gpio_init(void)
{
    led_init();   /* LED引脚初始化(PB13) */
    beep_init();  /* 蜂鸣器引脚初始化(PB5) */
    key_init();   /* 按键引脚初始化(PA8/PA11) */
    LED_OFF();    /* 默认LED熄灭 */
    BEEP_OFF();   /* 默认蜂鸣器关闭 */
}

/**
 * @brief LED闪烁任务
 * 每1000ms翻转一次LED状态，形成1Hz闪烁（心跳指示）
 *
 * 使用millis()非阻塞计时，不会阻塞主循环。
 * 原理：记录上次翻转时间，如果超过1000ms就翻转。
 */
void app_led_run(void)
{
    static u32 time_count = 0;
    if (millis() - time_count < 1000) /* 还没到1秒 */
    {
        return;
    }
    time_count = millis(); /* 记录本次翻转时间 */
    LED_TOGGLE;            /* 翻转LED状态 */
}

/**
 * @brief 开机启动信号
 * LED和蜂鸣器交替响3声，表示系统正常启动
 */
void app_setup_start(void)
{
    u8 i;
    for (i = 0; i < 3; i++)
    {
        BEEP_ON();
        LED_ON();
        Delay_ms(100);
        BEEP_OFF();
        LED_OFF();
        Delay_ms(100);
    }
}

/**
 * @brief 按键处理任务
 * 根据按键状态执行相应操作：
 * - KEY1短按：打开SPI引脚复用，读写Flash数据
 * - KEY1长按：打印长按信息
 * - KEY2短按：关闭SPI引脚复用，恢复LED功能
 * - KEY2长按：打印长按信息
 */
void app_key_run(void)
{
    u8 a, b;

    if (key1_pressing != 0)
    {
        if ((key1_pressing == 1) && (key1_pressing != 2))
        {
            /* KEY1短按：读写Flash测试 */
            spi_flash_on(1);              /* 切换PB13为SPI功能 */
            a = 0;
            b = 0;
            a = spi_flash_read_byte(0);   /* 读取Flash地址0的数据 */
            a++;
            b++;
            UsartPrintf(USART_DEBUG, "a=%d,b=%d\n", a, b);
            spi_flash_erase_sector(0);    /* 擦除扇区0 */
            spi_flash_write_byte(a, 0);   /* 写入数据到地址0 */
            key1_pressing = 0;            /* 手动清零，防止重复处理 */
            UsartPrintf(USART_DEBUG, "key1_pressing=1\n");
            app_setup_start();            /* 鸣叫提示 */
            key1_pressing = 0;
        }
        else if (key1_pressing == 2)
        {
            /* KEY1长按 */
            UsartPrintf(USART_DEBUG, "key1_pressing=2\n");
        }
    }

    if (key2_pressing != 0)
    {
        if (key2_pressing == 1)
        {
            /* KEY2短按：切换PB13为LED功能 */
            spi_flash_on(0);              /* 切换PB13为LED功能 */
            key2_pressing = 0;
            UsartPrintf(USART_DEBUG, "key2_pressing=1\n");
        }
        else if (key2_pressing == 2)
        {
            /* KEY2长按 */
            UsartPrintf(USART_DEBUG, "key2_pressing=2\n");
        }
    }
}
