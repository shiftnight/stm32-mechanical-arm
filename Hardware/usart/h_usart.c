/**
 * @file h_usart.c
 * @brief 串口驱动实现
 *
 * 【串口通信基础知识】
 * 串口通信就像两个人通过电话线对话：
 * - TX（Transmit）= 嘴巴，说话（发送数据）
 * - RX（Receive）= 耳朵，听（接收数据）
 * - GND = 共同语言，双方必须共地才能理解对方
 *
 * 【串口数据帧格式】
 * 一个字节的传输过程（从左到右）：
 *
 * 空闲  起始位  数据位(D0~D7)      停止位  空闲
 * ────┐  ┌──┬──┬──┬──┬──┬──┬──┬──┐  ┌────
 *     └──┘D0│D1│D2│D3│D4│D5│D6│D7└──┘
 *     低  ←── 8位数据(低位先发) ──→  高
 *
 * - 空闲状态：高电平
 * - 起始位：1位低电平，标志传输开始
 * - 数据位：8位，低位先发（LSB First）
 * - 停止位：1位高电平，标志传输结束
 * - 校验位：可选，本项目中不使用
 *
 * 【波特率含义】
 * 波特率 = 每秒传输的位数（包括起始位和停止位）
 * 例如 115200 波特率：
 * - 每秒传输 115200 位
 * - 每个字节需要 10 位（1起始+8数据+1停止）
 * - 每秒最多传输 115200/10 = 11520 个字节
 * - 传输一个字节需要 10/115200 ≈ 86.8 微秒
 *
 * 【中断接收原理】
 * 不用中断的话，CPU需要不断检查"有没有收到数据"（轮询），浪费CPU时间。
 * 用中断的话，CPU可以去做别的事，当硬件收到数据时，自动跳到中断函数处理。
 *
 * 数据流：
 * 电脑发送"#001P1500T1000!" → USART1逐字节接收 → 每收到1字节触发中断
 * → 中断函数存入uart_receive_buf → 收到'!'结束符 → uart1_get_ok=1
 * → 主循环检测到标志 → app_usart_run()解析指令 → 控制舵机
 *
 * 【引脚连接】
 * USART1: PA9(TX) → 电脑RX, PA10(RX) ← 电脑TX
 * USART3: PB10(TX) → 总线设备RX, PB11(RX) ← 总线设备TX
 */
#include "h_usart.h"

/* 接收缓冲区：存放串口收到的数据，最大1024字节 */
u8 uart_receive_buf[UART_BUF_SIZE];

/* 接收完成标志：
 * 0 = 正在接收中，或者等待新指令
 * 1 = 一条完整指令已接收完成，等待主循环处理
 * 主循环处理完后必须清零，否则新数据会被丢弃 */
uint16_t uart1_get_ok;

/* 指令模式：
 * 0 = 空闲，等待新指令的起始字符
 * 1 = 命令模式（$开头，!结尾）
 * 2 = 单舵机调试模式（#开头，!结尾）
 * 3 = 多舵机调试模式（{开头，!结尾）
 * 4 = 存储模式（<开头，>结尾） */
u8 uart1_mode;

/**
 * @brief 初始化串口1（调试串口）
 * @param baud 波特率，常用值：9600, 115200, 460800
 *
 * 【初始化步骤】（顺序很重要！）
 * 1. 开时钟 → 2. 配引脚 → 3. 配串口参数 → 4. 配中断 → 5. 使能串口
 *
 * 为什么这个顺序？
 * - 不开时钟，寄存器无法写入
 * - 不配引脚，串口信号无法输出
 * - 不配参数，通信速度/格式不对
 * - 不配中断，收不到数据通知
 * - 不使能，串口不工作
 *
 * USART1挂在APB2总线上（72MHz），是高速串口
 * USART2/3挂在APB1总线上（36MHz），是低速串口
 */
void Usart1_init(u32 baud)
{
    USART_InitTypeDef USART_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 第1步：使能时钟
     * GPIOA时钟：PA9/PA10需要时钟才能工作
     * USART1时钟：串口外设需要时钟才能工作
     * USART1在APB2总线上，所以用RCC_APB2PeriphClockCmd */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    /* 第2步：配置TX引脚(PA9)
     * GPIO_Mode_AF_PP = Alternate Function Push-Pull（复用推挽输出）
     *
     * "复用"意味着这个引脚不是由GPIO寄存器控制，而是由USART外设控制
     * "推挽"意味着能主动输出高电平和低电平，驱动能力强
     *
     * 为什么TX用复用推挽？
     * TX是发送引脚，需要主动驱动电平变化，所以用推挽输出
     * 但它不是普通GPIO输出，而是由USART硬件自动控制，所以用"复用" */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 第3步：配置USART1参数
     *
     * USART_BaudRate = baud
     *   波特率由参数传入，如115200
     *   USART硬件会根据时钟频率自动计算分频系数
     *   公式：波特率 = fCK / (16 × USARTDIV)
     *   fCK = APB2时钟 = 72MHz
     *   USARTDIV = 72000000 / (16 × 115200) ≈ 39.0625
     *
     * USART_WordLength_8b = 8位数据
     *   每帧传输8位数据（一个字节），最常用的设置
     *
     * USART_StopBits_1 = 1位停止位
     *   停止位标志一帧数据结束，1位最常用
     *
     * USART_Parity_No = 无校验
     *   校验位用于检测传输错误，但会降低效率，一般不用
     *
     * USART_Mode_Rx | USART_Mode_Tx = 收发模式
     *   同时启用发送和接收功能
     *
     * USART_HardwareFlowControl_None = 无硬件流控
     *   硬件流控用RTS/CTS引脚控制数据流，一般不用 */
    USART_InitStructure.USART_BaudRate = baud;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &USART_InitStructure);

    /* 第4步：配置NVIC中断
     *
     * NVIC = Nested Vectored Interrupt Controller（嵌套向量中断控制器）
     * 它决定哪个中断先执行、能不能被打断等。
     *
     * NVIC_IRQChannel = USART1_IRQn
     *   指定中断通道为USART1
     *
     * NVIC_IRQChannelPreemptionPriority = 1
     *   抢占优先级：数字越小优先级越高
     *   如果正在执行优先级2的中断，优先级1的中断可以打断它
     *   设为1，比SysTick(0)低，比普通任务高
     *
     * NVIC_IRQChannelSubPriority = 0
     *   子优先级：当两个中断抢占优先级相同时，子优先级决定谁先执行
     *   数字越小越先执行，但不能打断正在执行的同优先级中断 */
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 开启接收中断，关闭发送中断
     *
     * USART_IT_RXNE = Receive Data Register Not Empty
     *   当接收数据寄存器非空时（收到数据时）触发中断
     *   我们需要这个中断来接收数据
     *
     * USART_IT_TXE = Transmit Data Register Empty
     *   当发送数据寄存器空时触发中断（可以发送下一个字节）
     *   我们不用中断发送，而是用轮询方式发送，所以关闭 */
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_ITConfig(USART1, USART_IT_TXE, DISABLE);

    /* 第5步：使能USART1，开始工作 */
    USART_Cmd(USART1, ENABLE);
}

/**
 * @brief 初始化串口3（总线串口）
 * @param baud 波特率
 *
 * USART3挂在APB1总线上（36MHz），是低速串口
 * TX引脚(PB10)：复用推挽输出
 * RX引脚(PB11)：浮空输入
 *
 * 【浮空输入 vs 上拉输入】
 * - 浮空输入：引脚电平完全由外部决定，没有内部上拉/下拉
 *   适合：外部有明确驱动的情况（如另一个芯片的TX引脚）
 * - 上拉输入：内部接一个电阻到VCC，没信号时默认高电平
 *   适合：按键等可能没有驱动的情况
 * 这里RX用浮空输入，因为对方TX会主动驱动电平
 */
void Usart3_init(u32 baud)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    USART_ClockInitTypeDef USART_ClockInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 使能时钟：GPIOB在APB2，USART3在APB1 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    /* TX引脚(PB10)：复用推挽输出，由USART3外设控制 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* RX引脚(PB11)：浮空输入
     * GPIO_Mode_IN_FLOATING = 浮空输入
     * 引脚电平完全由外部设备（对方的TX）驱动 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* USART3时钟配置（同步模式相关，异步通信一般用默认值） */
    USART_ClockInitStructure.USART_Clock = USART_Clock_Disable;
    USART_ClockInitStructure.USART_CPOL = USART_CPOL_Low;
    USART_ClockInitStructure.USART_CPHA = USART_CPHA_2Edge;
    USART_ClockInitStructure.USART_LastBit = USART_LastBit_Disable;
    USART_ClockInit(USART3, &USART_ClockInitStructure);

    /* USART3参数配置（同USART1） */
    USART_InitStructure.USART_BaudRate = baud;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);

    /* NVIC中断配置，子优先级2（比USART1低） */
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    USART_ITConfig(USART3, USART_IT_TXE, DISABLE);
    USART_Cmd(USART3, ENABLE);
}

/**
 * @brief 发送一个字节
 * @param USARTx 串口外设（USART1或USART3）
 * @param data 要发送的字节
 *
 * 发送流程：
 * 1. 将数据写入USART_DR寄存器（发送数据寄存器）
 * 2. 等待TXE标志置1（发送数据寄存器为空，数据已转移到移位寄存器）
 * 3. 此时数据正在通过TX引脚逐位发送出去
 *
 * 注意：TXE=1只表示数据已从DR转移到移位寄存器，不是发送完成
 * 如果需要确保发送完成，应该等TC（Transmission Complete）标志
 */
void uart_send_byte(USART_TypeDef *USARTx, u8 data)
{
    USART_SendData(USARTx, data);                                       /* 写入发送数据寄存器 */
    while (USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET);      /* 等待TXE=1 */
}

/**
 * @brief 发送字符串
 * @param USARTx 串口外设
 * @param str 字符串指针（以'\0'结尾）
 *
 * 逐个字符发送，直到遇到'\0'（字符串结束符）
 * 例如 Usart_SendString(USART1, "hello") 会发送5个字节：h, e, l, l, o
 */
void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str)
{
    while (*str)              /* *str != '\0' 时继续 */
        uart_send_byte(USARTx, *str++); /* 发送当前字符，指针后移 */
}

/**
 * @brief 发送整数（转为字符串发送）
 * @param USARTx 串口外设
 * @param num 要发送的整数
 *
 * 例如 Usart_SendInt(USART1, 123) 会发送字符串 "123"（3个字节）
 * 不是发送数值123（1个字节），而是发送ASCII字符'1','2','3'
 */
void Usart_SendInt(USART_TypeDef *USARTx, int num)
{
    static u8 str[20];           /* 缓冲区，最大支持20位数字 */
    sprintf((char *)str, "%d", num); /* 将整数转为字符串 */
    Usart_SendString(USARTx, str);
}

/**
 * @brief 格式化打印（类似printf）
 * @param USARTx 串口外设
 * @param fmt 格式字符串
 * @param ... 可变参数
 *
 * 【用法示例】
 * UsartPrintf(USART1, "hello");                → 输出: hello
 * UsartPrintf(USART1, "value=%d", 100);        → 输出: value=100
 * UsartPrintf(USART1, "hex=%x", 0xEF);         → 输出: hex=ef
 * UsartPrintf(USART1, "float=%.2f", 3.14);     → 输出: float=3.14
 * UsartPrintf(USART1, "str=%s", "world");      → 输出: str=world
 *
 * 【\r\n的含义】
 * \r = CR (Carriage Return) = 回车，光标回到行首
 * \n = LF (Line Feed) = 换行，光标移到下一行
 * Windows串口助手通常需要 \r\n 才能正确换行
 * Linux/Mac通常只需要 \n
 *
 * 【实现原理】
 * 1. va_start(ap, fmt) → 开始可变参数访问
 * 2. vsnprintf() → 将格式化结果写入缓冲区（类似sprintf，但限制最大长度）
 * 3. va_end(ap) → 结束可变参数访问
 * 4. 逐字节发送缓冲区内容
 */
void UsartPrintf(USART_TypeDef *USARTx, char *fmt, ...)
{
    unsigned char UsartPrintfBuf[296]; /* 格式化缓冲区，最大296字节 */
    va_list ap;                        /* 可变参数列表 */
    unsigned char *pStr = UsartPrintfBuf;

    va_start(ap, fmt);                /* 从fmt之后开始读取可变参数 */
    vsnprintf((char *)UsartPrintfBuf, sizeof(UsartPrintfBuf), fmt, ap); /* 格式化 */
    va_end(ap);                       /* 结束可变参数访问 */

    /* 逐字节发送，等待每次发送完成(TC标志) */
    while (*pStr != 0)
    {
        USART_SendData(USARTx, *pStr++);
        while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET); /* 等待TC=1（发送完成） */
    }
}

/**
 * @brief 重定义fputc函数，使printf输出到串口
 *
 * C标准库的printf()最终会调用fputc()来输出单个字符。
 * 我们重写fputc()，让它把字符发送到USART1，这样printf()就能通过串口输出了。
 *
 * 注意：需要在Keil的魔术棒→Target→勾选 "Use MicroLIB" 才能生效
 * 因为标准库的printf体积太大，MicroLIB是精简版，适合嵌入式。
 *
 * 【寄存器直接操作说明】
 * USART1->SR & 0x40 → 检查SR寄存器的bit6(TC标志)
 *   TC = Transmission Complete，发送完成标志
 *   TC=0：发送未完成（数据还在移位寄存器中）
 *   TC=1：发送完成（数据已从TX引脚发出）
 * USART1->DR = ch → 将字符写入数据寄存器，启动发送
 */
int fputc(int ch, FILE *f)
{
    while ((USART1->SR & 0x40) == 0); /* 等待TC=1（上一次发送完成） */
    USART1->DR = (u8)ch;              /* 写入新数据，自动清除TC标志 */
    return ch;
}

/**
 * @brief USART1中断服务函数
 *
 * 【什么时候会被调用？】
 * 当USART1的RXNE标志置1时（收到一个字节），CPU自动跳转到此函数。
 * 函数名USART1_IRQHandler是固定的，不能改，启动文件中定义了向量表。
 *
 * 【中断处理流程图】
 *
 * 串口收到1字节 → RXNE标志=1 → CPU暂停当前任务
 *   │
 *   ▼
 * 清除RXNE标志（否则会反复进中断）
 *   │
 *   ▼
 * 读取DR寄存器获取数据字节
 *   │
 *   ▼
 * uart1_get_ok==1? ──是──→ 丢弃数据，返回（上一条指令还没处理完）
 *   │否
 *   ▼
 * 判断是否为起始字符（$、#、{、<）
 *   │
 *   ├─ 是 → 设置uart1_mode，重置buf_index=0
 *   └─ 否 → 继续接收
 *   │
 *   ▼
 * 存入uart_receive_buf[buf_index++]
 *   │
 *   ▼
 * 判断是否为结束符（!或>）
 *   │
 *   ├─ 是 → 添加'\0'，uart1_get_ok=1
 *   └─ 否 → 继续
 *   │
 *   ▼
 * buf_index溢出保护
 *   │
 *   ▼
 * 返回，CPU恢复之前的任务
 *
 * 【为什么要清除中断标志？】
 * 读取DR寄存器会自动清除RXNE标志。
 * 但如果不清除，CPU会认为还有新数据，反复进入中断，导致死循环。
 * 所以先手动清除，再读取数据，更安全。
 */
void USART1_IRQHandler(void)
{
    u8 sbuf_bak;                /* 暂存当前收到的字节 */
    static u16 buf_index = 0;   /* 缓冲区写入位置索引（static保持值不变） */

    /* 检查是否是RXNE中断（接收缓冲区非空） */
    if (USART_GetFlagStatus(USART1, USART_IT_RXNE) == SET)
    {
        /* 清除中断标志，防止重复进入 */
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);

        /* 读取接收到的字节
         * USART_ReceiveData()会读取DR寄存器并自动清除RXNE标志 */
        sbuf_bak = USART_ReceiveData(USART1);

        /* 如果上一条指令还没处理完，丢弃新数据
         * 这是一种简单的"流控"：主循环处理不过来时，新数据被丢弃
         * 更好的做法是用环形缓冲区，但对本项目够用了 */
        if (uart1_get_ok)
            return;

        /* 检测指令起始字符，确定指令模式
         *
         * 为什么'<'单独判断？
         * 因为'<'可能在任何时刻出现（如uart1_mode!=0时收到'<'），
         * 需要立即切换到模式4并重置缓冲区。
         * 而其他起始字符只在uart1_mode==0（空闲）时才有效。 */
        if (sbuf_bak == '<')
        {
            uart1_mode = 4;    /* 存储模式 */
            buf_index = 0;     /* 重置缓冲区索引 */
        }
        else if (uart1_mode == 0) /* 当前空闲，等待新指令 */
        {
            if (sbuf_bak == '$')
                uart1_mode = 1;    /* 命令模式 */
            else if (sbuf_bak == '#')
                uart1_mode = 2;    /* 单舵机调试模式 */
            else if (sbuf_bak == '{')
                uart1_mode = 3;    /* 多舵机调试模式 */
            buf_index = 0;         /* 新指令开始，重置索引 */
        }

        /* 将当前字节存入缓冲区 */
        uart_receive_buf[buf_index++] = sbuf_bak;

        /* 检测指令结束符
         * 模式1/2/3以'!'结尾，模式4以'>'结尾
         * 收到结束符后，添加字符串结束符'\0'，设置完成标志 */
        if (((uart1_mode == 4) && (sbuf_bak == '>')) ||
            ((uart1_mode == 1) && (sbuf_bak == '!')) ||
            ((uart1_mode == 2) && (sbuf_bak == '!')) ||
            ((uart1_mode == 3) && (sbuf_bak == '!')))
        {
            uart_receive_buf[buf_index] = '\0'; /* 添加字符串结束符，方便后续用字符串函数处理 */
            uart1_get_ok = 1;                    /* 通知主循环：一条完整指令已接收完成 */
        }

        /* 缓冲区溢出保护
         * 如果数据量超过缓冲区大小，重置索引，从头开始存
         * 这会导致之前的数据被覆盖，但至少不会越界写内存 */
        if (buf_index >= UART_BUF_SIZE)
            buf_index = 0;
    }
}

/**
 * @brief USART3中断服务函数（逻辑同USART1）
 *
 * USART3和USART1共用uart_receive_buf和uart1_get_ok，
 * 所以两个串口不能同时接收不同的指令。
 * 如果USART3正在接收，USART1的数据会被丢弃（uart1_get_ok==1时）。
 */
void USART3_IRQHandler(void)
{
    u8 sbuf_bak;
    static u16 buf_index = 0;

    if (USART_GetFlagStatus(USART3, USART_IT_RXNE) == SET)
    {
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
        sbuf_bak = USART_ReceiveData(USART3);

        if (uart1_get_ok)
            return;

        if (sbuf_bak == '<')
        {
            uart1_mode = 4;
            buf_index = 0;
        }
        else if (uart1_mode == 0)
        {
            if (sbuf_bak == '$')
                uart1_mode = 1;
            else if (sbuf_bak == '#')
                uart1_mode = 2;
            else if (sbuf_bak == '{')
                uart1_mode = 3;
            buf_index = 0;
        }

        uart_receive_buf[buf_index++] = sbuf_bak;

        if (((uart1_mode == 4) && (sbuf_bak == '>')) ||
            ((uart1_mode == 1) && (sbuf_bak == '!')) ||
            ((uart1_mode == 2) && (sbuf_bak == '!')) ||
            ((uart1_mode == 3) && (sbuf_bak == '!')))
        {
            uart_receive_buf[buf_index] = '\0';
            uart1_get_ok = 1;
        }

        if (buf_index >= UART_BUF_SIZE)
            buf_index = 0;
    }
}
