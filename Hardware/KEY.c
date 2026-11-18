#include "stm32f10x.h"                  // Device header
#include "Delay.h"
//两个按键 PB1 PB11
void KEY_Init(void){
	//1.使能时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	
	//2.初始化
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_11;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;//输入模式。由于使用的是下接按键，输入方式必须是上拉输入
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&GPIO_InitStruct);
	
}

//获取按键状态
/*
通过一个函数获取两个按键的状态
如果按键1（PB1）被按--返回1
如果按键2（PB11）被按--返回2
没有按键被按--返回0

读取数据寄存器：
	uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
			读取输入数据寄存器指定位的值---读取指定端口的指定引脚输入的高低电平
	uint16_t GPIO_ReadInputData(GPIO_TypeDef* GPIOx);
			读取整个输入数据寄存器的值
	uint8_t GPIO_ReadOutputDataBit(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
			读取输出数据寄存器指定位的值---获取指定端口的指定引脚输出的高低电平
	uint16_t GPIO_ReadOutputData(GPIO_TypeDef* GPIOx)
			读取整个输出数据寄存器的值
*/
uint8_t KEY_Get(void){
	//判断按键1有没有被按--PB1
	if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_1)==0){
		Delay_ms(15);//消除按下抖动
		while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_1)==0);//等待松开按键
		Delay_ms(15);//消除松手抖动
		return 1;
	}
	
	//判断按键2有没有被按
	if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_11)==0){
		Delay_ms(15);//消除按下抖动
		while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_11)==0);//等待松开按键
		Delay_ms(15);//消除松手抖动
		return 2;
	}
	
	return 0;
}
