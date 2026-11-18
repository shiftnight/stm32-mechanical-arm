/**
 * @file app_servo.h
 * @brief 舵机动作组管理头文件
 *
 * 【动作组概念】
 * 动作组是一系列舵机动作的集合，可以按顺序自动执行。
 * 例如：机械臂"抓取"动作组 = 肩膀转到A位 -> 肘部转到B位 -> 爪子闭合
 * 每个动作存储在W25Q64 Flash中，掉电不丢失。
 *
 * 【存储结构】
 * W25Q64 Flash中分配了专门的区域存储动作组数据：
 * - 起始地址：W25Q64_INFO_ADDR_SAVE_STR = (8*1024-4)*1024
 * - eeprom_info结构体：存储版本号、舵机偏差、预命令等配置信息
 * - 动作数据：每个动作占256字节，按编号顺序存储
 *
 * 【指令格式】
 * 舵机控制：#001P1500T1000!
 *   #     -> 指令起始
 *   001   -> 舵机编号(1~6)
 *   P     -> 位置标记(Position)
 *   1500  -> 目标脉宽(510~2490us)
 *   T     -> 时间标记(Time)
 *   1000  -> 运动时间(ms)
 *   !     -> 指令结束
 *
 * 偏差调整：#001PSCK+050! 或 #001PSCK-050!
 *   PSCK  -> 偏差调整标记
 *   +/-   -> 偏差方向
 *   050   -> 偏差值
 */
#ifndef __APP_SERVO_H__
#define __APP_SERVO_H__
#include "main.h"

#define DJ_NUM 8

#define W25Q64_INFO_ADDR_SAVE_STR (((8 << 10) - 4) << 10)

#define FLAG_VERIFY 0X25

#define ACTION_SIZE    256
#define PRE_CMD_SIZE   128
#define CMD_RETURN_SIZE 1024

extern u8 cmd_return[CMD_RETURN_SIZE];

/**
 * @brief 存储在Flash中的配置信息结构体
 *
 * 这个结构体保存了舵机的校准偏差和预定义命令，
 * 掉电后不会丢失（存在外部Flash中）。
 *
 * version: 版本号/校验值，用于判断Flash数据是否有效
 * dj_record_num: 已记录的动作数量
 * pre_cmd: 预定义命令（开机自动执行的命令）
 * dj_bias_pwm: 每个舵机的脉宽偏差值（校准用）
 *   例如：舵机1的中间位置应该是1500us，但实际是1520us
 *   则 dj_bias_pwm[1] = 20，系统会自动补偿这个偏差
 */
typedef struct
{
    u32 version;
    u32 dj_record_num;
    u8 pre_cmd[PRE_CMD_SIZE + 1];
    int dj_bias_pwm[DJ_NUM + 1];
    int last_pos[DJ_NUM + 1];
} eeprom_info_t;

extern eeprom_info_t eeprom_info;

extern u8 group_do_ok;

extern u8 AI_mode;

extern int do_start_index;
extern int do_time;
extern int group_num_start;
extern int group_num_end;
extern int group_num_times;
extern int do_end_index;
extern u32 action_time;

void zx_uart_send_send(u8 *str);

void parse_action(u8 *uart_receive_buf);

void save_action(u8 *str);

void parse_cmd(u8 *cmd);

void loop_action(void);

void rewrite_eeprom(void);

void app_servo_save_pos_check(void);

void str_replace(u8 *str, u8 ch1, u8 ch2);

void int_exchange(int *a, int *b);

void print_group(int start, int end);

uint16_t str_contain_str(unsigned char *str1, unsigned char *str2);

void do_group_once(int group_num);

int getMaxTime(u8 *str);

void soft_reset(void);

int kinematics_move(float x,float y,float z,int time);

#endif
