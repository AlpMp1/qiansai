/***
	************************************************************************************************
	*	@file					key.c
	*	@version			V1.0
	*	@date					2023-11-28
	*	@author				Makerbase	
	*	@brief				KEY相关函数
  ************************************************************************************************
  * @description
	*
	*	实验平台：DRG ST-4 STM32F103RTC6
	*	Q群：366182133，Q群答疑团队随时为您提供技术支持
	*	B站：https://space.bilibili.com/486637340			
	*	官方网站：www.genbotter.com
	*	开发板购买：makerbase.taobao.com
	*	微信公众号：GenBotter
	************************************************************************************************
***/	



#include "key.h"  
#include "led.h"
#include "usart.h"


/***************************************************************************************************
*	函 数 名: KEY_Init
*	入口参数: 无
*	返 回 值: 无
*	函数功能: 按键IO口初始化和外部中断初始化
*	说    明: 无
****************************************************************************************************/


void KEY_Init(void)
{		

	 GPIO_InitTypeDef GPIO_InitStruct;
	 // 配置外部中断线
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
 	 // 启用GPIOC时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);	//使能复用功能时钟

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



/***************************************************************************************************
*	函 数 名: KEY_Scan
*	入口参数: 无
*	返 回 值: uint8_t KEY_ON,KEY_OFF
*	函数功能: 按键扫描
*	说    明: KEY_ON - 按键按下，KEY_OFF - 按键放开
****************************************************************************************************/

uint8_t	KEY_Scan(void)
{
	if( GPIO_ReadInputDataBit ( KEY_PORT,KEY_PIN) == 0 )	//检测按键是否被按下
	{	
		Delay_ms(10);	//延时消抖
		if(GPIO_ReadInputDataBit ( KEY_PORT,KEY_PIN) == 0)	//再次检测是否为低电平
		{
			while(GPIO_ReadInputDataBit ( KEY_PORT,KEY_PIN) == 0);	//等待按键放开
			return KEY_ON;	//返回按键按下标志
		}
	}
	return KEY_OFF;	
}

/***************************************************************************************************
*	函 数 名: EXTI2_IRQHandler
*	入口参数: 无
*	返 回 值: 无
*	函数功能: 外部中断服务函数
*	说    明: 无
****************************************************************************************************/


void EXTI2_IRQHandler(void)
{
	 if (EXTI_GetITStatus(EXTI_Line2) != RESET) {
        // 立即清除中断标志位
        EXTI_ClearITPendingBit(EXTI_Line2);

        // 等待一段时间，进行延时消抖
        delay_ms(10);

        // 判断按键的实际状态
        if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_2) == Bit_SET) 
				{
            pattern  = !pattern;
        }
    }
}

/***************************************************************************************************
*	函 数 名: delay_ms
*	入口参数: uint32_t milliseconds
*	返 回 值: 无
*	函数功能: 简单的毫秒级延时函数
*	说    明: milliseconds -- 需要延时的毫秒级时间
****************************************************************************************************/

void delay_ms(uint32_t milliseconds) {
    volatile uint32_t counter = milliseconds * (SystemCoreClock / 1000 / 4); // 假设系统时钟为72MHz
    while (counter--);
}
