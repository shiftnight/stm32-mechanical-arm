/**
 * @file h_flash.h
 * @brief SPI Flash驱动头文件（W25Q64芯片）
 *
 * 【SPI基础知识】
 * SPI = Serial Peripheral Interface（串行外设接口）
 * 是一种高速、全双工、同步的通信总线，常用于连接Flash、屏幕等外设。
 *
 * SPI有4根线：
 * - CS（片选）：选中从设备，低电平有效
 * - CLK（时钟）：由主机产生，同步数据传输
 * - MOSI（主出从入）：主机发送数据
 * - MISO（主入从出）：主机接收数据
 *
 * 【W25Q64芯片】
 * 容量：64Mbit = 8MByte
 * 扇区大小：4KB（擦除的最小单位）
 * 页大小：256字节（写入的最小单位）
 * 地址：24位（0x000000 ~ 0x7FFFFF）
 *
 * 【重要提示】
 * PB13同时是LED引脚和SPI时钟引脚！
 * 使用SPI Flash前需调用spi_flash_on(1)切换为SPI模式，
 * 使用完后调用spi_flash_on(0)切换回LED模式。
 */
#ifndef __H_FLASH_H__
#define __H_FLASH_H__
#include "main.h"

/* SPI片选引脚控制宏：0=选中Flash，1=取消选中 */
#define SPI_FLASH_CS(x) GPIO_WriteBit(GPIOB, GPIO_Pin_12, (BitAction)x)

/* W25Q系列芯片ID定义 */
#define W25Q80  0XEF13
#define W25Q16  0XEF14
#define W25Q32  0XEF15
#define W25Q64  0XEF16

/* W25Q64芯片参数 */
#define W25Q64_SECTOR_SIZE  4096  /* 扇区大小：4KB */
#define W25Q64_SECTOR_NUM   2048  /* 扇区数量：8MB/4KB=2048 */

/* Flash地址分配表（用于存储位图和配置数据） */
#define FLASH_ASC16_ADDRESS             0        /* ASCII字库地址 */
#define FLASH_HZK16_ADDRESS             0x1000   /* 汉字字库地址 */
#define FLASH_SYSTEM_CONFIG_ADDRESS     0x43000  /* 系统配置地址 */

/* 位图数据地址（每个位图占160KB = 0x28000） */
#define FLASH_BITMAP1_SIZE_ADDRESS      0x50000
#define FLASH_BITMAP2_SIZE_ADDRESS      (FLASH_BITMAP1_SIZE_ADDRESS + 0X28000)
#define FLASH_BITMAP3_SIZE_ADDRESS      (FLASH_BITMAP2_SIZE_ADDRESS + 0X28000)
#define FLASH_BITMAP4_SIZE_ADDRESS      (FLASH_BITMAP3_SIZE_ADDRESS + 0X28000)
#define FLASH_BITMAP5_SIZE_ADDRESS      (FLASH_BITMAP4_SIZE_ADDRESS + 0X28000)
#define FLASH_BITMAP6_SIZE_ADDRESS      (FLASH_BITMAP5_SIZE_ADDRESS + 0X28000)
#define FLASH_BITMAPMAIN_SIZE_ADRESS    (FLASH_BITMAP6_SIZE_ADDRESS + 0X28000)
#define FLASH_BITMAPDS1302_SIZE_ADDRESS (FLASH_BITMAPMAIN_SIZE_ADRESS + 0X28000)
#define FLASH_BITMAPDS18B20_SIZE_ADDRESS (FLASH_BITMAPDS1302_SIZE_ADDRESS + 0X28000)
#define FLASH_BITMAPBLUE_SIZE_ADDRESS   (FLASH_BITMAPDS18B20_SIZE_ADDRESS + 0X28000)

/* W25Q64指令表（十六进制操作码） */
#define W25X_WriteEnable       0X06  /* 写使能：每次写入前必须先发此指令 */
#define W25X_WriteDisable      0X04  /* 写失能 */
#define W25X_ReadStatusReg     0X05  /* 读状态寄存器：检查是否忙 */
#define W25X_WriteStatusReg    0X01  /* 写状态寄存器 */
#define W25X_ReadData          0X03  /* 读数据 */
#define W25X_FastReadData      0X0B  /* 快速读数据 */
#define W25X_FastReadDual      0X3B  /* 双线读数据 */
#define W25X_PageProgram       0X02  /* 页编程：写入数据到一页(256字节) */
#define W25X_SectorErase       0X20  /* 扇区擦除：擦除4KB */
#define W25X_BlockErase        0X60  /* 块擦除：擦除64KB */
#define W25X_ChipErase         0X80  /* 整片擦除 */
#define W25X_PowerDown         0XB9  /* 掉电模式：省电 */
#define W25X_ReleasePowerDown  0XAB  /* 唤醒：从掉电模式恢复 */
#define W25X_DeviceID          0XD0  /* 读取设备ID */
#define W25X_ManufacturerID    0XD9  /* 读取制造商ID */
#define W25X_JedecDeviceID     0XD8  /* 读取JEDEC ID */

/*============================ 函数声明 ============================*/

/* LED/SPI引脚切换：x=1切换为SPI，x=0切换为LED */
void spi_flash_on(u8 x);

/* 初始化SPI2接口和Flash引脚 */
void spi_flash_init(void);

/* 设置SPI时钟分频，影响通信速度 */
void spi_set_speed(uint16_t speed);

/* SPI读写一个字节：发送TxData，同时接收一个字节返回 */
u8 spi_write_read(u8 TxData);

/* 读取Flash芯片ID，如返回0xEF16表示W25Q64 */
u16 spi_flash_read_id(void);

/* 读取Flash状态寄存器（bit0=1表示忙） */
u8 spi_flash_read_SR(void);

/* 写入Flash状态寄存器 */
void spi_flash_write_SR(u8 SR);

/* 写使能：每次写入/擦除前必须调用 */
void spi_flash_write_enable(void);

/* 写失能 */
void spi_flash_write_disable(void);

/* 读取1个字节，ReadAddr为24位地址 */
char spi_flash_read_byte(u32 ReadAddr);

/* 写入1个字节，WriteAddr为24位地址 */
void spi_flash_write_byte(char tmp, u32 WriteAddr);

/* 读取多个字节 */
void spi_flash_read(u8 *pBuffer, u32 ReadAddr, u16 NumByteToRead);

/* 写入多个字节（带擦除检查） */
void spi_flash_write(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite);

/* 页编程：在一页(256字节)内写入数据 */
void spi_flash_write_page(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite);

/* 扇区写入：在一个扇区(4KB)内写入数据 */
void spi_flash_write_sector(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite);

/* 无校验写入：直接写入，不检查是否需要擦除 */
void spi_flash_write_no_check(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite);

/* 擦除一个扇区(4KB) */
void spi_flash_erase_sector(u32 Dst_Addr);

/* 擦除整个芯片 */
void spi_flash_erase_chip(void);

/* 等待Flash忙状态结束 */
void spi_flash_wait_busy(void);

/* 进入掉电模式（省电） */
void spi_flash_powerdown(void);

/* 从掉电模式唤醒 */
void spi_flash_wake_up(void);

/* 简化写函数 */
void SpiFlsahWriteS(u8 *pBuffer, u32 WriteAddr, u16 NumByteToWrite);

/*============================ 简化宏 ============================*/
#define SpiFlashOn()              spi_flash_on(1)       /* 打开SPI模式 */
#define w25x_init()               spi_flash_init()      /* 初始化 */
#define w25x_readId()             spi_flash_read_id()   /* 读ID */
#define w25x_read(buf,addr,len)   spi_flash_read(buf,addr,len)       /* 读数据 */
#define w25x_write(buf,addr,len)  spi_flash_write_no_check(buf,addr,len) /* 写数据 */
#define w25x_writeS(buf,addr,len) SpiFlsahWriteS(buf,addr,len)       /* 写数据(带擦除) */
#define w25x_eraseSector(addr)    spi_flash_erase_sector(addr)        /* 擦除扇区 */
#define w25x_waitBusy()           spi_flash_wait_busy()               /* 等待忙 */

#endif
