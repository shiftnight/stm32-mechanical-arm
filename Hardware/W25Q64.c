#include "stm32f10x.h"                  // Device header
#include "MySPI.h"
#include "W25Q64_Ins.h"
void W25Q64_Init(void){
	MySPI_Init();
}

//写使能
void W25Q64_WriteEnable(void){
	MySPI_Start();
	MySPI_Swap(W25Q64_WRITE_ENABLE);
	MySPI_Stop();
}

//等待忙状态过去   
void W25Q64_WaitBusy(void){
	MySPI_Start();
	MySPI_Swap(W25Q64_READ_STATUS_REGISTER_1);
	while((MySPI_Swap(W25Q64_DUMMY_BYTE)&0x01)==1);
	MySPI_Stop();
}

//指定地址写---页编程
void W25Q64_PageProgram(uint32_t Addr,uint8_t *DataArray,uint16_t len){
	W25Q64_WriteEnable();//写使能
	MySPI_Start();//起始条件
	MySPI_Swap(W25Q64_PAGE_PROGRAM);//发送页编程指令
	MySPI_Swap(Addr>>16);//发送地址23~16
	MySPI_Swap((Addr>>8)&0x00FF);//发送地址15~8
	MySPI_Swap(Addr&0x00FF);//发送地址7~0
	//发送要写的数据
	for(int i=0;i<len;i++){
		MySPI_Swap(DataArray[i]);
	}
	//终止条件
	MySPI_Stop();
	W25Q64_WaitBusy();//等待忙状态过去
}


//指定地址读
void W25Q64_ReadData(uint32_t Addr,uint8_t *DataArray,uint16_t len){
	MySPI_Start();//起始条件
	MySPI_Swap(W25Q64_READ_DATA);//发送读指令
	MySPI_Swap(Addr>>16);//发送地址23~16
	MySPI_Swap((Addr>>8)&0x00FF);//发送地址15~8
	MySPI_Swap(Addr&0x00FF);//发送地址7~0
	//获取读的数据
	for(int i=0;i<len;i++){
		DataArray[i] = MySPI_Swap(W25Q64_DUMMY_BYTE);
	}
	//终止条件
	MySPI_Stop();
}

//扇区擦除
void W25Q64_SectorErase(uint32_t Addr){
	W25Q64_WriteEnable();//写使能
	MySPI_Start();//起始条件
	MySPI_Swap(W25Q64_SECTOR_ERASE_4KB);//发送指令
	MySPI_Swap(Addr>>16);//发送地址23~16
	MySPI_Swap((Addr>>8)&0x00FF);//发送地址15~8
	MySPI_Swap(Addr&0x00FF);//发送地址7~0
	//终止条件
	MySPI_Stop();
	W25Q64_WaitBusy();//等待忙状态过去
}

//获取ID
void W25Q64_ReadID(uint8_t *MID,uint16_t *DID){
	MySPI_Start();
	MySPI_Swap(W25Q64_JEDEC_ID);
	*MID = MySPI_Swap(0xFF);
	uint16_t DID_H = MySPI_Swap(0xFF);
	uint16_t DID_L = MySPI_Swap(0xFF);
	MySPI_Stop();
	*DID = (DID_H<<8)|DID_L;
}
