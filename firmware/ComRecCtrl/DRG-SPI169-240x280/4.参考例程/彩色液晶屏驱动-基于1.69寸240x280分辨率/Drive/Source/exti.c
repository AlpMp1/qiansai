#include "exti.h"
#include "led.h"
#include "key.h"
#include "delay.h"
#include "usart.h"


//void EXTI2_IRQHandler(void)
//{
//	Delay_ms(10);//消抖
//	printf("test\r\n");
//	if(KEY_Scan() == KEY_ON)	 //按键KEY0
//	{
//		LED1_Toggle;
//		LED2_Toggle;
//	}		 
//	EXTI_ClearITPendingBit(EXTI_Line2);  //清除LINE2上的中断标志位  
//}


void EXTIX_Init(void)
{
 
	
	 GPIO_InitTypeDef GPIO_InitStruct;
	 // 配置外部中断线
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
 	 // 启用GPIOC时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
		//RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);	//使能复用功能时钟

    // 配置PC2为带上拉的输入
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;  // 上拉输入
    GPIO_Init(GPIOC, &GPIO_InitStruct);

		GPIO_EXTILineConfig(GPIO_PortSourceGPIOC,GPIO_PinSource2);
    // 配置外部中断线为PC2
    EXTI_InitStruct.EXTI_Line = EXTI_Line2;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;  // 上升沿触发
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);

    // 配置外部中断优先级
    NVIC_InitStruct.NVIC_IRQChannel = EXTI2_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
}


