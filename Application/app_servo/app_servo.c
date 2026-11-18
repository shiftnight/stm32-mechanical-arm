/**
 * @file app_servo.c
 * @brief 舵机动作组管理实现
 *
 * 【本文件的功能】
 * 1. 解析串口发来的舵机控制指令（#001P1500T1000!格式）
 * 2. 管理舵机偏差校准（每个舵机可能有机械偏差，需要补偿）
 * 3. 动作组的存储和回放（将一系列动作存入Flash，按顺序执行）
 *
 * 【动作组执行流程】
 * 串口发送动作组指令 -> parse_action()解析 -> duoji_doing_set()设置舵机目标
 * -> TIM2中断自动驱动舵机平滑运动 -> 运动完成后执行下一个动作
 *
 * 【偏差校准原理】
 * 由于机械装配误差，同一PWM值下不同舵机的实际角度可能不同。
 * 通过偏差值补偿：实际PWM = 指令PWM + 偏差值
 * 偏差值存储在Flash中，掉电不丢失。
 */
#include "app_servo.h"

u8 cmd_return[CMD_RETURN_SIZE];

eeprom_info_t eeprom_info;

u8 group_do_ok = 1;

u8 AI_mode = 255;

int do_start_index;
int do_time = 0;
int group_num_start;
int group_num_end;
int group_num_times;
int do_end_index;
u32 action_time = 0;

/**
 * @brief 发送串口指令（同时发送到USART1和USART3）
 * @param str 要发送的字符串
 *
 * 发送前设置uart1_get_ok=1，防止中断接收覆盖缓冲区
 * 发送后恢复uart1_get_ok=0，允许继续接收
 */
void zx_uart_send_send(u8 *str)
{
    uart1_get_ok = 1;
    Usart_SendString(USART1, str);
    Usart_SendString(USART3, str);
    uart1_get_ok = 0;
}

/**
 * @brief 解析舵机控制指令
 * @param uart_receive_buf 接收缓冲区中的指令字符串
 *
 * 【支持的指令格式】
 *
 * 1. 舵机运动指令：#001P1500T1000!
 *    解析过程：
 *    - 找到'#' -> 开始解析舵机编号
 *    - 找到'P' -> 开始解析目标脉宽
 *    - 找到'T' -> 开始解析运动时间
 *    - 找到'!' -> 指令结束，执行运动
 *
 *    解析结果：
 *    - index = 1（舵机编号）
 *    - pwm = 1500（目标脉宽，单位us）
 *    - time = 1000（运动时间，单位ms）
 *    然后调用 duoji_doing_set(1, 1500, 1000) 设置舵机运动
 *
 * 2. 偏差调整指令：#001PSCK+050! 或 #001PSCK-050!
 *    格式：#NNNPSCK±BBB!
 *    - NNN = 舵机编号
 *    - PSCK = 偏差调整标记
 *    - ± = 偏差方向（+或-）
 *    - BBB = 偏差值（0~500）
 *
 * 3. 舵机停止指令：#NNNPDST!
 *    - NNN = 舵机编号（255表示所有舵机）
 *    - PDST = 停止标记
 */
void parse_action(u8 *uart_receive_buf)
{
    u16 index, time, i = 0;
    int bias, len;
    int delta, sign, j;
    float pwm;

    zx_uart_send_send(uart_receive_buf);

    if (uart_receive_buf[0] == '#' &&
        uart_receive_buf[4] == 'P' &&
        uart_receive_buf[5] == 'S' &&
        uart_receive_buf[6] == 'C' &&
        uart_receive_buf[7] == 'K' &&
        uart_receive_buf[12] == '!')
    {
        index = (uart_receive_buf[1] - '0') * 100 +
                (uart_receive_buf[2] - '0') * 10 +
                (uart_receive_buf[3] - '0');

        bias = (uart_receive_buf[9] - '0') * 100 +
               (uart_receive_buf[10] - '0') * 10 +
               (uart_receive_buf[11] - '0');

        if ((bias >= -500) && (bias <= 500) && (index < DJ_NUM))
        {
            if (uart_receive_buf[8] == '+')
            {
                duoji_doing[index].cur = duoji_doing[index].cur -
                                         eeprom_info.dj_bias_pwm[index] + bias;
                eeprom_info.dj_bias_pwm[index] = bias;
            }
            else if (uart_receive_buf[8] == '-')
            {
                duoji_doing[index].cur = duoji_doing[index].cur +
                                         eeprom_info.dj_bias_pwm[index] - bias;
                eeprom_info.dj_bias_pwm[index] = -bias;
            }
            duoji_doing[index].aim = duoji_doing[index].cur;
            duoji_doing[index].inc = 0.001;
            rewrite_eeprom();
        }
        return;
    }

    if (uart_receive_buf[0] == '#' &&
        uart_receive_buf[4] == 'P' &&
        uart_receive_buf[5] == 'D' &&
        uart_receive_buf[6] == 'S' &&
        uart_receive_buf[7] == 'T' &&
        uart_receive_buf[8] == '!')
    {
        index = (uart_receive_buf[1] - '0') * 100 +
                (uart_receive_buf[2] - '0') * 10 +
                (uart_receive_buf[3] - '0');
        if (index < DJ_NUM)
        {
            duoji_doing[index].inc = 0.0;
            duoji_doing[index].aim = duoji_doing[index].cur;
        }
        else if (index == 255)
        {
            for (index = 0; index < DJ_NUM; index++)
            {
                duoji_doing[index].inc = 0.0;
                duoji_doing[index].aim = duoji_doing[index].cur;
            }
        }
        return;
    }

    /* PR命令：相对定位（增量移动）
     * 格式：#NNNPR+DDDDTMMMM! 或 #NNNPR-DDDDTMMMM!
     * NNN = 舵机编号
     * PR = Position Relative（相对定位标记）
     * +DDDD / -DDDD = 从当前位置增加/减少的PWM值(us)
     * TMMMM = 运动时间(ms)
     *
     * 【与绝对定位的区别】
     * 绝对定位 #004P1500T1000! → 舵机4转到1500us
     * 相对定位 #004PR+0500T1000! → 舵机4从当前位置增加500us
     *
     * 【用途】
     * PS2手柄控制时，按住按键舵机持续往一个方向移动，
     * 松开按键舵机停止，而不是只能跑到固定位置。
     */
    if (uart_receive_buf[0] == '#' &&
        uart_receive_buf[4] == 'P' &&
        uart_receive_buf[5] == 'R')
    {
        index = (uart_receive_buf[1] - '0') * 100 +
                (uart_receive_buf[2] - '0') * 10 +
                (uart_receive_buf[3] - '0');

        delta = 0;
        sign = 1;
        j = 6;

        if (uart_receive_buf[j] == '-')
        {
            sign = -1;
            j++;
        }
        else if (uart_receive_buf[j] == '+')
        {
            j++;
        }

        while (uart_receive_buf[j] && uart_receive_buf[j] != 'T')
        {
            delta = delta * 10 + (uart_receive_buf[j] - '0');
            j++;
        }
        delta *= sign;

        time = 0;
        if (uart_receive_buf[j] == 'T')
        {
            j++;
            while (uart_receive_buf[j] && uart_receive_buf[j] != '!')
            {
                time = time * 10 + (uart_receive_buf[j] - '0');
                j++;
            }
        }

        if (index < DJ_NUM)
        {
            int new_aim = (int)duoji_doing[index].cur + delta;
            duoji_doing_set(index, new_aim, time);
        }
        return;
    }

    len = strlen((char *)uart_receive_buf);
    while (uart_receive_buf[i] && (i <= len))
    {
        if (uart_receive_buf[i] == '#')
        {
            index = 0;
            i++;
            while (uart_receive_buf[i] && (uart_receive_buf[i] != 'P'))
            {
                index = index * 10 + (uart_receive_buf[i] - '0');
                i++;
            }
        }
        else if (uart_receive_buf[i] == 'P')
        {
            pwm = 0;
            i++;
            while (uart_receive_buf[i] && (uart_receive_buf[i] != 'T'))
            {
                pwm = pwm * 10 + (uart_receive_buf[i] - '0');
                i++;
            }
        }
        else if (uart_receive_buf[i] == 'T')
        {
            time = 0;
            i++;
            while (uart_receive_buf[i] && (uart_receive_buf[i] != '!'))
            {
                time = time * 10 + (uart_receive_buf[i] - '0');
                i++;
            }
            pwm += eeprom_info.dj_bias_pwm[index];
            duoji_doing_set(index, pwm, time);
        }
        else
        {
            i++;
        }
    }
}

/**
 * @brief 动作组保存函数
 *
 * 只有用<>包含的字符串才能在此函数中进行解析
 * 格式：<G0000#000P1500T1000!#001P1500T1000!B000!>
 * 用尖括号括起来，带有组序号
 *
 * @param str 接收到的动作组数据字符串
 */
void save_action(u8 *str)
{
    s32 action_index = 0;
    group_do_ok = 1;

    spi_flash_on(1);
    Delay_ms(10);

    if (str[1] == '$' && str[2] == '!')
    {
        eeprom_info.pre_cmd[PRE_CMD_SIZE] = 0;
        rewrite_eeprom();
        zx_uart_send_send((u8 *)"@CLEAR PRE_CMD OK");
        spi_flash_on(0);
        return;
    }
    else if (str[1] == '$')
    {
        memset(eeprom_info.pre_cmd, 0, sizeof(eeprom_info.pre_cmd));
        strcpy((char *)eeprom_info.pre_cmd, (char *)str + 1);
        eeprom_info.pre_cmd[strlen((char *)str + 1) - 2] = '\0';
        eeprom_info.pre_cmd[PRE_CMD_SIZE] = FLAG_VERIFY;
        rewrite_eeprom();
        zx_uart_send_send((u8 *)"@SET_ACTION_INDEX OK");
        zx_uart_send_send((u8 *)eeprom_info.pre_cmd);
        spi_flash_on(0);
        return;
    }

    action_index = (str[2] - '0') * 1000 + (str[3] - '0') * 100 + (str[4] - '0') * 10 + (str[5] - '0');

    if (action_index == -1 || str[6] != '#')
    {
        UsartPrintf(USART_DEBUG, "ERROR:action_index error");
        spi_flash_on(0);
        return;
    }

    if ((action_index * ACTION_SIZE % 4096) == 0)
    {
        w25x_eraseSector(action_index * ACTION_SIZE / 4096);
    }

    str_replace(str, '<', '[');
    str_replace(str, ']', '>');
    w25x_write(str, action_index * ACTION_SIZE, strlen((char *)str) + 1);

    Usart_SendString(USART1, (u8 *)"A");
    Usart_SendString(USART3, (u8 *)"A");
    spi_flash_on(0);
}

/**
 * @brief 命令解析函数
 * @param cmd 命令字符串
 *
 * 支持的命令：
 * - $DST!      : 所有舵机停止
 * - $DST:x!    : 第x个舵机停止
 * - $RST!      : 单片机重启
 * - $PTG:x-y!  : 获取第x到y组动作
 * - $DGS:x!    : 执行第x个动作
 * - $DGT:x-y,z!: 执行第x到y组动作z次
 * - $DJR!      : 所有舵机复位
 * - $GETA!     : 获取应答信号
 * - $BEEP!     : 蜂鸣器鸣叫
 */
void parse_cmd(u8 *cmd)
{
    int pos, i, index, int1, int2, int3;
    float kinematics_x = 0.0f, kinematics_y = 0.0f, kinematics_z = 0.0f;

    if (pos = str_contain_str(cmd, (u8 *)"DRS!"), pos)
    {
        UsartPrintf(USART_DEBUG, "hello,world!");
    }
    else if (pos = str_contain_str(cmd, (u8 *)"KMS"), pos)
    {
        if (sscanf((char *)cmd, "KMS:%f,%f,%f,%d!", &kinematics_x, &kinematics_y, &kinematics_z, &int3))
        {
            UsartPrintf(USART_DEBUG, "Try to find best pos\r\n");
            if (!kinematics_move(kinematics_x, kinematics_y, kinematics_z, int3))
            {
                UsartPrintf(USART_DEBUG, "Can't find best pos!");
            }
        }
    }
    else if (pos = str_contain_str(cmd, (u8 *)"DST!"), pos)
    {
        group_do_ok = 1;
        for (i = 0; i < DJ_NUM; i++)
        {
            duoji_doing[i].inc = 0;
            duoji_doing[i].aim = duoji_doing[i].cur;
        }
        zx_uart_send_send((u8 *)"#255PDST!");
    }
    else if (pos = str_contain_str(cmd, (u8 *)"DST:"), pos)
    {
        if (sscanf((char *)cmd, "DST:%d!", &index))
        {
            if (index < DJ_NUM)
            {
                duoji_doing[index].inc = 0;
                duoji_doing[index].aim = duoji_doing[index].cur;
            }
            sprintf((char *)cmd_return, "#%03dPDST!\r\n", (int)index);
            zx_uart_send_send(cmd_return);
            memset(cmd_return, 0, sizeof(cmd_return));
        }
    }
    else if (pos = str_contain_str(cmd, (u8 *)"RST!"), pos)
    {
        soft_reset();
    }
    else if (pos = str_contain_str(cmd, (u8 *)"PTG:"), pos)
    {
        if (sscanf((char *)cmd, "PTG:%d-%d!", &int1, &int2))
        {
            print_group(int1, int2);
        }
    }
    else if (pos = str_contain_str(cmd, (u8 *)"DGS:"), pos)
    {
        if (sscanf((char *)cmd, "DGS:%d!", &index))
        {
            group_do_ok = 1;
            do_group_once(index);
        }
    }
    else if (pos = str_contain_str(cmd, (u8 *)"DGT:"), pos)
    {
        if (sscanf((char *)cmd, "DGT:%d-%d,%d!", &group_num_start, &group_num_end, &group_num_times))
        {
            group_do_ok = 1;
            if (group_num_start != group_num_end)
            {
                do_start_index = group_num_start;
                do_time = group_num_times;
                group_do_ok = 0;
            }
            else
            {
                do_group_once(group_num_start);
            }
        }
    }
    else if (pos = str_contain_str(cmd, (u8 *)"DJR!"), pos)
    {
        zx_uart_send_send((u8 *)"#255P1500T0500!\r\n");
        AI_mode = 255;
        for (i = 0; i < DJ_NUM; i++)
        {
            int target_pos = 1500 + eeprom_info.dj_bias_pwm[i];
            duoji_doing_set(i, target_pos, 500);
        }
    }
    else if (pos = str_contain_str(cmd, (u8 *)"GETA!"), pos)
    {
        UsartPrintf(USART_DEBUG, "AAA");
    }
    else if (pos = str_contain_str(cmd, (u8 *)"$BEEP!"), pos)
    {
        beep_on_times(1, 100);
    }
}

/**
 * @brief 动作组批量执行
 *
 * 通过判断舵机是否全部执行完毕，并且group_do_ok==0时进入处理
 * 当动作组执行完毕后，打印"@Group Done"
 */
void loop_action(void)
{
    static u32 systick_ms_bak = 0;
    if (group_do_ok == 0)
    {
        if (millis() - systick_ms_bak > action_time)
        {
            systick_ms_bak = millis();
            if (group_num_times != 0 && do_time == 0)
            {
                group_do_ok = 1;
                UsartPrintf(USART_DEBUG, "@Group Done");
                return;
            }
            do_group_once(do_start_index);
            if (group_num_start < group_num_end)
            {
                if (do_start_index == group_num_end)
                {
                    do_start_index = group_num_start;
                    if (group_num_times != 0)
                    {
                        do_time--;
                    }
                    return;
                }
                do_start_index++;
            }
            else
            {
                if (do_start_index == group_num_end)
                {
                    do_start_index = group_num_start;
                    if (group_num_times != 0)
                    {
                        do_time--;
                    }
                    return;
                }
                do_start_index--;
            }
        }
    }
    else
    {
        action_time = 10;
    }
}

/**
 * @brief 把eeprom_info写入W25Q64的W25Q64_INFO_ADDR_SAVE_STR位置
 *
 * 步骤：
 * 1. 切换PB13为SPI功能
 * 2. 擦除对应扇区
 * 3. 写入eeprom_info结构体数据
 * 4. 切换PB13回LED功能
 */
void rewrite_eeprom(void)
{
    spi_flash_on(1);
    Delay_ms(10);
    w25x_eraseSector(W25Q64_INFO_ADDR_SAVE_STR / 4096);
    w25x_writeS((u8 *)(&eeprom_info), W25Q64_INFO_ADDR_SAVE_STR, sizeof(eeprom_info_t));
    spi_flash_on(0);
}

void app_servo_save_pos_check(void)
{
    static u32 last_save_time = 0;
    u8 all_idle = 1;
    u8 pos_changed = 0;
    int i;

    if (millis() - last_save_time < 30000)
        return;

    for (i = 0; i < DJ_NUM; i++)
    {
        if (duoji_doing[i].inc != 0)
        {
            all_idle = 0;
            break;
        }
    }

    if (!all_idle)
        return;

    for (i = 0; i < DJ_NUM; i++)
    {
        int cur_pos = (int)duoji_doing[i].cur;
        if (cur_pos != eeprom_info.last_pos[i])
        {
            pos_changed = 1;
            break;
        }
    }

    if (!pos_changed)
        return;

    for (i = 0; i < DJ_NUM; i++)
    {
        eeprom_info.last_pos[i] = (int)duoji_doing[i].cur;
    }
    eeprom_info.last_pos[DJ_NUM] = FLAG_VERIFY;
    rewrite_eeprom();
    last_save_time = millis();
}

/**
 * @brief 字符串中的字符替代函数
 * @param str 字符串
 * @param ch1 要被替换的字符
 * @param ch2 替换后的字符
 *
 * 遍历字符串，把所有的ch1替换为ch2
 */
void str_replace(u8 *str, u8 ch1, u8 ch2)
{
    while (*str)
    {
        if (*str == ch1)
        {
            *str = ch2;
        }
        str++;
    }
}

/**
 * @brief 打印存储在Flash里的动作组
 * @param start 起始动作组编号
 * @param end 结束动作组编号
 *
 * 从串口1发送动作组数据，由$PTG:x-y!命令调用
 */
void print_group(int start, int end)
{
    int i;
    spi_flash_on(1);
    Delay_ms(10);
    if (start > end)
    {
        int_exchange(&start, &end);
    }
    for (i = start; i <= end; i++)
    {
        memset(uart_receive_buf, 0, sizeof(uart_receive_buf));
        w25x_read(uart_receive_buf, i * ACTION_SIZE, ACTION_SIZE);
        Usart_SendString(USART1, uart_receive_buf);
        Usart_SendString(USART1, (u8 *)"\r\n");
    }
    spi_flash_on(0);
}

/**
 * @brief 两个int变量交换
 * @param a 第一个变量的指针
 * @param b 第二个变量的指针
 */
void int_exchange(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

/**
 * @brief 判断字符串str1是否包含str2
 * @param str1 被搜索的字符串
 * @param str2 要查找的子串
 * @return 找到则返回位置（从1开始），未找到返回0
 */
uint16_t str_contain_str(unsigned char *str1, unsigned char *str2)
{
    unsigned char *str_temp1, *str_temp2;
    str_temp1 = str1;
    str_temp2 = str2;
    while (*str_temp1)
    {
        if (*str_temp1 == *str_temp2)
        {
            while (*str_temp2)
            {
                if (*str_temp1++ != *str_temp2++)
                {
                    str_temp1 = str_temp1 - (str_temp2 - str2) + 1;
                    str_temp2 = str2;
                    break;
                }
            }
            if (!*str_temp2)
            {
                return (str_temp1 - str1);
            }
        }
        else
        {
            str_temp1++;
        }
    }
    return 0;
}

/**
 * @brief 执行动作组1次
 * @param group_num 动作组序号
 *
 * 步骤：
 * 1. 切换PB13为SPI功能
 * 2. 清空接收缓冲区
 * 3. 从Flash读取动作组数据
 * 4. 获取动作组中的最大时间
 * 5. 解析并执行动作组
 * 6. 切换PB13回LED功能
 */
void do_group_once(int group_num)
{
    spi_flash_on(1);
    Delay_ms(10);
    memset(uart_receive_buf, 0, sizeof(uart_receive_buf));
    w25x_read(uart_receive_buf, group_num * ACTION_SIZE, ACTION_SIZE);
    action_time = getMaxTime(uart_receive_buf);
    parse_action(uart_receive_buf);
    spi_flash_on(0);
}

/**
 * @brief 获取动作组中的最大时间
 * @param str 动作组字符串
 * @return 最大时间值（ms）
 *
 * 遍历字符串，找到所有'T'后面的4位数字，取最大值
 * 例如 #000P1500T1000!#001P2000T2000! 中最大时间是2000
 */
int getMaxTime(u8 *str)
{
    int i = 0, max_time = 0, temp_time = 0;
    while (str[i])
    {
        if (str[i] == 'T')
        {
            temp_time = (str[i + 1] - '0') * 1000 + (str[i + 2] - '0') * 100 + (str[i + 3] - '0') * 10 + (str[i + 4] - '0');
            if (temp_time > max_time)
            {
                max_time = temp_time;
            }
            i += 4;
            continue;
        }
        i++;
    }
    return max_time;
}

/**
 * @brief 单片机软件复位
 *
 * 先关闭所有中断，然后调用NVIC系统复位
 * 复位后程序从头开始执行，相当于重新上电
 */
void soft_reset(void)
{
    UsartPrintf(USART1, "stm32f103x6 reset");
    __set_FAULTMASK(1);
    NVIC_SystemReset();
}

/**
 * @brief 逆运动学移动：根据目标坐标计算舵机角度并执行
 * @param x 目标X坐标(mm)
 * @param y 目标Y坐标(mm)
 * @param z 目标Z坐标(mm)
 * @param time 运动时间(ms)
 * @return 1=成功找到解，0=无法到达目标位置
 *
 * 【算法思路】
 * 遍历爪子夹角从0到-135度，找到使3号舵机角度最大的那个解。
 * 3号舵机角度最大意味着机械臂姿态最"舒展"，不容易碰到自身。
 *
 * 【命令格式】
 * 找到解后，生成多舵机控制命令：
 * {#000PxxxxTxxxx!#001PxxxxTxxxx!#002PxxxxTxxxx!#003PxxxxTxxxx!}
 * 然后调用parse_action执行
 */
int kinematics_move(float x, float y, float z, int time)
{
    int i, min = 0, flag = 0;
    if (y < 0)
    {
        return 0;
    }
    flag = 0;
    for (i = 0; i > -135; i--)
    {
        if (kinematics_analysis(x, y, z, (float)i, &kinematics) == 0)
        {
            if (i < min)
            {
                min = i;
                flag = 1;
            }
        }
    }
    if (flag)
    {
        kinematics_analysis(x, y, z, (float)min, &kinematics);
        kinematics.servo_pwm[3] = 3000 - kinematics.servo_pwm[3];
        sprintf((char *)cmd_return, "{#000P%04dT%04d!#001P%04dT%04d!#002P%04dT%04d!#003P%04dT%04d!}",
                kinematics.servo_pwm[0], time,
                kinematics.servo_pwm[1], time,
                kinematics.servo_pwm[2], time,
                kinematics.servo_pwm[3], time);
        parse_action(cmd_return);
        return 1;
    }
    return 0;
}