#include "stm32f10x.h"                  // Device header
#define GPIO_Pin_SS   GPIO_Pin_4
#define GPIO_Pin_SCK GPIO_Pin_5
#define GPIO_Pin_MISO GPIO_Pin_6
#define GPIO_Pin_MOSI	GPIO_Pin_7

void MySPI_W_SS(uint8_t val){
	GPIO_WriteBit(GPIOA,GPIO_Pin_SS,(BitAction)val);
}

//硬件SPI  SPI1
void MySPI_Init(void){
	//1.使能时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1,ENABLE);
	
	//2.初始化GPIO
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_SCK | GPIO_Pin_MOSI;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;//复用推挽输出
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStruct);
	
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_SS;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;//复用推挽输出
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStruct);
	
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_MISO;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;//上拉输入
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStruct);
	
	//3.SPI初始化
	SPI_InitTypeDef SPI_InitStruct;
	SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
	SPI_InitStruct.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStruct.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStruct.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStruct.SPI_Mode = SPI_Mode_Master;
	SPI_InitStruct.SPI_NSS = SPI_NSS_Soft;
	SPI_InitStruct.SPI_CRCPolynomial = 7;
	SPI_Init(SPI1, &SPI_InitStruct);
	
	//4.使能SPI
	SPI_Cmd(SPI1,ENABLE);
	
	//设置引脚默认的高低电平
	MySPI_W_SS(1);
}

//起始条件
void MySPI_Start(void){
	MySPI_W_SS(0);
}

//终止条件
void MySPI_Stop(void){
	MySPI_W_SS(1);
}

//交换一个字节
uint8_t MySPI_Swap(uint8_t sendByte){
	while(SPI_I2S_GetFlagStatus(SPI1,SPI_I2S_FLAG_TXE)!=SET);//等待发送缓冲区为空
	SPI_I2S_SendData(SPI1,sendByte);//发送数据
	while(SPI_I2S_GetFlagStatus(SPI1,SPI_I2S_FLAG_RXNE)!=SET);//等待接收缓冲区非空
	return SPI_I2S_ReceiveData(SPI1);//接收数据
}

