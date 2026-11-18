/**
 * @file h_kinematics.h
 * @brief 机械臂逆运动学头文件
 *
 * 【什么是逆运动学？】
 * 逆运动学就是：已知末端（爪子）的目标位置(x,y,z)，
 * 反推每个关节需要转动多少角度。
 *
 * 就像你要伸手拿桌上的杯子：
 * - 大脑知道杯子在哪（目标位置）
 * - 需要计算肩膀、肘部、手腕各转多少度（关节角度）
 * - 这就是逆运动学
 *
 * 【机械臂结构】
 * 本项目是4自由度机械臂：
 * - 0号舵机：底座旋转（水平面内转动）
 * - 1号舵机：肩关节（上下摆动）
 * - 2号舵机：肘关节（上下摆动）
 * - 3号舵机：腕关节（爪子角度）
 *
 * 四段臂长：L0(底座高度)、L1(大臂)、L2(小臂)、L3(爪子)
 */
#ifndef __H_KINEMATICS_H__
#define __H_KINEMATICS_H__
#include "main.h"

/**
 * @brief 运动学参数结构体
 *
 * L0~L3: 四段臂的长度（单位：10mm，代码中放大了10倍）
 * servo_angle[6]: 6个舵机的角度（度）
 * servo_range[6]: 6个舵机的角度范围
 * servo_pwm[6]: 6个舵机的PWM值（1500=中间，1000~2000）
 */
typedef struct
{
    float L0;
    float L1;
    float L2;
    float L3;
    float servo_angle[6];
    float servo_range[6];
    int servo_pwm[6];
} Kinematics_t;

extern Kinematics_t kinematics;

/**
 * @brief 设置四个关节的长度
 * @param L0 底座高度(mm)
 * @param L1 大臂长度(mm)
 * @param L2 小臂长度(mm)
 * @param L3 爪子长度(mm)
 * @param kinematics 运动学结构体指针
 *
 * 函数内部会将长度放大10倍（单位变为10mm）
 */
void set_kinematics(float L0, float L1, float L2, float L3, Kinematics_t *kinematics);

/**
 * @brief 逆运动学分析：根据目标坐标计算各舵机角度和PWM
 * @param x 目标X坐标(mm)
 * @param y 目标Y坐标(mm)
 * @param z 目标Z坐标，距离地面的高度(mm)
 * @param Alpha 爪子与水平面的夹角(度)，-25~-65范围比较好
 * @param kinematics 运动学结构体指针
 * @return 0=成功，其他值=失败（超出范围）
 *
 * 成功后，结果存储在kinematics->servo_angle[]和servo_pwm[]中
 */
int kinematics_analysis(float x, float y, float z, float Alpha, Kinematics_t *kinematics);

#endif
