/**
 * @file h_servo.h
 * @brief 舵机驱动头文件
 *
 * 【舵机基础知识】
 * 舵机（Servo）是一种可以精确控制角度的电机，常用于机器人和遥控模型。
 *
 * 控制信号：PWM波，周期20ms
 * - 高电平0.5ms → 0°
 * - 高电平1.5ms → 90°（中间位置）
 * - 高电平2.5ms → 180°
 *
 * 脉宽单位：微秒(us)，范围510~2490
 * 默认中间位置：1500us
 *
 * 【本项目舵机配置】
 * 6个舵机，连接在GPIOB的不同引脚上：
 * - 舵机0: PB3    - 舵机1: PB8
 * - 舵机2: PB9    - 舵机3: PB6
 * - 舵机4: PB7    - 舵机5: PB4
 *
 * PWM信号由TIM2中断手动生成，8个时隙（6个实际+2个空位）组成20ms周期。
 */
#ifndef __H_SERVO_H__
#define __H_SERVO_H__
#include "main.h"

/* 舵机引脚配置 */
#define SERVO0_PIN       GPIO_Pin_3   /* 舵机0: PB3 */
#define SERVO0_GPIO_PORT GPIOB
#define SERVO0_GPIO_CLK  RCC_APB2Periph_GPIOB

#define SERVO1_PIN       GPIO_Pin_8   /* 舵机1: PB8 */
#define SERVO1_GPIO_PORT GPIOB
#define SERVO1_GPIO_CLK  RCC_APB2Periph_GPIOB

#define SERVO2_PIN       GPIO_Pin_9   /* 舵机2: PB9 */
#define SERVO2_GPIO_PORT GPIOB
#define SERVO2_GPIO_CLK  RCC_APB2Periph_GPIOB

#define SERVO3_PIN       GPIO_Pin_6   /* 舵机3: PB6 */
#define SERVO3_GPIO_PORT GPIOB
#define SERVO3_GPIO_CLK  RCC_APB2Periph_GPIOB

#define SERVO4_PIN       GPIO_Pin_7   /* 舵机4: PB7 */
#define SERVO4_GPIO_PORT GPIOB
#define SERVO4_GPIO_CLK  RCC_APB2Periph_GPIOB

#define SERVO5_PIN       GPIO_Pin_4   /* 舵机5: PB4 */
#define SERVO5_GPIO_PORT GPIOB
#define SERVO5_GPIO_CLK  RCC_APB2Periph_GPIOB

/* 舵机引脚电平控制宏 */
#define SERVO0_PIN_SET(level) GPIO_WriteBit(SERVO0_GPIO_PORT, SERVO0_PIN, level)
#define SERVO1_PIN_SET(level) GPIO_WriteBit(SERVO1_GPIO_PORT, SERVO1_PIN, level)
#define SERVO2_PIN_SET(level) GPIO_WriteBit(SERVO2_GPIO_PORT, SERVO2_PIN, level)
#define SERVO3_PIN_SET(level) GPIO_WriteBit(SERVO3_GPIO_PORT, SERVO3_PIN, level)
#define SERVO4_PIN_SET(level) GPIO_WriteBit(SERVO4_GPIO_PORT, SERVO4_PIN, level)
#define SERVO5_PIN_SET(level) GPIO_WriteBit(SERVO5_GPIO_PORT, SERVO5_PIN, level)

#ifndef DJ_NUM
#define DJ_NUM 8
#endif

/* 舵机PWM默认范围 */
#define SERVO_PWM_MIN_DEFAULT  510
#define SERVO_PWM_MAX_DEFAULT  2490

/* 4号舵机（腕部角度调节）限位：±90°
 * 换算公式：PWM = 1500 - 2000 * angle / 270
 * -90° → 1500 + 667 ≈ 2167us
 * +90° → 1500 - 667 ≈ 833us */
#define SERVO4_PWM_MIN  833
#define SERVO4_PWM_MAX  2167

/* 每个舵机的独立PWM限位数组（索引0~7对应舵机0~7）
 * 可以给每个舵机设置不同的最小/最大PWM值
 * 例如4号舵机限制在±90°，其他舵机使用默认510~2490 */
extern const int servo_pwm_min[DJ_NUM];
extern const int servo_pwm_max[DJ_NUM];

/**
 * @brief 舵机状态结构体
 * aim: 目标脉宽(us)，如1500表示中间位置
 * time: 运动时间(ms)，从当前位置到目标位置的时间
 * cur: 当前脉宽(us)，浮点数用于平滑插值
 * inc: 每次中断的增量值，用于平滑运动
 */
typedef struct
{
    uint16_t aim;
    uint16_t time;
    float cur;
    float inc;
} servo_t;

/* 舵机状态数组，所有舵机的当前状态 */
extern servo_t duoji_doing[DJ_NUM];

/* 初始化舵机引脚和默认位置 */
void servo_init(void);

/* 设置舵机引脚电平（由TIM2中断调用） */
void servo_pin_set(uint8_t servo_id, BitAction level);

/* 设置舵机引脚电平（servo_pin_set的别名） */
void servo_set(uint8_t index, BitAction level);

/* 设置舵机运动参数
 * @param index 舵机编号(0~5)
 * @param aim 目标脉宽(us)，510~2490，0=停止
 * @param time 运动时间(ms) */
void duoji_doing_set(uint8_t index, int aim, int time);

/* 更新舵机增量偏移（由TIM2中断调用，实现平滑运动） */
void servo_inc_offset(u8 index);

#endif
