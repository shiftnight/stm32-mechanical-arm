/**
 * @file h_kinematics.c
 * @brief 机械臂逆运动学算法实现
 *
 * 【逆运动学算法原理】
 * 已知爪子的目标位置(x,y,z)和爪子与水平面的夹角Alpha，
 * 求解4个舵机需要转动的角度。
 *
 * 【求解步骤】
 * 1. 根据x,y计算底座旋转角theta6（0号舵机）
 * 2. 将3D坐标投影到2D平面（只看侧面）
 * 3. 减去爪子长度的影响，得到肩关节的等效目标位置
 * 4. 用余弦定理求肩关节角theta5（1号舵机）
 * 5. 用余弦定理求肘关节角theta4（2号舵机）
 * 6. 由Alpha、theta4、theta5推算腕关节角theta3（3号舵机）
 * 7. 将角度转换为舵机PWM值
 *
 * 【角度到PWM的转换公式】
 * PWM = 1500 - 2000 * angle / 270
 * 其中1500是中间位置，2000/270是每度对应的PWM增量
 */
#include "h_kinematics.h"

#define PI 3.1415926f

Kinematics_t kinematics;

/**
 * @brief 设置四个关节的长度
 * @param L0 底座高度(mm)
 * @param L1 大臂长度(mm)
 * @param L2 小臂长度(mm)
 * @param L3 爪子长度(mm)
 * @param kinematics 运动学结构体指针
 *
 * 函数内部将长度放大10倍，方便整数运算
 * 例如：L0=100mm -> kinematics->L0=1000（单位变为0.1mm）
 */
void set_kinematics(float L0, float L1, float L2, float L3, Kinematics_t *kinematics)
{
    kinematics->L0 = L0 * 10.0f;
    kinematics->L1 = L1 * 10.0f;
    kinematics->L2 = L2 * 10.0f;
    kinematics->L3 = L3 * 10.0f;
}

/**
 * @brief 逆运动学分析
 * @param x 目标X坐标(mm)
 * @param y 目标Y坐标(mm)
 * @param z 目标Z坐标，距离地面的高度(mm)
 * @param Alpha 爪子与水平面的夹角(度)
 * @param kinematics 运动学结构体指针
 * @return 0=成功，其他值=失败原因
 *
 * 返回值含义：
 * - 0: 成功
 * - -1: z坐标太低（低于底座）
 * - 2: 目标太远，超出臂长范围
 * - 3: 肘关节角度计算异常（acos参数超出[-1,1]）
 * - 4: 肘关节角度超出[-135,135]范围
 * - 5: 肩关节角度计算异常
 * - 6: 肩关节角度超出[0,180]范围
 * - 7: 腕关节角度超出[-90,90]范围
 */
int kinematics_analysis(float x, float y, float z, float Alpha, Kinematics_t *kinematics)
{
    float theta3, theta4, theta5, theta6;
    float l0, l1, l2, l3;
    float aaa, bbb, ccc, zf_flag;

    x = x * 10.0f;
    y = y * 10.0f;
    z = z * 10.0f;

    l0 = kinematics->L0;
    l1 = kinematics->L1;
    l2 = kinematics->L2;
    l3 = kinematics->L3;

    if (x == 0)
    {
        theta6 = 0.0f;
    }
    else
    {
        theta6 = atan2(y, x) * 270.0f / PI;
    }

    y = sqrt(x * x + y * y);
    y = y - l3 * cos(Alpha * PI / 180.0f);
    z = z - l0 - l3 * sin(Alpha * PI / 180.0f);

    if (z < -l0)
    {
        return -1;
    }

    if (sqrt(x * x + y * y) > (l1 + l2))
    {
        return 2;
    }

    float dist_yz = sqrt(z * z + y * y);
    if (dist_yz < 0.001f)
    {
        return 5;
    }

    ccc = acos(y / dist_yz);
    bbb = (y * y + z * z - l1 * l1 - l2 * l2) / (2 * l1 * dist_yz);

    if (bbb > 1.0f || bbb < -1.0f)
    {
        return 5;
    }

    if (z < 0)
    {
        zf_flag = -1;
    }
    else
    {
        zf_flag = 1;
    }

    theta5 = ccc * zf_flag + acos(bbb);
    theta5 = theta5 * 180.0f / PI;

    if (theta5 > 180.0f || theta5 < 0)
    {
        return 6;
    }

    aaa = -(y * y + z * z - l1 * l1 - l2 * l2) / (2 * l1 * l2);

    if (aaa > 1.0f || aaa < -1.0f)
    {
        return 3;
    }

    theta4 = acos(aaa);
    theta4 = 180 - theta4 * 180.0f / PI;

    if (theta4 > 135 || theta4 < -135)
    {
        return 4;
    }

    theta3 = Alpha + theta4 - theta5;

    if (theta3 > 90 || theta3 < -90)
    {
        return 7;
    }

    kinematics->servo_angle[0] = theta6;
    kinematics->servo_angle[1] = theta5 - 90;
    kinematics->servo_angle[2] = theta4;
    kinematics->servo_angle[3] = theta3;

    kinematics->servo_pwm[0] = (int)(1500 - 2000.0f * kinematics->servo_angle[0] / 270.0f);
    kinematics->servo_pwm[1] = (int)(1500 - 2000.0f * kinematics->servo_angle[1] / 270.0f);
    kinematics->servo_pwm[2] = (int)(1500 - 2000.0f * kinematics->servo_angle[2] / 270.0f);
    kinematics->servo_pwm[3] = (int)(1500 - 2000.0f * kinematics->servo_angle[3] / 270.0f);

    return 0;
}
