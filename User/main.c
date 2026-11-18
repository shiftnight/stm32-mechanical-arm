/**
 * @file main.c
 * @brief 主程序入口
 *
 * 【程序执行流程】
 * 1. 上电后，启动文件(startup_stm32f10x_md.s)自动调用main()
 * 2. main()依次调用各模块的初始化函数
 * 3. 进入while(1)主循环，不断执行LED、按键、串口任务
 *
 * 【同时运行的中断】
 * - SysTick中断（每1ms）：计时 + 按键扫描
 * - TIM2中断：生成舵机PWM信号
 * - USART1/3中断：接收串口数据
 */
#include "main.h"

/**
 * @brief 主函数，程序入口
 *
 * 【初始化顺序的黄金法则】
 * 舵机相关的初始化必须按以下顺序，缺一不可：
 * 1. 先设置 duoji_doing[] 的值（确保cur不为0）
 * 2. 从Flash读取上次保存的舵机位置，覆盖cur值
 * 3. 配置GPIO为输出（此时输出高电平）
 * 4. 立即启动TIM2（输出PWM信号，基于cur值）
 * 5. 等500ms让舵机稳定
 *
 * 【防猛动机制】
 * 关机前舵机位置会保存到Flash。开机时先读取保存的位置设为cur，
 * TIM2输出的PWM就与舵机实际位置一致，舵机不会猛然跳动。
 * 然后duoji_doing_set()会让舵机平滑地移到目标位置。
 */
int main(void)
{
    u8 i;

    rcc_init();

    SWJ_gpio_init();

    SysTick_Init();

    for (i = 0; i < DJ_NUM; i++)
    {
        duoji_doing[i].aim = 1500;
        duoji_doing[i].time = 5000;
        duoji_doing[i].cur = 1500;
        duoji_doing[i].inc = 0;
    }

    app_gpio_init();

    app_setup_start();

    app_usart_init();

    spi_flash_init();

    u16 num = spi_flash_read_id();
    UsartPrintf(USART_DEBUG, "W25Q64 ID=%x\r\n", num);

    other_init();

    servo_init();
    TIM2_Init(2000, 72 - 1);

    Delay_ms(500);

    app_ps2_init();

    set_kinematics(100.0f, 105.0f, 88.0f, 155.0f, &kinematics);

    while (1)
    {
        app_led_run();
        app_key_run();
        app_usart_run();
        loop_action();
        app_ps2_run();
        app_servo_save_pos_check();
    }
}

/**
 * @brief 禁用JTAG/SWD调试接口，释放引脚给舵机使用
 */
void SWJ_gpio_init(void)
{
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_GPIOB |
        RCC_APB2Periph_AFIO,
        ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
}

/**
 * @brief 从Flash读取配置信息
 *
 * 【安全设计】
 * 1. 偏差值限制在-500~+500，超出范围清零
 * 2. 初始位置限制在每个舵机的独立限位范围内（servo_pwm_min/max）
 * 3. 4号舵机限制在±90°（833~2167us），其他舵机510~2490us
 * 4. 预命令只执行$开头的命令，防止执行恶意数据
 */
void other_init(void)
{
    uint8_t i = 0;
    int init_pos;

    spi_flash_on(1);
    Delay_ms(10);

    w25x_read((u8 *)(&eeprom_info), W25Q64_INFO_ADDR_SAVE_STR, sizeof(eeprom_info_t));

    if (eeprom_info.version != FLAG_VERIFY)
    {
        eeprom_info.version = FLAG_VERIFY;
        eeprom_info.dj_record_num = 0;
    }

    if (eeprom_info.dj_bias_pwm[DJ_NUM] != FLAG_VERIFY)
    {
        for (i = 0; i < DJ_NUM; i++)
        {
            eeprom_info.dj_bias_pwm[i] = 0;
        }
        eeprom_info.dj_bias_pwm[DJ_NUM] = FLAG_VERIFY;
    }

    for (i = 0; i < DJ_NUM; i++)
    {
        if (eeprom_info.dj_bias_pwm[i] > 500 || eeprom_info.dj_bias_pwm[i] < -500)
        {
            eeprom_info.dj_bias_pwm[i] = 0;
        }
    }

    if (eeprom_info.last_pos[DJ_NUM] == FLAG_VERIFY)
    {
        for (i = 0; i < DJ_NUM; i++)
        {
            if (eeprom_info.last_pos[i] >= servo_pwm_min[i] &&
                eeprom_info.last_pos[i] <= servo_pwm_max[i])
            {
                duoji_doing[i].cur = (float)eeprom_info.last_pos[i];
                duoji_doing[i].aim = eeprom_info.last_pos[i];
            }
        }
    }
    else
    {
        for (i = 0; i < DJ_NUM; i++)
        {
            eeprom_info.last_pos[i] = 1500;
        }
        eeprom_info.last_pos[DJ_NUM] = FLAG_VERIFY;
    }

    spi_flash_on(0);

    for (i = 0; i < DJ_NUM; i++)
    {
        init_pos = 1500 + eeprom_info.dj_bias_pwm[i];
        if (init_pos > servo_pwm_max[i]) init_pos = servo_pwm_max[i];
        if (init_pos < servo_pwm_min[i]) init_pos = servo_pwm_min[i];

        duoji_doing_set(i, init_pos, 1000);
    }

    if (eeprom_info.pre_cmd[PRE_CMD_SIZE] == FLAG_VERIFY)
    {
        if (eeprom_info.pre_cmd[0] == '$')
        {
            parse_cmd(eeprom_info.pre_cmd);
        }
    }
}
