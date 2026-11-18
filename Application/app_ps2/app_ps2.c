/**
 * @file app_ps2.c
 * @brief PS2手柄应用层实现
 *
 * 【本文件的功能】
 * 1. 初始化PS2手柄通信
 * 2. 定期读取PS2手柄按键数据
 * 3. 将按键映射到舵机控制命令
 *
 * 【按键到舵机命令的映射】
 * 每个按键对应一个预定义的命令字符串，存储在pre_cmd_set_red或pre_cmd_set_grn数组中。
 * - 红灯模式：按键直接控制舵机角度（如L2控制5号舵机转到600）
 * - 绿灯模式：按键控制移动方向（如LU控制前进500）
 *
 * 【命令格式】
 * 按键按下时发送'^'前面的命令（如#005P0600T2000!）
 * 按键松开时发送'^'后面的命令（如#005PDST!，停止该舵机）
 * '^'是分隔符，将"按下动作"和"松开动作"分开
 */
#include  "h_ps2.h"
#include "app_ps2.h"

/**
 * @brief 红灯模式下的按键命令映射表
 *
 * 每个字符串格式：<PS2_REDxx:按下命令^松开命令>
 *
 * 【增量控制模式】
 * 按下按键时，舵机从当前位置增加/减少一定数值，持续往一个方向移动。
 * 松开按键时，舵机立即停止。
 *
 * 命令格式：#NNNPR+DDDDTMMMM!（增加）或 #NNNPR-DDDDTMMMM!（减少）
 * - PR = 相对定位（Position Relative）
 * - +DDDD / -DDDD = 增量值(us)，2000表示移动2000us
 * - TMMMM = 运动时间(ms)，2000表示2秒
 *
 * 移动速度 = 增量值 / 时间 = 2000us / 2000ms = 1us/ms = 1000us/s
 * 实际速度可能更慢，因为限位保护会截断超出范围的值
 *
 * - L2: 5号舵机减少（爪子闭合方向）
 * - R2: 5号舵机增加（爪子张开方向）
 * - L1: 4号舵机减少（腕部一个方向）
 * - R1: 4号舵机增加（腕部另一个方向）
 * - RU: 2号舵机增加（肘关节上抬）
 * - RR: 3号舵机减少（腕关节右转）
 * - RD: 2号舵机减少（肘关节下压）
 * - RL: 3号舵机增加（腕关节左转）
 * - SE/ST: 所有舵机复位
 * - LU: 1号舵机减少（肩关节上抬）
 * - LR: 0号舵机减少（底座右转）
 * - LD: 1号舵机增加（肩关节下压）
 * - LL: 0号舵机增加（底座左转）
 */
const char *pre_cmd_set_red[PSX_BUTTON_NUM] = {
    "<PS2_RED01:#005P0600T2000!^#005PDST!>",
    "<PS2_RED02:#005P2400T2000!^#005PDST!>",
    "<PS2_RED03:#004P0833T2000!^#004PDST!>",
    "<PS2_RED04:#004P2167T2000!^#004PDST!>",
    "<PS2_RED05:#002P0600T2000!^#002PDST!>",
    "<PS2_RED06:#003P0600T2000!^#003PDST!>",
    "<PS2_RED07:#002P2400T2000!^#002PDST!>",
    "<PS2_RED08:#003P2400T2000!^#003PDST!>",
    "<PS2_RED09:$DJR!>",
    "<PS2_RED10:>",
    "<PS2_RED11:>",
    "<PS2_RED12:$DJR!>",
    "<PS2_RED13:#001P0600T2000!^#001PDST!>",
    "<PS2_RED14:#000P0600T2000!^#000PDST!>",
    "<PS2_RED15:#001P2400T2000!^#001PDST!>",
    "<PS2_RED16:#000P2400T2000!^#000PDST!>",
};

/**
 * @brief 绿灯模式下的按键命令映射表
 *
 * 绿灯模式用于控制移动方向（适合轮式机器人）
 * $DCR命令格式：$DCR:左前,右前,左后,右后!
 * 正值=前进，负值=后退
 */
const char *pre_cmd_set_grn[PSX_BUTTON_NUM] = {
    "<PS2_RED01:$DCR:0,500,500,0!^$DCR:0,0,0,0!>",
    "<PS2_RED02:$DCR:500,0,0,500!^$DCR:0,0,0,0!>",
    "<PS2_RED03:$DCR:0,1000,1000,0!^$DCR:0,0,0,0!>",
    "<PS2_RED04:$DCR:1000,0,0,1000!^$DCR:0,0,0,0!>",
    "<PS2_RED05:$DCR:1000,1000,1000,1000!^$DCR:0,0,0,0!>",
    "<PS2_RED06:$DCR:1000,-1000,-1000,1000!^$DCR:0,0,0,0!>",
    "<PS2_RED07:$DCR:-1000,-1000,-1000,-1000!^$DCR:0,0,0,0!>",
    "<PS2_RED08:$DCR:-1000,1000,1000,-1000!^$DCR:0,0,0,0!>",
    "<PS2_RED09:$DJR!>",
    "<PS2_RED10:>",
    "<PS2_RED11:>",
    "<PS2_RED12:$DJR!>",
    "<PS2_RED13:$DCR:500,500,500,500!^$DCR:0,0,0,0!>",
    "<PS2_RED14:$DCR:500,-500,500,-500!^$DCR:0,0,0,0!>",
    "<PS2_RED15:$DCR:-500,-500,-500,-500!^$DCR:0,0,0,0!>",
    "<PS2_RED16:$DCR:-500,500,-500,500!^$DCR:0,0,0,0!>",
};

/**
 * @brief PS2手柄初始化
 * 调用底层驱动初始化PS2手柄的GPIO引脚
 */
void app_ps2_init(void)
{
    ps2_init();
}

/**
 * @brief PS2手柄数据处理任务
 *
 * 每50ms读取一次PS2手柄数据，检测按键变化。
 * 只有按键状态发生变化时才处理（避免重复触发）。
 *
 * 【处理流程】
 * 1. 检查是否到达50ms间隔
 * 2. 读取PS2手柄数据到psx_buf
 * 3. 比较本次和上次按键值，如果不同则处理
 * 4. 调用app_ps2_buf解析按键并执行对应命令
 */
void app_ps2_run(void)
{
    static unsigned char psx_button_bak[2] = {0xFF, 0xFF};
    static u32 systick_ms_bak = 0;

    if (millis() - systick_ms_bak < 50)
    {
        return;
    }
    systick_ms_bak = millis();

    ps2_write_read();

    if (!ps2_isConnected)
    {
        return;
    }

    if (psx_button_bak[0] != psx_buf[3] || psx_button_bak[1] != psx_buf[4])
    {
        app_ps2_buf(psx_buf + 3, psx_buf[1]);
        psx_button_bak[0] = psx_buf[3];
        psx_button_bak[1] = psx_buf[4];
    }
}

/**
 * @brief 从PS2命令字符串中提取实际命令
 * @param src 源字符串，格式：<PS2_REDxx:命令1^命令2> 或 <PS2_REDxx:命令>
 * @param is_release 0=按下（提取命令1），1=松开（提取命令2）
 * @param dest 输出缓冲区
 * @param dest_size 输出缓冲区大小
 *
 * 【提取规则】
 * 按下时：提取':'后面到'^'（或到'>'）之间的内容
 * 松开时：提取'^'后面到'>'之间的内容
 *
 * 例如：<PS2_RED01:#005PR-2000T2000!^#005PDST!>
 * 按下提取：#005PR-2000T2000!
 * 松开提取：#005PDST!
 */
static void extract_ps2_cmd(const char *src, u8 is_release, char *dest, int dest_size)
{
    const char *start = NULL;
    const char *end = NULL;
    const char *caret = NULL;
    int len;

    memset(dest, 0, dest_size);

    start = strchr(src, ':');
    if (!start)
        return;
    start++;

    caret = strchr(src, '^');

    if (is_release)
    {
        if (!caret)
            return;
        start = caret + 1;
        end = strchr(start, '>');
    }
    else
    {
        if (caret)
            end = caret;
        else
            end = strchr(start, '>');
    }

    if (!end)
        end = start + strlen(start);

    len = end - start;
    if (len <= 0 || len >= dest_size)
        return;

    memcpy(dest, start, len);
    dest[len] = '\0';
}

/**
 * @brief 执行提取出来的命令
 * @param cmd 命令字符串（以'$'或'#'开头）
 *
 * '$'开头 → parse_cmd（系统命令）
 * '#'开头 → parse_action（舵机控制命令）
 */
static void execute_ps2_cmd(char *cmd)
{
    if (strlen(cmd) == 0)
        return;

    if (cmd[0] == '$')
    {
        usart1_close();
        strcpy((char *)cmd_return, cmd);
        strcpy((char *)uart_receive_buf, cmd);
        usart1_open();
        parse_cmd((u8 *)cmd);
    }
    else if (cmd[0] == '#')
    {
        usart1_close();
        strcpy((char *)cmd_return, cmd);
        strcpy((char *)uart_receive_buf, cmd);
        usart1_open();
        parse_action((u8 *)cmd);
    }
}

/**
 * @brief 处理PS2手柄按键数据
 * @param buf 按键数据指针（2字节，16个按键状态）
 * @param mode 手柄模式（0x41=绿灯，0x73=红灯）
 *
 * 【按键检测原理】
 * PS2协议：bit=0表示按键按下，bit=1表示按键未按下
 * 通过XOR比较当前状态和上次状态，检测哪些按键发生了变化
 *
 * 【按下和松开的处理】
 * - 按键按下（bit从1变0）：发送'^'前面的命令（动作命令）
 * - 按键松开（bit从0变1）：发送'^'后面的命令（停止命令）*/
void app_ps2_buf(u8 *buf, u8 mode)
{
    u8 i;
    static u16 bak = 0xffff;
    u16 current, changed;
    const char *cmd_str;
    char extracted_cmd[64];

    current = buf[0] << 8 | buf[1];

    if (bak == current)
        return;

    changed = bak ^ current;

    for (i = 0; i < 16; i++)
    {
        if (!(changed & (1 << i)))
            continue;

        if (mode == PS2_LED_RED)
            cmd_str = pre_cmd_set_red[i];
        else if (mode == PS2_LED_GRN)
            cmd_str = pre_cmd_set_grn[i];
        else
            continue;

        if (strlen(cmd_str) == 0 || strchr(cmd_str, ':') == NULL)
            continue;

        if (!(current & (1 << i)))
        {
            extract_ps2_cmd(cmd_str, 0, extracted_cmd, sizeof(extracted_cmd));
            execute_ps2_cmd(extracted_cmd);
        }
        else
        {
            extract_ps2_cmd(cmd_str, 1, extracted_cmd, sizeof(extracted_cmd));
            execute_ps2_cmd(extracted_cmd);
        }
    }

    bak = current;
    beep_on();
    Delay_ms(10);
    beep_off();
}
