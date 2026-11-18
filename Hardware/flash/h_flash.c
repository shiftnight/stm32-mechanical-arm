/**
 * @file h_flash.c
 * @brief SPI Flash驱动实现（W25Q64芯片）
 *
 * 【SPI通信流程】
 * 每次与Flash通信的步骤：
 * 1. 拉低CS（选中Flash）
 * 2. 发送指令码（如0x03=读数据）
 * 3. 发送24位地址（3个字节）
 * 4. 发送/接收数据
 * 5. 拉高CS（取消选中）
 *
 * 【Flash写入注意事项】
 * - 写入前必须先发送"写使能"指令(0x06)
 * - 只能从1写到0，不能从0写到1
 * - 要把0变回1，必须先擦除整个扇区(4KB)
 * - 擦除后所有字节变为0xFF
 * - 一次最多写入256字节（一页）
 * - 写入地址跨页时需要分多次写入
 *
 * 【引脚连接】
 * PB12 → CS（片选）
 * PB13 → CLK（时钟）  ← 与LED共用！
 * PB14 → MISO（主入从出）
 * PB15 → MOSI（主出从入）
 */
#include "h_flash.h"

/* SPI和GPIO初始化结构体（全局变量，多处复用） */
SPI_InitTypeDef SPI_InitStructure;
GPIO_InitTypeDef GPIO_InitStructure;

/**
 * @brief 切换SPI/LED引脚模式
 * @param x 1=切换PB13为SPI时钟功能，0=切换PB13为LED输出功能
 *
 * 因为PB13同时是LED和SPI2_CLK的引脚，两者不能同时使用。
 * 使用Flash时调用spi_flash_on(1)，用完调用spi_flash_on(0)。
 */
void spi_flash_on(u8 x)
{
    if (x)
    {
        /* 切换为SPI模式：PB13设为复用推挽输出（SPI2_CLK） */
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_Init(GPIOB, &GPIO_InitStructure);
        GPIO_SetBits(GPIOB, GPIO_Pin_13);
        SPI_Cmd(SPI2, ENABLE);
        spi_write_read(0xFF); /* 启动SPI传输（发送一个空字节） */
    }
    else
    {
        /* 切换为LED模式：PB13设为普通推挽输出 */
        SPI_Cmd(SPI2, DISABLE);
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOB, &GPIO_InitStructure);
    }
}

/**
 * @brief 初始化SPI2接口和Flash引脚
 *
 * 配置GPIOB的12/13/14/15引脚为SPI2功能，
 * 然后初始化SPI2为主机模式，先低速(256分频)启动，再切换到高速(2分频)。
 */
void spi_flash_init(void)
{
    /* 使能GPIOB和SPI2时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* CS引脚(PB12)：推挽输出，默认高电平（未选中） */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_12);

    /* CLK(PB13)和MOSI(PB15)：复用推挽输出（由SPI外设控制） */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* MISO(PB14)：上拉输入（接收Flash数据） */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_SetBits(GPIOB, GPIO_Pin_13 | GPIO_Pin_15);

    /* SPI2配置：
     * 全双工、主机模式、8位数据、CPOL=高、CPHA=第二边沿
     * 软件CS、256分频(低速启动)、MSB优先 */
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure);
    SPI_Cmd(SPI2, ENABLE);
    spi_write_read(0xFF);
    spi_set_speed(SPI_BaudRatePrescaler_2); /* 切换到高速(36MHz) */
}

/**
 * @brief 设置SPI时钟分频
 * @param SpeedSet 分频值，如SPI_BaudRatePrescaler_2=2分频
 * SPI2挂在APB1(36MHz)上，2分频=18MHz
 */
void spi_set_speed(uint16_t SpeedSet)
{
    SPI_InitStructure.SPI_BaudRatePrescaler = SpeedSet;
    SPI_Init(SPI2, &SPI_InitStructure);
    SPI_Cmd(SPI2, ENABLE);
}

/**
 * @brief SPI读写一个字节
 * @param TxData 要发送的字节
 * @return 接收到的字节
 *
 * SPI是全双工通信，发送和接收同时进行。
 * 发送一个字节的同时，也会接收到一个字节。
 * 如果只想读取，发送0xFF（dummy字节）即可。
 */
u8 spi_write_read(u8 TxData)
{
    u8 retry = 0;
    /* 等待发送缓冲区为空 */
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET)
    {
        retry++;
        if (retry > 200) return 0;
    }
    SPI_I2S_SendData(SPI2, TxData);
    /* 等待接收缓冲区非空 */
    retry = 0;
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET)
    {
        retry++;
        if (retry > 200) return 0;
    }
    return SPI_I2S_ReceiveData(SPI2);
}

/**
 * @brief 读取Flash芯片ID
 * @return 16位ID，如0xEF16表示W25Q64
 *
 * JEDEC ID格式：制造商ID(1字节) + 设备ID(2字节)
 * 本函数只读取前2字节，返回16位ID
 */
u16 spi_flash_read_id(void)
{
    u16 Temp = 0;
    SPI_FLASH_CS(0);
    spi_write_read(W25X_JedecDeviceID);
    Temp |= spi_write_read(0xFF) << 8;
    Temp |= spi_write_read(0xFF);
    SPI_FLASH_CS(1);
    return Temp;
}

/**
 * @brief 读取Flash状态寄存器
 * @return 状态寄存器值，bit0=1表示忙
 */
u8 spi_flash_read_SR(void)
{
    u8 byte = 0;
    SPI_FLASH_CS(0);
    spi_write_read(W25X_ReadStatusReg);
    byte = spi_write_read(0xFF);
    SPI_FLASH_CS(1);
    return byte;
}

/**
 * @brief 写入Flash状态寄存器
 */
void spi_flash_write_SR(u8 SR)
{
    SPI_FLASH_CS(0);
    spi_write_read(W25X_WriteStatusReg);
    spi_write_read(SR);
    SPI_FLASH_CS(1);
}

/**
 * @brief 写使能
 * Flash每次写入/擦除前必须先发送写使能指令
 */
void spi_flash_write_enable(void)
{
    SPI_FLASH_CS(0);
    spi_write_read(W25X_WriteEnable);
    SPI_FLASH_CS(1);
}

/**
 * @brief 写失能
 */
void spi_flash_write_disable(void)
{
    SPI_FLASH_CS(0);
    spi_write_read(W25X_WriteDisable);
    SPI_FLASH_CS(1);
}

/**
 * @brief 读取1个字节
 * @param ReadAddr 24位读取地址
 */
char spi_flash_read_byte(u32 ReadAddr)
{
    char temp;
    SPI_FLASH_CS(0);
    spi_write_read(W25X_ReadData);
    spi_write_read(ReadAddr >> 16); /* 地址高8位 */
    spi_write_read(ReadAddr >> 8);  /* 地址中8位 */
    spi_write_read(ReadAddr);       /* 地址低8位 */
    temp = spi_write_read(0xFF);
    SPI_FLASH_CS(1);
    return temp;
}

/**
 * @brief 读取多个字节
 * @param pBuffer 接收缓冲区
 * @param ReadAddr 起始地址(24位)
 * @param NumByteToRead 读取长度
 */
void spi_flash_read(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead)
{
    u16 i;
    SPI_FLASH_CS(0);
    spi_write_read(W25X_ReadData);
    spi_write_read(ReadAddr >> 16);
    spi_write_read(ReadAddr >> 8);
    spi_write_read(ReadAddr);
    for (i = 0; i < NumByteToRead; i++)
        pBuffer[i] = spi_write_read(0xFF);
    SPI_FLASH_CS(1);
}

/**
 * @brief 写入1个字节
 * @param tmp 要写入的数据
 * @param WriteAddr 写入地址(24位)
 */
void spi_flash_write_byte(char tmp, u32 WriteAddr)
{
    spi_flash_write_enable();
    SPI_FLASH_CS(0);
    spi_write_read(W25X_PageProgram);
    spi_write_read(WriteAddr >> 16);
    spi_write_read(WriteAddr >> 8);
    spi_write_read((u8)WriteAddr);
    spi_write_read((u8)tmp);
    SPI_FLASH_CS(1);
    spi_flash_wait_busy();
}

/* 扇区缓冲区，用于带擦除检查的写入 */
u8 W25Q_BUFF[4096];

/**
 * @brief 带擦除检查的写入
 * @param pBuffer 数据缓冲区
 * @param WriteAddr 起始地址
 * @param NumByteToWrite 写入长度
 *
 * Flash只能从1写到0，要写0必须先擦除。
 * 此函数会检查目标区域是否需要擦除，如果需要则：
 * 1. 读取整个扇区到缓冲区
 * 2. 擦除扇区
 * 3. 合并新旧数据
 * 4. 写回整个扇区
 */
void spi_flash_write(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u32 secpos;
    u16 secoff;
    u16 secremain;
    u16 i;
    secpos = WriteAddr / 4096;
    secoff = WriteAddr % 4096;
    secremain = 4096 - secoff;
    if (NumByteToWrite <= secremain)
        secremain = NumByteToWrite;

    while (1)
    {
        spi_flash_read(W25Q_BUFF, secpos * 4096, 4096);
        for (i = secoff; i < secremain + secoff && i < 4096; i++)
        {
            if (W25Q_BUFF[i] != 0xFF)
                break;
        }
        if (i < secremain + secoff && i < 4096)
        {
            spi_flash_erase_sector(secpos);
            for (i = 0; i < secremain; i++)
                W25Q_BUFF[secoff + i] = pBuffer[i];
            spi_flash_write_no_check(W25Q_BUFF, secpos * 4096, 4096);
        }
        else
        {
            spi_flash_write_no_check(pBuffer, WriteAddr, secremain);
        }

        if (NumByteToWrite == secremain)
            break;

        secpos++;
        secoff = 0;
        pBuffer += secremain;
        WriteAddr += secremain;
        NumByteToWrite -= secremain;
        secremain = 4096;
        if (NumByteToWrite > 4096)
            secremain = 4096;
        else
            secremain = NumByteToWrite;
    }
}

/**
 * @brief 页编程：在一页(256字节)内写入数据
 * @param pBuffer 数据缓冲区
 * @param WriteAddr 起始地址
 * @param NumByteToWrite 写入长度（不超过页内剩余空间）
 */
void spi_flash_write_page(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 pageremain;
    pageremain = 256 - WriteAddr % 256;
    if (NumByteToWrite > pageremain)
        NumByteToWrite = pageremain;

    spi_flash_write_enable();
    SPI_FLASH_CS(0);
    spi_write_read(W25X_PageProgram);
    spi_write_read((u8)(WriteAddr >> 16));
    spi_write_read((u8)(WriteAddr >> 8));
    spi_write_read((u8)WriteAddr);
    for (u16 i = 0; i < NumByteToWrite; i++)
        spi_write_read(pBuffer[i]);
    SPI_FLASH_CS(1);
    spi_flash_wait_busy();
}

/**
 * @brief 扇区写入：在一个扇区(4KB)内写入数据
 */
void spi_flash_write_sector(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 sectorremain = 4096 - WriteAddr % 4096;
    if (NumByteToWrite > sectorremain)
        NumByteToWrite = sectorremain;
    spi_flash_write_no_check(pBuffer, WriteAddr, NumByteToWrite);
}

/**
 * @brief 无校验写入：直接按页写入，不检查是否需要擦除
 * 自动处理跨页写入
 */
void spi_flash_write_no_check(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite)
{
    u16 pageremain;
    pageremain = 256 - WriteAddr % 256;
    if (NumByteToWrite <= pageremain)
        pageremain = NumByteToWrite;

    while (1)
    {
        spi_flash_write_page(pBuffer, WriteAddr, NumByteToWrite);
        if (NumByteToWrite == pageremain)
            break;
        else
        {
            pBuffer += pageremain;
            WriteAddr += pageremain;
            NumByteToWrite -= pageremain;
            if (NumByteToWrite > 256)
                pageremain = 256;
            else
                pageremain = NumByteToWrite;
        }
    }
}

/**
 * @brief 擦除一个扇区(4KB)
 * @param Dst_Addr 扇区编号(0~2047)，不是字节地址！
 * 内部会左移12位转换为字节地址
 */
void spi_flash_erase_sector(u32 Dst_Addr)
{
    Dst_Addr <<= 12;
    spi_flash_write_enable();
    spi_flash_wait_busy();
    SPI_FLASH_CS(0);
    spi_write_read(W25X_SectorErase);
    spi_write_read((u8)(Dst_Addr >> 16));
    spi_write_read((u8)(Dst_Addr >> 8));
    spi_write_read((u8)Dst_Addr);
    SPI_FLASH_CS(1);
    spi_flash_wait_busy();
}

/**
 * @brief 等待Flash忙状态结束
 * 状态寄存器bit0=1表示忙，=0表示空闲
 */
void spi_flash_wait_busy(void)
{
    while ((spi_flash_read_SR() & 0x01) == 0x01);
}

/**
 * @brief 擦除整个芯片（慎用！会清除所有数据）
 */
void spi_flash_erase_chip(void)
{
    spi_flash_write_enable();
    SPI_FLASH_CS(0);
    spi_write_read(W25X_ChipErase);
    SPI_FLASH_CS(1);
    spi_flash_wait_busy();
}

/**
 * @brief 进入掉电模式（省电）
 */
void spi_flash_powerdown(void)
{
    SPI_FLASH_CS(0);
    spi_write_read(W25X_PowerDown);
    SPI_FLASH_CS(1);
}

/**
 * @brief 从掉电模式唤醒
 */
void spi_flash_wake_up(void)
{
    SPI_FLASH_CS(0);
    spi_write_read(W25X_ReleasePowerDown);
    SPI_FLASH_CS(1);
}

/**
 * @brief 简化写函数：先擦除扇区再写入
 * @param pBuff 数据缓冲区
 * @param WriteAddr 写入地址
 * @param NumByteToWrite 写入长度
 *
 * 步骤：
 * 1. 计算扇区地址和偏移
 * 2. 擦除目标扇区
 * 3. 读取扇区原有数据到W25Q_BUFF
 * 4. 将新数据合并到W25Q_BUFF中
 * 5. 将整个W25Q_BUFF写回扇区
 */
void SpiFlsahWriteS(u8 *pBuff, u32 WriteAddr, u16 NumByteToWrite)
{
    u32 sectaddr = WriteAddr / 4096;
    u16 secoff = WriteAddr % 4096;
    u16 secremain = 4096 - secoff;
    u16 i;

    if (NumByteToWrite <= secremain)
        secremain = NumByteToWrite;

    spi_flash_read(W25Q_BUFF, sectaddr * 4096, 4096);
    spi_flash_erase_sector(sectaddr);

    for (i = 0; i < secoff; i++)
    {
        /* 保留扇区前半部分的原有数据（W25Q_BUFF已在上面读取）
         * 这里不需要做任何操作，因为W25Q_BUFF已经包含了这些数据 */
    }

    for (i = 0; i < secremain; i++)
        W25Q_BUFF[secoff + i] = pBuff[i];

    spi_flash_write_no_check(W25Q_BUFF, sectaddr * 4096, 4096);
}
