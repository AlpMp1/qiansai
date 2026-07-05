/***
	************************************************************************************************
	*	@file					main.c
	*	@version			V1.0
	*	@date					2023-11-28
	*	@author				Makerbase	
	*	@brief				SPI驱动显示屏，屏幕控制器 ST7789
  ************************************************************************************************
  * @description
	*
	*	实验平台：DRG ST-4 STM32F103RTC6 + 1.69寸液晶模块（型号：SPI169M1-240*280）
	*	Q群：366182133，Q群答疑团队随时为您提供技术支持
	*	B站：https://space.bilibili.com/486637340			
	*	官方网站：www.genbotter.com
	*	开发板购买：makerbase.taobao.com
	*	微信公众号：GenBotter
	************************************************************************************************
***/	
#include "stm32f10x.h"
#include "delay.h"
#include "led.h"  
#include "key.h"
#include "usart.h"
#include "lcd_spi_169.h"
#include "lcd_model.h"
#include <stdbool.h>


bool pattern = true;


/***************************************************************************************************
*	函 数 名: main
*	入口参数: 无
*	返 回 值: 无
*	函数功能: 运行主程序
*	说    明: 无
****************************************************************************************************/

int main(void)
{
	Delay_Init();		// 延时函数初始化
	LED_Init();			// LED初始化
	Usart1_Init(115200);	// USART1初始化函数
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置系统中断优先级分组2
	KEY_Init();
	SPI_LCD_Init();			// LCD初始化
	
	LED1_ON;
	LED2_OFF;
	LCD_Clear(); 											
	LCD_SetColor(LIGHT_GREEN);				
	LCD_SetTextFont(&CH_Font32);			
	LCD_DisplayText(60,100,"按K1进入");
	LCD_DisplayText(30,140,"LCD动态演示");
	while(1)
	{
		if(pattern )
		{
			LED1_Toggle;
			LED2_Toggle;
			Delay_ms(100);
		}
		else
		{
			LCD_Test();//文本显示
			LCD_Picture();//图片
			LCD_Line();//画线
			LCD_Rectangle();//矩形
			LCD_RouRectangle();//圆角矩形
			LCD_Ellipse();//椭圆
			LCD_Circle();//圆
			LCD_Triangle();//三角形
			LCD_Arc();//圆弧
			LCD_Polygon();//多边形
			LCD_Clock();//时钟
		}
	}
}

