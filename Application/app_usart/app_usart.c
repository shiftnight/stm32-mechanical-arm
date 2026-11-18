/**
 * @file app_usart.c
 * @brief 串口应用层实现
 *
 * 【串口指令格式】
 * 本项目支持4种串口指令模式，通过起始字符区分：
 *
 * 模式1 - 命令模式：    $xxxx!     （$开头，!结尾）
 * 模式2 - 单舵机调试：  #001P1500T1000!
 *   #     -> 指令开始
 *   001   -> 舵机编号(1~255)
 *   P     -> 位置标记
 *   1500  -> 目标脉宽(510~2490us)
 *   T     -> 时间标记
 *   1000  -> 运动时间(ms)
 *   !     -> 指令结束
 * 模式3 - 多舵机调试：  {xxxx!     （{开头，!结尾）
 * 模式4 - 存储模式：    <xxxx>     （<开头，>结尾）
 *
 * 【数据流】
 * 串口收到数据 -> USART中断存入uart_receive_buf ->
 * 收到结束符后uart1_get_ok=1 -> 主循环调用app_usart_run() ->
 * 根据模式分发到parse_action()或parse_cmd()或save_action() -> 控制舵机
 */
#include "app_usart.h"

/**
 * @brief 初始化串口
 * USART1: 115200波特率，连接电脑调试
 * USART3: 115200波特率，连接总线设备
 */
void app_usart_init(void)
{
    Usart1_init(115200);
    Usart3_init(115200);
    UsartPrintf(USART_DEBUG, "uart13_init succed\r\n");
}

/**
 * @brief 串口指令处理任务
 *
 * 检测串口是否接收完成，如果完成则根据模式分发指令：
 * - 模式1（$开头）：命令模式，调用parse_cmd()
 * - 模式2（#开头）：舵机控制模式，调用parse_action()
 * - 模式3（{开头）：多舵机调试模式，调用parse_action()
 * - 模式4（<开头）：存储模式，调用save_action()
 */
void app_usart_run(void)
{
    if (uart1_get_ok)
    {
        UsartPrintf(USART_DEBUG, "%s\r\n", uart_receive_buf);

        if (uart1_mode == 1)
        {
            UsartPrintf(USART_DEBUG, "uart1_mode1 命令模式\r\n");
            parse_cmd(uart_receive_buf);
        }
        else if (uart1_mode == 2)
        {
            UsartPrintf(USART_DEBUG, "uart1_mode2 单个舵机调试模式\r\n");
            parse_action(uart_receive_buf);
        }
        else if (uart1_mode == 3)
        {
            UsartPrintf(USART_DEBUG, "uart1_mode3 多舵机调试模式\r\n");
            parse_action(uart_receive_buf);
        }
        else if (uart1_mode == 4)
        {
            UsartPrintf(USART_DEBUG, "uart1_mode4 存储模式\r\n");
            save_action(uart_receive_buf);
        }

        uart1_mode = 0;
        uart1_get_ok = 0;
    }
}
