/**
 * @file h_timer.c
 * @brief 定时器驱动实现
 *
 * 【SysTick基础知识】
 * SysTick是Cortex-M3内核自带的24位倒计时定时器。
 * 它的特点是：简单、可靠、所有Cortex-M芯片都有。
 * 常用于：操作系统心跳、延时函数、计时。
 *
 * 【TIM2基础知识】
 * TIM2是STM32的通用定时器，有4个通道，可以：
 * - 计时（定时中断）
 * - 输出PWM（控制电机/舵机）
 * - 输入捕获（测量脉冲宽度）
 * 本项目用TIM2的中断来手动生成多路舵机PWM信号。
 *
 * 【舵机PWM原理】
 * 舵机需要20ms周期的PWM信号，高电平时间0.5~2.5ms对应角度0°~180°。
 * TIM2中断中，轮流给8个舵机（6个实际+2个空位）输出高/低电平，
 * 通过改变ARR值来控制高电平时间。
 */
#include "h_timer.h"

/* 毫秒计数器，每1ms加1 */
static u32 systick_us = 0;

/**
 * @brief 初始化SysTick，每1ms中断一次
 *
 * SysTick_Config() 会自动：
 * 1. 设置重装载值 = SystemCoreClock/1000 = 72000
 * 2. 开启SysTick中断
 * 3. 启动倒计时
 * 当倒计时到0时，触发SysTick_Handler()中断
 */
void SysTick_Init(void)
{
    SysTick_Config(SystemCoreClock / 1000); /* 72MHz/1000 = 72000次 = 1ms */
}

/**
 * @brief SysTick中断服务函数，每1ms执行一次
 *
 * 功能：
 * 1. 毫秒计数器+1
 * 2. 每10ms调用一次按键扫描
 */
void SysTick_Handler(void)
{
    systick_us++; /* 毫秒计数+1 */

    static uint32_t key_scan_times = 0;
    key_scan_times++;
    if (key_scan_times >= 10) /* 每10ms扫描一次按键 */
    {
        key_scan_times = 0;
        key_scan(); /* 调用按键扫描函数（在h_key.c中定义） */
    }
}

/**
 * @brief 获取当前毫秒数
 * @return 从开机到现在的毫秒数
 */
u32 millis(void)
{
    return systick_us;
}

/**
 * @brief 初始化定时器2，用于生成舵机PWM信号
 * @param arr 自动重装载值，决定定时周期
 * @param psc 预分频系数，决定计数频率
 *
 * 计算公式：
 * 定时器时钟 = 72MHz / (psc+1) = 72MHz/72 = 1MHz
 * 定时周期 = arr / 1MHz = 2000/1MHz = 2ms
 */
void TIM2_Init(u16 arr, u16 psc)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 使能TIM2时钟（TIM2挂在APB1总线上） */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* 定时器基本配置 */
    TIM_TimeBaseInitStruct.TIM_Prescaler = psc;                  /* 预分频：72-1，时钟=1MHz */
    TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up; /* 向上计数模式 */
    TIM_TimeBaseInitStruct.TIM_Period = arr;                     /* 自动重装载值 */
    TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;     /* 不分频 */
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStruct);

    /* 允许更新中断 */
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    /* 配置NVIC中断优先级 */
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;           /* TIM2中断通道 */
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0; /* 抢占优先级0（最高） */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;        /* 子优先级2 */
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 启动定时器2 */
    TIM_Cmd(TIM2, ENABLE);
}

/**
 * @brief TIM2中断服务函数，生成多路舵机PWM信号
 *
 * 【工作原理】
 * 每次中断，轮流处理一个舵机：
 * - flag=0时：输出高电平，ARR设为舵机脉宽（如1500us→1500个计数）
 * - flag=1时：输出低电平，ARR设为剩余时间（2500-脉宽）
 * 处理完8个舵机为一个完整周期（约20ms）
 *
 * 时序示意（以舵机0为例，脉宽1500us）：
 * ┌────1500us────┐               ┌────1500us────┐
 * │              │               │              │
 * ─┘              └───1000us─────┘              └──
 * ←舵机0高电平→  ←舵机0低电平→
 */
void TIM2_IRQHandler(void)
{
    static u8 flag = 0;          /* 0=输出高电平，1=输出低电平 */
    static u16 duoji_index1 = 0; /* 当前处理的舵机索引 */
    int temp;
    unsigned int cur_pwm;

    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update); /* 清除中断标志 */

        /* 舵机索引循环：0~7，到8后归0 */
        if (duoji_index1 == DJ_NUM)
        {
            duoji_index1 = 0;
        }

        /* 【安全检查】获取当前脉宽值，确保在合理范围内
         * 如果cur值异常（如0或负数），ARR会设为0导致定时器卡死
         * 限制在500~2500范围内，保证ARR值始终大于0 */
        cur_pwm = (unsigned int)(duoji_doing[duoji_index1].cur);
        if (cur_pwm < 500)  cur_pwm = 500;
        if (cur_pwm > 2500) cur_pwm = 2500;

        if (flag == 0)
        {
            /* 高电平阶段：设置ARR为当前舵机的脉宽值 */
            TIM2->ARR = cur_pwm;
            servo_set(duoji_index1, Bit_SET);       /* 舵机引脚输出高电平 */
            servo_inc_offset(duoji_index1);          /* 更新舵机增量 */
        }
        else
        {
            /* 低电平阶段：ARR = 2500 - 脉宽值 */
            temp = 2500 - cur_pwm;
            if (temp < 100) temp = 100;  /* 【安全检查】确保低电平时间不为0 */
            TIM2->ARR = temp;
            servo_pin_set(duoji_index1, Bit_RESET);  /* 舵机引脚输出低电平 */
            duoji_index1++;                           /* 切换到下一个舵机 */
        }
        flag = !flag; /* 高低电平交替 */
    }
}
