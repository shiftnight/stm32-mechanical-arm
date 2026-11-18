/**
 * @file h_servo.c
 * @brief 舵机驱动实现
 *
 * 【舵机平滑运动原理】
 * 如果直接设置目标位置，舵机会瞬间转到目标，动作很生硬。
 * 通过增量插值，让舵机逐步移动到目标位置：
 *
 * 增量 = (目标位置 - 当前位置) / (运动时间 / 20ms)
 * 每20ms，当前位置 += 增量
 *
 * 这样舵机就会匀速运动到目标位置，动作更流畅。
 *
 * 【开机安全机制】
 * 舵机引脚在配置为输出之前，必须先设置ODR寄存器为高电平。
 * 否则引脚默认输出低电平，舵机会收到0%占空比的PWM， 
 * 导致舵机猛转到极限位置，可能损坏机械臂！
 */
#include "h_servo.h"

/* 舵机状态数组，全局变量默认初始化为0 */
servo_t duoji_doing[DJ_NUM];

/* 每个舵机的独立PWM限位
 * 4号舵机限制在±90°（833~2167us），其他舵机使用默认510~2490us
 * 如果需要给其他舵机也加限位，修改对应索引的值即可 */
const int servo_pwm_min[DJ_NUM] = {
    SERVO_PWM_MIN_DEFAULT, /* 舵机0: 底座旋转 */
    SERVO_PWM_MIN_DEFAULT, /* 舵机1: 肩关节 */
    SERVO_PWM_MIN_DEFAULT, /* 舵机2: 肘关节 */
    SERVO_PWM_MIN_DEFAULT, /* 舵机3: 腕关节 */
    SERVO4_PWM_MIN,        /* 舵机4: 腕部角度调节，±90°限制 */
    SERVO_PWM_MIN_DEFAULT, /* 舵机5: 爪子开合 */
    SERVO_PWM_MIN_DEFAULT, /* 舵机6: 空位 */
    SERVO_PWM_MIN_DEFAULT  /* 舵机7: 空位 */
};
const int servo_pwm_max[DJ_NUM] = {
    SERVO_PWM_MAX_DEFAULT, /* 舵机0: 底座旋转 */
    SERVO_PWM_MAX_DEFAULT, /* 舵机1: 肩关节 */
    SERVO_PWM_MAX_DEFAULT, /* 舵机2: 肘关节 */
    SERVO_PWM_MAX_DEFAULT, /* 舵机3: 腕关节 */
    SERVO4_PWM_MAX,        /* 舵机4: 腕部角度调节，±90°限制 */
    SERVO_PWM_MAX_DEFAULT, /* 舵机5: 爪子开合 */
    SERVO_PWM_MAX_DEFAULT, /* 舵机6: 空位 */
    SERVO_PWM_MAX_DEFAULT  /* 舵机7: 空位 */
};

/**
 * @brief 初始化舵机引脚和默认位置
 *
 * 【关键安全措施】
 * 必须在配置引脚为输出模式之前，先将ODR寄存器设为高电平！
 * 这样引脚切换为输出的瞬间就是高电平，舵机不会收到错误信号。
 *
 * 原因：GPIO默认ODR=0，如果先配置为输出再设高电平，
 * 会有一个短暂的低电平脉冲，舵机可能误动作。
 */
void servo_init(void)
{
    RCC_APB2PeriphClockCmd(SERVO0_GPIO_CLK, ENABLE);

    /* 【安全措施】先设置所有舵机引脚的ODR为高电平
     * 这样配置为推挽输出时，引脚立即输出高电平
     * 舵机收到高电平 = "无PWM信号"，不会乱动 */
    GPIO_SetBits(SERVO0_GPIO_PORT, SERVO0_PIN | SERVO1_PIN | SERVO2_PIN |
                                     SERVO3_PIN | SERVO4_PIN | SERVO5_PIN);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Pin = SERVO0_PIN;
    GPIO_Init(SERVO0_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = SERVO1_PIN;
    GPIO_Init(SERVO1_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = SERVO2_PIN;
    GPIO_Init(SERVO2_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = SERVO3_PIN;
    GPIO_Init(SERVO3_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = SERVO4_PIN;
    GPIO_Init(SERVO4_GPIO_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = SERVO5_PIN;
    GPIO_Init(SERVO5_GPIO_PORT, &GPIO_InitStructure);

    /* 注意：不再在这里初始化duoji_doing[]
     * 因为main()中已经提前初始化了，这里再初始化会覆盖偏差值 */
}

/**
 * @brief 设置指定舵机引脚的电平
 * @param index 舵机编号(0~5)
 * @param level 电平值：Bit_SET=高电平，Bit_RESET=低电平
 */
void servo_pin_set(uint8_t index, BitAction level)
{
    switch (index)
    {
    case 0: SERVO0_PIN_SET(level); break;
    case 1: SERVO1_PIN_SET(level); break;
    case 2: SERVO2_PIN_SET(level); break;
    case 3: SERVO3_PIN_SET(level); break;
    case 4: SERVO4_PIN_SET(level); break;
    case 5: SERVO5_PIN_SET(level); break;
    default: break;
    }
}

/**
 * @brief servo_pin_set的别名，供TIM2中断调用
 */
void servo_set(uint8_t index, BitAction level)
{
    servo_pin_set(index, level);
}

/**
 * @brief 设置舵机运动参数
 * @param index 舵机编号(0~5)
 * @param aim 目标脉宽(us)，0=停止
 * @param time 运动时间(ms)
 *
 * 【限位机制】
 * 每个舵机有独立的PWM范围限制（servo_pwm_min/servo_pwm_max），
 * 超出范围的aim值会被自动裁剪到限位范围内。
 * 例如4号舵机限制在±90°（833~2167us），防止腕部过度旋转。
 */
void duoji_doing_set(uint8_t index, int aim, int time)
{
    float diff;
    
    if (index >= DJ_NUM)
        return;

    if (aim == 0)
    {
        duoji_doing[index].inc = 0;
        duoji_doing[index].aim = duoji_doing[index].cur;
        return;
    }

    if (aim > servo_pwm_max[index]) aim = servo_pwm_max[index];
    else if (aim < servo_pwm_min[index]) aim = servo_pwm_min[index];

    if (time >= 10000) time = 10000;

    diff = duoji_doing[index].cur - aim;
    if (diff < 0) diff = -diff;
    
    if (diff < 1.0f)
    {
        duoji_doing[index].aim = aim;
        duoji_doing[index].cur = (float)aim;
        duoji_doing[index].inc = 0;
        return;
    }

    if (time < 20)
    {
        duoji_doing[index].aim = aim;
        duoji_doing[index].cur = (float)aim;
        duoji_doing[index].inc = 0;
    }
    else
    {
        duoji_doing[index].aim = aim;
        duoji_doing[index].time = time;
        duoji_doing[index].inc = ((float)aim - duoji_doing[index].cur) / ((float)time / 20.0f);
    }
}

/**
 * @brief 更新舵机增量偏移（由TIM2中断调用）
 * @param index 舵机编号
 *
 * 【限位保护】
 * 增量更新时也检查aim是否超出该舵机的独立限位范围，
 * 防止因浮点运算漂移导致位置越界。
 */
void servo_inc_offset(u8 index)
{
    int aim_temp;
    float diff;
    
    if (duoji_doing[index].inc != 0)
    {
        aim_temp = duoji_doing[index].aim;
        if (aim_temp > servo_pwm_max[index]) aim_temp = servo_pwm_max[index];
        else if (aim_temp < servo_pwm_min[index]) aim_temp = servo_pwm_min[index];

        diff = duoji_doing[index].cur - aim_temp;
        if (diff < 0) diff = -diff;
        
        if (diff < 1.0f)
        {
            duoji_doing[index].cur = (float)aim_temp;
            duoji_doing[index].inc = 0;
        }
        else
        {
            duoji_doing[index].cur += duoji_doing[index].inc;
        }
    }
}
