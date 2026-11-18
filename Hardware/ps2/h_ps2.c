/**
 * @file h_ps2.c
 * @brief PS2手柄驱动实现
 *
 * 【PS2通信协议】
 * PS2手柄使用类似SPI的同步串行协议：
 * 1. 拉低CS（选中手柄）
 * 2. 单片机在CLK下降沿发送CMD位，手柄在CLK上升沿发送DAT位
 * 3. 每次传输8位（一个字节），低位先发
 * 4. 拉高CS（取消选中）
 *
 * 【数据帧格式】
 * 字节0：0xFF（空闲）
 * 字节1：0x41(绿灯) 或 0x73(红灯)，表示手柄模式
 * 字节2：0x5A（数据开始标志）
 * 字节3~4：按键状态（16个按键，每位对应一个按键，0=按下，1=未按下）
 * 字节5~8：摇杆模拟量（红灯模式下有效）
 */
#include "h_ps2.h"
#include "app_ps2.h"

/* PS2手柄数据缓冲区，9字节 */
u8 psx_buf[9] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* PS2手柄连接状态标志 */
u8 ps2_isConnected = 0;

/**
 * @brief 初始化PS2手柄引脚
 *
 * DAT(PA15)：下拉输入，接收手柄数据
 * CMD(PA14)：推挽输出，发送命令到手柄
 * CS(PA13)：推挽输出，片选信号
 * CLK(PA12)：推挽输出，时钟信号
 *
 * 默认状态：CS=1(未选中)，CLK=1(空闲)，CMD=1
 */
void ps2_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能GPIOA时钟 */
    RCC_APB2PeriphClockCmd(
        PS2_DAT_GPIO_CLK | PS2_CMD_GPIO_CLK |
        PS2_CS_GPIO_CLK | PS2_CLK_GPIO_CLK, ENABLE);

    /* DAT引脚：下拉输入（手柄输出数据，单片机读取） */
    GPIO_InitStructure.GPIO_Pin = PS2_DAT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PS2_DAT_GPIO_PORT, &GPIO_InitStructure);

    /* CMD引脚：推挽输出（单片机输出命令） */
    GPIO_InitStructure.GPIO_Pin = PS2_CMD_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PS2_CMD_GPIO_PORT, &GPIO_InitStructure);

    /* CS引脚：推挽输出（片选） */
    GPIO_InitStructure.GPIO_Pin = PS2_CS_PIN;
    GPIO_Init(PS2_CS_GPIO_PORT, &GPIO_InitStructure);

    /* CLK引脚：推挽输出（时钟） */
    GPIO_InitStructure.GPIO_Pin = PS2_CLK_PIN;
    GPIO_Init(PS2_CLK_GPIO_PORT, &GPIO_InitStructure);

    /* 默认电平：CS=1(未选中)，CLK=1(空闲)，CMD=1 */
    PS2_CS(1);
    PS2_CLK(1);
    PS2_CMD(1);
}

/**
 * @brief SPI-like传输一个字节
 * @param data 要发送的字节
 * @return 接收到的字节
 *
 * 通信时序（每个bit）：
 * 1. 设置CMD引脚为当前bit值
 * 2. 拉高CLK → 延时 → 拉低CLK → 延时 → 拉高CLK
 * 3. 读取DAT引脚的值
 *
 * 低位先发（LSB First），与标准SPI一致
 */
u8 ps2_transfer(u8 data)
{
    u8 rd_data = 0, wt_data = data, i;
    for (i = 0; i < 8; i++)
    {
        /* 设置CMD引脚为当前bit值（低位先发） */
        PS2_CMD((wt_data & (0x01 << i)));

        /* 产生时钟脉冲：高→低→高 */
        PS2_CLK(1);
        Delay_us(6);
        PS2_CLK(0);     /* 下降沿：手柄在此时读取CMD */
        Delay_us(6);
        PS2_CLK(1);     /* 上升沿：手柄在此时输出DAT */

        /* 读取DAT引脚 */
        if (PS2_DAT())
        {
            rd_data |= (0x01 << i);
        }
    }
    return rd_data;
}

/**
 * @brief 读取PS2手柄数据
 *
 * 通信流程：
 * 1. 拉低CS（选中手柄）
 * 2. 发送0x01（开始指令），接收psx_buf[0]
 * 3. 发送0x42（询问数据指令），接收psx_buf[1]（模式：0x41或0x73）
 * 4. 发送0x00（空字节，只接收），依次接收psx_buf[2]~[8]
 * 5. 拉高CS（取消选中）
 */
void ps2_write_read(void)
{
    PS2_CS(0);                                /* 选中手柄 */
    psx_buf[0] = ps2_transfer(START_CMD);    /* 发送开始指令 */
    psx_buf[1] = ps2_transfer(ASK_DAT_CMD);  /* 发送询问数据指令 */
    psx_buf[2] = ps2_transfer(0x00);         /* 接收数据开始标志 */
    psx_buf[3] = ps2_transfer(0x00);         /* 接收按键状态字节1 */
    psx_buf[4] = ps2_transfer(0x00);         /* 接收按键状态字节2 */
    psx_buf[5] = ps2_transfer(0x00);         /* 接收摇杆数据 */
    psx_buf[6] = ps2_transfer(0x00);
    psx_buf[7] = ps2_transfer(0x00);
    psx_buf[8] = ps2_transfer(0x00);
    PS2_CS(1);                                /* 取消选中 */

    /* 判断手柄是否连接：模式字节正确则已连接 */
    ps2_isConnected = (psx_buf[1] == PS2_MODE_GRN || psx_buf[1] == PS2_MODE_RED);
}

/**
 * @brief 解析PS2手柄按键数据
 * @param buf 按键数据（2字节，16个按键）
 * @param mode 手柄模式（0x41绿灯/0x73红灯）
 *
 * 此函数由app_ps2_run()间接调用（通过app_ps2_buf）
 * h_ps2.c是底层驱动，不直接处理按键逻辑
 */
void parse_psx_buf(u8 *buf, u8 mode)
{
    app_ps2_buf(buf, mode);
}
