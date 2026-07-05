/***
	************************************************************************************************
	*	@file					lcd_spi_169.c
	*	@version			V1.0
	*	@date					2023-11-28
	*	@author				Makerbase	
	*	@brief				SPI驱动显示屏相关函数
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
	* @说明：
	*
	* 1.屏幕配置为16位RGB565格式
	************************************************************************************************
***/	


#include "lcd_spi_169.h"


static pFONT *LCD_AsciiFonts;		 // 英文字体，ASCII字符集
static pFONT *LCD_CHFonts;		   // 中文字体（同时也包含英文字体）
uint16_t  LCD_Buff[1024];        // LCD缓冲区，16位宽（每个像素点占2字节）
static int  _pointx=0;
static int 	_pointy=0;
unsigned char ScreenBuffer[8][240]={0};   //8  240
TypeRoate _RoateValue={{0,0},0,1}; //保存旋转操作的相关信息
//LCD相关参数结构体
struct	
{
	uint32_t Color;  				//	LCD当前画笔颜色
	uint32_t BackColor;			//	背景色
  uint8_t  ShowNum_Mode;	//	数字显示模式
	uint8_t  Direction;			//	显示方向
  uint16_t Width;         //	屏幕像素长度
  uint16_t Height;        //	屏幕像素宽度	
  uint8_t  X_Offset;      //	X坐标偏移，用于设置屏幕控制器的显存写入方式
  uint8_t  Y_Offset;      //	Y坐标偏移，用于设置屏幕控制器的显存写入方式
}LCD;


/*****************************************************************************************
* 函 数 名: LCD_Delay
* 入口参数: uint32_t a - 待延时的计数值
* 返 回 值: 无
* 函数功能: 延时函数
* 说    明: 使用循环方式实现粗略的延时，具体延时时间与系统时钟频率相关。
*             入口参数a表示循环计数值，通过多次循环来实现延时。
******************************************************************************************/

void LCD_Delay(uint32_t a)
{
	volatile uint16_t i;
	
	while (a --)				
	{
		for ( i = 0; i < 5000; i++);
	}
}


/*****************************************************************************************
* 函 数 名: LCD_GPIO_Config
* 入口参数: 无
* 返 回 值: 无
* 函数功能: 初始化LCD的GPIO口
* 说    明: 通过配置GPIO口实现LCD的初始化。设置SCK、SDA、CS、DC、背光等引脚的模式和
*            参数，其中SCK和SDA采用推挽复用输出，CS和DC采用推挽输出，背光引脚采用推挽
*            输出。在初始化完成后，将DC引脚拉高，处于写数据状态，CS引脚拉高，禁止通信，
*            背光引脚关闭，待初始化完成后再打开。
******************************************************************************************/
void LCD_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(GPIO_LCD_CS_CLK | GPIO_LCD_DC_CLK | GPIO_LCD_Backlight_CLK | GPIO_LCD_SCK_CLK | GPIO_LCD_SDA_CLK, ENABLE); // 开启LCD控制引脚的时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);     // 开启IO口复用时钟
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE); // 禁用JTAG 只使用SWD，不然PA15、PB3、PB4无法正常使用
    GPIO_PinRemapConfig(GPIO_Remap_SPI1, ENABLE);            // 重定义SPI1的IO口，使用PB3/PB5分别作为 SCLK/MOSI

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  // 推挽复用输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // 速度等级

    GPIO_InitStructure.GPIO_Pin = LCD_SCK_PIN; // 初始化SCK引脚
    GPIO_Init(LCD_SCK_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = LCD_SDA_PIN; // 初始化SDA引脚
    GPIO_Init(LCD_SDA_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // 速度等级

    GPIO_InitStructure.GPIO_Pin = LCD_CS_PIN; // 初始化CS引脚
    GPIO_Init(LCD_CS_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = LCD_DC_PIN; // 初始化DC引脚
    GPIO_Init(LCD_DC_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = LCD_Backlight_PIN; // 初始化背光引脚
    GPIO_Init(LCD_Backlight_PORT, &GPIO_InitStructure);

    LCD_DC_Data;    // DC引脚拉高，默认处于写数据状态
    LCD_CS_H;       // 拉高片选，禁止通信
    LCD_Backlight_OFF; // 先关闭背光，初始化完成之后再打开
}

/*****************************************************************************************
* 函 数 名: LCD_SPI_Config
* 入口参数: 无
* 返 回 值: 无
* 函数功能: 初始化LCD的用到的SPI口
* 说    明: 通过配置SPI口实现LCD的初始化。设置SPI的工作模式、数据宽度、极性、相位等参数，
*             并可以根据实际需求设置时钟分频系数。最后使能SPI1。
******************************************************************************************/
void LCD_SPI_Config(void)
{		
    SPI_InitTypeDef SPI_InitStructure;	

    LCD_SPI_APBClock_Enable;	// 使能外设时钟

    SPI_InitStructure.SPI_Direction 				= SPI_Direction_1Line_Tx;	// 单线只发模式
    SPI_InitStructure.SPI_Mode		 				= SPI_Mode_Master;			// 主机
    SPI_InitStructure.SPI_DataSize	 			= SPI_DataSize_8b;			// 8位数据宽度
    SPI_InitStructure.SPI_CPOL 					= SPI_CPOL_Low;				// SCLK时钟空闲为低电平
    SPI_InitStructure.SPI_CPHA 					= SPI_CPHA_1Edge;				// 奇数跳变沿有效
    SPI_InitStructure.SPI_NSS						= SPI_NSS_Soft;				// 软件控制片选信号

    // 用户可以把分频系数设置为2，将SPI时钟提高到36，可以大大提高刷屏速度	
    // 但是因为ST的103手册里，限定了SPI的最高速度为18M，所以需要根据您的实际工况去选择是否使用
    SPI_InitStructure.SPI_BaudRatePrescaler 	= SPI_BaudRatePrescaler_2;	// 4分频，此时SPI时钟为18MHz
    
    SPI_InitStructure.SPI_FirstBit 				= SPI_FirstBit_MSB;			// 先发送高位
    SPI_InitStructure.SPI_CRCPolynomial 		= 7;								// CRC校验项，这里用不到
    SPI_Init(LCD_SPI , &SPI_InitStructure);		// 进行初始化

    SPI_Cmd(LCD_SPI , ENABLE);		// 使能SPI1
}


/*****************************************************************************************
* 函 数 名: LCD_WriteCommand
* 入口参数: uint8_t lcd_command - 需要写入的控制指令
* 返 回 值: 无
* 函数功能: 用于向LCD写入控制字
* 说    明: 通过SPI总线向LCD发送控制指令。在发送之前，检查SPI是否空闲，等待上一次通信
*            完成。将DC引脚置低，表示写入控制指令，然后发送控制指令数据，并等待发送缓
*            冲区清空和通信完成。
******************************************************************************************/
void LCD_WriteCommand(uint8_t lcd_command)
{
    while ((LCD_SPI->SR & SPI_I2S_FLAG_BSY) != RESET);  // 先判断SPI是否空闲，等待通信完成

    LCD_DC_Command;  // DC引脚输出低，表示写指令

    LCD_SPI->DR = lcd_command;  // 发送数据
    while ((LCD_SPI->SR & SPI_I2S_FLAG_TXE) == 0);  // 等待发送缓冲区清空
    while ((LCD_SPI->SR & SPI_I2S_FLAG_BSY) != RESET);  // 等待通信完成

    LCD_DC_Data;  // DC引脚输出高，表示写数据
}

/*****************************************************************************************
* 函 数 名: LCD_WriteData_8bit
* 入口参数: uint8_t lcd_data - 待写入的8位数据
* 返 回 值: 无
* 函数功能: 通过SPI总线向LCD写入8位数据
* 说    明: 将待写入的8位数据写入SPI数据寄存器，然后等待发送缓冲区清空，以确保数据成功传输。
******************************************************************************************/
void LCD_WriteData_8bit(uint8_t lcd_data)
{
    LCD_SPI->DR = lcd_data;  // 发送数据
    while ((LCD_SPI->SR & SPI_I2S_FLAG_TXE) == 0);  // 等待发送缓冲区清空
}



/*****************************************************************************************
* 函 数 名: LCD_WriteData_16bit
* 入口参数: uint16_t lcd_data - 待写入的16位数据
* 返 回 值: 无
* 函数功能: 通过SPI总线向LCD写入16位数据
* 说    明: 将待写入的16位数据拆分为高8位和低8位，先发送高8位，等待发送缓冲区清空，
*            然后发送低8位，再次等待发送缓冲区清空，以确保数据成功传输。
******************************************************************************************/
void LCD_WriteData_16bit(uint16_t lcd_data)
{
    LCD_SPI->DR = lcd_data >> 8;  // 发送数据，高8位
    while ((LCD_SPI->SR & SPI_I2S_FLAG_TXE) == 0);  // 等待发送缓冲区清空

    LCD_SPI->DR = lcd_data;  // 发送数据，低8位
    while ((LCD_SPI->SR & SPI_I2S_FLAG_TXE) == 0);  // 等待发送缓冲区清空
}

/*****************************************************************************************
* 函 数 名: LCD_WriteBuff
* 入口参数: uint16_t *DataBuff - 待写入数据的缓冲区指针
*           uint16_t DataSize - 待写入数据的大小
* 返 回 值: 无
* 函数功能: 通过SPI总线向LCD写入一段数据
* 说    明: 关闭SPI，切换成16位数据格式，使能SPI。然后，通过循环将数据缓冲区中的数据
*            逐个发送到SPI数据寄存器，等待发送缓冲区清空，确保数据成功传输。最后，等待
*            通信完成，关闭SPI，切换成8位数据格式，再次使能SPI。
******************************************************************************************/
void LCD_WriteBuff(uint16_t *DataBuff, uint16_t DataSize)
{
    uint32_t i;

    LCD_SPI->CR1 &= 0xFFBF;                  // 关闭SPI
    LCD_SPI->CR1 |= SPI_DataSize_16b;        // 切换成16位数据格式
    LCD_SPI->CR1 |= 0x0040;                  // 使能SPI

    LCD_CS_L;   // 片选拉低，使能IC

    for (i = 0; i < DataSize; i++)
    {
        LCD_SPI->DR = DataBuff[i];
        while ((LCD_SPI->SR & SPI_I2S_FLAG_TXE) == 0);
    }

    while ((LCD_SPI->SR & SPI_I2S_FLAG_BSY) != RESET); // 等待通信完成
    LCD_CS_H;   // 片选拉高

    LCD_SPI->CR1 &= 0xFFBF; // 关闭SPI
    LCD_SPI->CR1 &= 0xF7FF; // 切换成8位数据格式
    LCD_SPI->CR1 |= 0x0040; // 使能SPI
}



/*****************************************************************************************
* 函 数 名: SPI_LCD_Init
* 入口参数: 无
* 返 回 值: 无
* 函数功能: 初始化SPI LCD
* 说    明: 初始化LCD的GPIO口和SPI口，设置LCD控制器的各种参数，包括显存访问方式、像素
*            格式、电压设置等。然后发送打开显示指令和退出休眠指令，等待通信完成，最后
*            打开背光。
******************************************************************************************/
void SPI_LCD_Init(void)
{         
    LCD_GPIO_Config();   // 初始化IO口
    LCD_SPI_Config();    // 初始化SPI配置
    
    LCD_Delay(5);        // 屏幕刚完成复位时（包括上电复位），需要等待5ms才能发送指令
    
    LCD_CS_L; // 片选拉低，使能IC，开始通信

    LCD_WriteCommand(0x36);     // 显存访问控制 指令，用于设置访问显存的方式
    LCD_WriteData_8bit(0x00);   // 配置成从上到下、从左到右，RGB像素格式

    LCD_WriteCommand(0x3A);     // 接口像素格式 指令，用于设置使用12位、16位还是18位色
    LCD_WriteData_8bit(0x05);   // 此处配置成16位像素格式

    // 接下来很多都是电压设置指令，直接使用厂家给设定值
    LCD_WriteCommand(0xB2);
    LCD_WriteData_8bit(0x0C);
    LCD_WriteData_8bit(0x0C); 
    LCD_WriteData_8bit(0x00); 
    LCD_WriteData_8bit(0x33); 
    LCD_WriteData_8bit(0x33); 

    LCD_WriteCommand(0xB7);     // 栅极电压设置指令    
    LCD_WriteData_8bit(0x35);   // VGH = 13.26V，VGL = -10.43V

    LCD_WriteCommand(0xBB);     // 公共电压设置指令
    LCD_WriteData_8bit(0x19);   // VCOM = 1.35V

    LCD_WriteCommand(0xC0);
    LCD_WriteData_8bit(0x2C);

    LCD_WriteCommand(0xC2);     // VDV 和 VRH 来源设置
    LCD_WriteData_8bit(0x01);   // VDV 和 VRH 由用户自由配置

    LCD_WriteCommand(0xC3);     // VRH电压 设置指令  
    LCD_WriteData_8bit(0x12);   // VRH电压 = 4.6 + (vcom + vcom offset + vdv)

    LCD_WriteCommand(0xC4);     // VDV电压 设置指令    
    LCD_WriteData_8bit(0x20);   // VDV电压 = 0v

    LCD_WriteCommand(0xC6);     // 正常模式的帧率控制指令
    LCD_WriteData_8bit(0x0F);   // 设置屏幕控制器的刷新帧率为60帧    

    LCD_WriteCommand(0xD0);     // 电源控制指令
    LCD_WriteData_8bit(0xA4);   // 无效数据，固定写入0xA4
    LCD_WriteData_8bit(0xA1);   // AVDD = 6.8V，AVDD = -4.8V，VDS = 2.3V

    LCD_WriteCommand(0xE0);     // 正极电压伽马值设定
    LCD_WriteData_8bit(0xD0);
    LCD_WriteData_8bit(0x04);
    LCD_WriteData_8bit(0x0D);
    LCD_WriteData_8bit(0x11);
    LCD_WriteData_8bit(0x13);
    LCD_WriteData_8bit(0x2B);
    LCD_WriteData_8bit(0x3F);
    LCD_WriteData_8bit(0x54);
    LCD_WriteData_8bit(0x4C);
    LCD_WriteData_8bit(0x18);
    LCD_WriteData_8bit(0x0D);
    LCD_WriteData_8bit(0x0B);
    LCD_WriteData_8bit(0x1F);
    LCD_WriteData_8bit(0x23);

    LCD_WriteCommand(0xE1);     // 负极电压伽马值设定
    LCD_WriteData_8bit(0xD0);
    LCD_WriteData_8bit(0x04);
    LCD_WriteData_8bit(0x0C);
    LCD_WriteData_8bit(0x11);
    LCD_WriteData_8bit(0x13);
    LCD_WriteData_8bit(0x2C);
		LCD_WriteData_8bit(0x3F);
    LCD_WriteData_8bit(0x44);
    LCD_WriteData_8bit(0x51);
    LCD_WriteData_8bit(0x2F);
    LCD_WriteData_8bit(0x1F);
    LCD_WriteData_8bit(0x1F);
    LCD_WriteData_8bit(0x20);
    LCD_WriteData_8bit(0x23);

    LCD_WriteCommand(0x21);     // 打开反显，因为面板是常黑型，操作需要反过来

    // 退出休眠指令，LCD控制器在刚上电、复位时，会自动进入休眠模式 ，因此操作屏幕之前，需要退出休眠  
    LCD_WriteCommand(0x11);     // 退出休眠 指令
    LCD_Delay(120);             // 需要等待120ms，让电源电压和时钟电路稳定下来
    
    // 打开显示指令，LCD控制器在刚上电、复位时，会自动关闭显示 
    LCD_WriteCommand(0x29);     // 打开显示

    while ((LCD_SPI->SR & SPI_I2S_FLAG_BSY) != RESET); // 等待通信完成
    LCD_CS_H; // 片选拉高        

    // 以下进行一些驱动的默认设置
    LCD_SetDirection(Direction_V);          // 设置显示方向
    LCD_SetBackColor(LCD_BLACK);            // 设置背景色
    LCD_SetColor(LCD_WHITE);                // 设置画笔色  
    LCD_Clear();                            // 清屏

    LCD_SetAsciiFont(&ASCII_Font24);        // 设置默认字体

    // 全部设置完毕之后，打开背光    
    LCD_Backlight_ON;  // 引脚输出高电平点亮背光
}

/*****************************************************************************************
*	函 数 名: LCD_SetAddress
*	入口参数:
*       x1 - 矩形左上角 X 坐标
*       y1 - 矩形左上角 Y 坐标
*       x2 - 矩形右下角 X 坐标
*       y2 - 矩形右下角 Y 坐标
*
*	返 回 值: 无
*	函数功能: 设置LCD显存访问的区域范围
*	说    明: 在设置完区域之后，可以通过后续的数据写入指令向该区域写入颜色数据
******************************************************************************************/
void LCD_SetAddress(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD_CS_L; // 片选拉低，使能IC

    LCD_WriteCommand(0x2a); // 列地址设置，即X坐标
    LCD_WriteData_16bit(x1 + LCD.X_Offset);
    LCD_WriteData_16bit(x2 + LCD.X_Offset);

    LCD_WriteCommand(0x2b); // 行地址设置，即Y坐标
    LCD_WriteData_16bit(y1 + LCD.Y_Offset);
    LCD_WriteData_16bit(y2 + LCD.Y_Offset);

    LCD_WriteCommand(0x2c); // 开始写入显存，即要显示的颜色数据

    while ((LCD_SPI->SR & SPI_I2S_FLAG_BSY) != RESET); // 等待通信完成
    LCD_CS_H; // 片选拉高
}



/*****************************************************************************************
* 函 数 名: LCD_SetColor
* 入口参数: uint32_t Color - 待设置的颜色，格式为RGB888
* 返 回 值: 无
* 函数功能: 将RGB888格式的颜色值转换为RGB565格式，并将颜色值写入全局LCD参数
* 说    明: 将待设置的颜色值的RGB通道拆分，经过移位和合并操作，将其转换为RGB565格式，
*            然后写入全局LCD参数中。
******************************************************************************************/
void LCD_SetColor(uint32_t Color)
{
    uint16_t Red_Value = 0, Green_Value = 0, Blue_Value = 0; // 各个颜色通道的值

    Red_Value   = (uint16_t)((Color & 0x00F80000) >> 8);   // 转换成 16位 的RGB565颜色
    Green_Value = (uint16_t)((Color & 0x0000FC00) >> 5);
    Blue_Value  = (uint16_t)((Color & 0x000000F8) >> 3);

    LCD.Color = (uint16_t)(Red_Value | Green_Value | Blue_Value);  // 将颜色写入全局LCD参数
}

/*****************************************************************************************
* 函 数 名: LCD_SetBackColor
* 入口参数: uint32_t Color - 待设置的背景颜色，格式为RGB888
* 返 回 值: 无
* 函数功能: 将RGB888格式的颜色值转换为RGB565格式，并将背景颜色值写入全局LCD参数
* 说    明: 将待设置的颜色值的RGB通道拆分，经过移位和合并操作，将其转换为RGB565格式，
*            然后写入全局LCD参数中作为背景颜色。
******************************************************************************************/
void LCD_SetBackColor(uint32_t Color)
{
    uint16_t Red_Value = 0, Green_Value = 0, Blue_Value = 0; // 各个颜色通道的值

    Red_Value   = (uint16_t)((Color & 0x00F80000) >> 8);   // 转换成 16位 的RGB565颜色
    Green_Value = (uint16_t)((Color & 0x0000FC00) >> 5);
    Blue_Value  = (uint16_t)((Color & 0x000000F8) >> 3);

    LCD.BackColor = (uint16_t)(Red_Value | Green_Value | Blue_Value);    // 将颜色写入全局LCD参数
}


/*****************************************************************************************
* 函 数 名: LCD_SetDirection
* 入口参数: uint8_t direction - 显示方向，可选值为Direction_H、Direction_V、
*                               Direction_H_Flip、Direction_V_Flip
* 返 回 值: 无
* 函数功能: 设置LCD的显示方向，并更新全局LCD参数
* 说    明: 根据输入的显示方向值，调用相应的显存访问控制指令，设置LCD的显示方向，
*            并更新全局LCD参数，包括坐标偏移量、宽度和高度。
******************************************************************************************/
void LCD_SetDirection(uint8_t direction)
{
    LCD.Direction = direction;    // 写入全局LCD参数

    LCD_CS_L;    // 片选拉低，使能IC

    if (direction == Direction_H)   // 横屏显示
    {
        LCD_WriteCommand(0x36);         // 显存访问控制 指令，用于设置访问显存的方式
        LCD_WriteData_8bit(0x70);       // 横屏显示
        LCD.X_Offset   = 20;            // 设置控制器坐标偏移量
        LCD.Y_Offset   = 0;   
        LCD.Width      = LCD_Height;    // 重新赋值长、宽
        LCD.Height     = LCD_Width;     
    }
    else if (direction == Direction_V)
    {
        LCD_WriteCommand(0x36);         // 显存访问控制 指令，用于设置访问显存的方式
        LCD_WriteData_8bit(0x00);       // 垂直显示
        LCD.X_Offset   = 0;             // 设置控制器坐标偏移量
        LCD.Y_Offset   = 20;     
        LCD.Width      = LCD_Width;     // 重新赋值长、宽
        LCD.Height     = LCD_Height;                        
    }
    else if (direction == Direction_H_Flip)
    {
        LCD_WriteCommand(0x36);         // 显存访问控制 指令，用于设置访问显存的方式
        LCD_WriteData_8bit(0xA0);       // 横屏显示，并上下翻转，RGB像素格式
        LCD.X_Offset   = 20;            // 设置控制器坐标偏移量
        LCD.Y_Offset   = 0;      
        LCD.Width      = LCD_Height;    // 重新赋值长、宽
        LCD.Height     = LCD_Width;     
    }
    else if (direction == Direction_V_Flip)
    {
        LCD_WriteCommand(0x36);         // 显存访问控制 指令，用于设置访问显存的方式
        LCD_WriteData_8bit(0xC0);       // 垂直显示 ，并上下翻转，RGB像素格式
        LCD.X_Offset   = 0;             // 设置控制器坐标偏移量
        LCD.Y_Offset   = 20;     
        LCD.Width      = LCD_Width;     // 重新赋值长、宽
        LCD.Height     = LCD_Height;                        
    }   
    
    while ((LCD_SPI->SR & SPI_I2S_FLAG_BSY) != RESET); // 等待通信完成
    LCD_CS_H;    // 片选拉高            
}


/*****************************************************************************************
* 函 数 名: LCD_SetAsciiFont
* 入口参数: pFONT *Asciifonts - 指向ASCII字体数组的指针
* 返 回 值: 无
* 函数功能: 设置LCD当前使用的ASCII字体数组
* 说    明: 将传入的ASCII字体数组指针赋值给全局LCD参数LCD_AsciiFonts，
*            以便后续绘制ASCII字符时使用。
******************************************************************************************/
void LCD_SetAsciiFont(pFONT *Asciifonts)
{
    LCD_AsciiFonts = Asciifonts;
}

/*****************************************************************************************
* 函 数 名: LCD_Clear
* 入口参数: 无
* 返 回 值: 无
* 函数功能: 清除LCD屏幕内容
* 说    明: 通过设置LCD显示区域的坐标，然后循环填充背景色，实现清除屏幕内容的效果。
*            在填充每个像素时，通过SPI总线向LCD写入背景色数据。
******************************************************************************************/
void LCD_Clear(void)
{
    uint32_t i;

    LCD_SetAddress(0, 0, LCD.Width - 1, LCD.Height - 1); // 设置坐标

    LCD_SPI->CR1 &= 0xFFBF;               // 关闭SPI
    LCD_SPI->CR1 |= SPI_DataSize_16b;     // 切换成16位数据格式
    LCD_SPI->CR1 |= 0x0040;               // 使能SPI

    LCD_CS_L; // 片选拉低，使能IC

    for (i = 0; i < LCD.Width * LCD.Height; i++)
    {
        LCD_SPI->DR = LCD.BackColor;
        while ((LCD_SPI->SR & SPI_I2S_FLAG_TXE) == 0);
    }

    while ((LCD_SPI->SR & SPI_I2S_FLAG_BSY) != RESET); // 等待通信完成
    LCD_CS_H; // 片选拉高

    LCD_SPI->CR1 &= 0xFFBF; // 关闭SPI
    LCD_SPI->CR1 &= 0xF7FF; // 切换成8位数据格式
    LCD_SPI->CR1 |= 0x0040; // 使能SPI
}



/*****************************************************************************************
* 函 数 名: LCD_ClearRect
* 入口参数: uint16_t x - 矩形左上角的X坐标
*           uint16_t y - 矩形左上角的Y坐标
*           uint16_t width - 矩形的宽度
*           uint16_t height - 矩形的高度
* 返 回 值: 无
* 函数功能: 清除LCD屏幕指定矩形区域内容
* 说    明: 通过设置LCD显示区域的坐标，然后循环填充背景色，实现清除矩形区域内容的效果。
*            在填充每个像素时，通过SPI总线向LCD写入背景色数据。
******************************************************************************************/
void LCD_ClearRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    uint16_t i;

    LCD_SetAddress(x, y, x + width - 1, y + height - 1); // 设置坐标

    LCD_SPI->CR1 &= 0xFFBF;               // 关闭SPI
    LCD_SPI->CR1 |= SPI_DataSize_16b;     // 切换成16位数据格式
    LCD_SPI->CR1 |= 0x0040;               // 使能SPI

    LCD_CS_L; // 片选拉低，使能IC

    for (i = 0; i < width * height; i++)
    {
        LCD_SPI->DR = LCD.BackColor;
        while ((LCD_SPI->SR & SPI_I2S_FLAG_TXE) == 0);
    }

    while ((LCD_SPI->SR & SPI_I2S_FLAG_BSY) != RESET); // 等待通信完成
    LCD_CS_H; // 片选拉高

    LCD_SPI->CR1 &= 0xFFBF; // 关闭SPI
    LCD_SPI->CR1 &= 0xF7FF; // 切换成8位数据格式
    LCD_SPI->CR1 |= 0x0040; // 使能SPI
}



/*****************************************************************************************
* 函 数 名: LCD_DrawPoint
* 入口参数: uint16_t x - 点的X坐标
*           uint16_t y - 点的Y坐标
*           uint32_t color - 点的颜色
* 返 回 值: 无
* 函数功能: 在LCD屏幕指定位置画点
* 说    明: 通过设置LCD显示区域的坐标，然后将指定颜色数据通过SPI总线写入LCD，实现在指定
*            位置画点的效果。在该函数中，画点的范围是一个像素。
******************************************************************************************/
void LCD_DrawPoint(uint16_t x, uint16_t y, uint32_t color)
{
    LCD_SetAddress(x, y, x, y); // 设置坐标

    LCD_CS_L; // 片选拉低，使能IC

    LCD_WriteData_16bit(LCD.Color);

    while ((LCD_SPI->SR & SPI_I2S_FLAG_BSY) != RESET); // 等待通信完成
    LCD_CS_H; // 片选拉高
}


/*****************************************************************************************
* 函 数 名: LCD_DisplayChar
* 入口参数: uint16_t x - 字符的X坐标
*           uint16_t y - 字符的Y坐标
*           uint8_t c - 要显示的ASCII字符
* 返 回 值: 无
* 函数功能: 在LCD屏幕指定位置显示ASCII字符
* 说    明: 通过遍历字符的二进制模式，根据模式中的1和0，将颜色数据写入LCD缓冲区，然后通过
*            SPI总线写入LCD显示区域，实现在指定位置显示ASCII字符的效果。
******************************************************************************************/
void LCD_DisplayChar(uint16_t x, uint16_t y, uint8_t c)
{
    uint16_t index = 0, counter = 0, i = 0, w = 0; // 计数变量
    uint8_t disChar; // 存储字符的模式

    c = c - 32; // 计算ASCII字符的偏移

    LCD_CS_L; // 片选拉低，使能IC

    for (index = 0; index < LCD_AsciiFonts->Sizes; index++)
    {
        disChar = LCD_AsciiFonts->pTable[c * LCD_AsciiFonts->Sizes + index]; // 获取字符的模式
        for (counter = 0; counter < 8; counter++)
        {
            if (disChar & 0x01)
            {
                LCD_Buff[i] = LCD.Color; // 当前模式位为1时，使用画笔色绘点
            }
            else
            {
                LCD_Buff[i] = LCD.BackColor; // 否则使用背景色绘制点
            }
            disChar >>= 1;
            i++;
            w++;
            if (w == LCD_AsciiFonts->Width) // 如果写入的数据达到了字符宽度，则退出当前循环，进入下一字符的写入的绘制
            {
                w = 0;
                break;
            }
        }
    }
    LCD_SetAddress(x, y, x + LCD_AsciiFonts->Width - 1, y + LCD_AsciiFonts->Height - 1); // 设置坐标
    LCD_WriteBuff(LCD_Buff, LCD_AsciiFonts->Width * LCD_AsciiFonts->Height);           // 写入显存
}


/*****************************************************************************************
* 函 数 名: LCD_DisplayString
* 入口参数: uint16_t x - 字符串的起始X坐标
*           uint16_t y - 字符串的起始Y坐标
*           char *p - 要显示的字符串
* 返 回 值: 无
* 函数功能: 在LCD屏幕指定位置显示字符串
* 说    明: 通过调用`LCD_DisplayChar`函数，循环遍历字符串中的每个字符，根据字符宽度，
*            在LCD上显示整个字符串。
******************************************************************************************/
void LCD_DisplayString(uint16_t x, uint16_t y, char *p)
{
    while ((x < LCD.Width) && (*p != 0)) // 判断显示坐标是否超出显示区域并且字符是否为空字符
    {
        LCD_DisplayChar(x, y, *p);
        x += LCD_AsciiFonts->Width; // 显示下一个字符
        p++;                        // 取下一个字符地址
    }
}



/*****************************************************************************************
* 函 数 名: LCD_SetTextFont
* 入口参数: pFONT *fonts - 中文字体
* 返 回 值: 无
* 函数功能: 设置LCD的中文字体，同时根据中文字体的宽度，设置对应的ASCII字符字体。
* 说    明: 中文字体用于显示中文字符，而ASCII字符字体则用于显示英文字符。
******************************************************************************************/
void LCD_SetTextFont(pFONT *fonts)
{
    LCD_CHFonts = fonts; // 设置中文字体
    switch (fonts->Width)
    {
    case 12:
        LCD_AsciiFonts = &ASCII_Font12;
        break; // 设置ASCII字符的字体为 1206
    case 16:
        LCD_AsciiFonts = &ASCII_Font16;
        break; // 设置ASCII字符的字体为 1608
    case 20:
        LCD_AsciiFonts = &ASCII_Font20;
        break; // 设置ASCII字符的字体为 2010
    case 24:
        LCD_AsciiFonts = &ASCII_Font24;
        break; // 设置ASCII字符的字体为 2412
    case 32:
        LCD_AsciiFonts = &ASCII_Font32;
        break; // 设置ASCII字符的字体为 3216
    default:
        break;
    }
}

/*****************************************************************************************
* 函 数 名: LCD_DisplayChinese
* 入口参数: uint16_t x - 起始横坐标
*           uint16_t y - 起始纵坐标
*           char *pText - 中文字符字符串
* 返 回 值: 无
* 函数功能: 在LCD上显示中文字符字符串
* 说    明: 遍历中文字体字模列表，通过对比输入的中文字符编码，定位字模的地址。然后，
*           根据字模数据，绘制中文字符在LCD上。
******************************************************************************************/
void LCD_DisplayChinese(uint16_t x, uint16_t y, char *pText)
{
    uint16_t i = 0, index = 0, counter = 0; // 计数变量
    uint16_t addr;                           // 字模地址
    uint8_t disChar;                         // 字模的值
    uint16_t Xaddress = 0;                  // 水平坐标

    while (1)
    {
        // 对比数组中的汉字编码，用以定位该汉字字模的地址
        if (*(LCD_CHFonts->pTable + (i + 1) * LCD_CHFonts->Sizes + 0) == *pText && *(LCD_CHFonts->pTable + (i + 1) * LCD_CHFonts->Sizes + 1) == *(pText + 1))
        {
            addr = i; // 字模地址偏移
            break;
        }
        i += 2; // 每个中文字符编码占两字节

        if (i >= LCD_CHFonts->Table_Rows)
            break; // 字模列表中无相应的汉字
    }
    i = 0;
    for (index = 0; index < LCD_CHFonts->Sizes; index++)
    {
        disChar = *(LCD_CHFonts->pTable + (addr)*LCD_CHFonts->Sizes + index); // 获取相应的字模地址

        for (counter = 0; counter < 8; counter++)
        {
            if (disChar & 0x01)
            {
                LCD_Buff[i] = LCD.Color; // 当前模值不为0时，使用画笔色绘点
            }
            else
            {
                LCD_Buff[i] = LCD.BackColor; // 否则使用背景色绘制点
            }
            i++;
            disChar >>= 1;
            Xaddress++; // 水平坐标自加

            if (Xaddress == LCD_CHFonts->Width) // 如果水平坐标达到了字符宽度，则退出当前循环
            {                                     // 进入下一行的绘制
                Xaddress = 0;
                break;
            }
        }
    }
    LCD_SetAddress(x, y, x + LCD_CHFonts->Width - 1, y + LCD_CHFonts->Height - 1); // 设置坐标
    LCD_WriteBuff(LCD_Buff, LCD_CHFonts->Width * LCD_CHFonts->Height);             // 写入显存
}

/*****************************************************************************************
* 函 数 名: LCD_DisplayText
* 入口参数: uint16_t x - 起始横坐标
*           uint16_t y - 起始纵坐标
*           char *pText - 待显示的文本字符串
* 返 回 值: 无
* 函数功能: 在LCD上显示文本字符串，支持ASCII和中文混排
* 说    明: 遍历输入的文本字符串，判断字符是否为ASCII码，如果是，则调用显示ASCII字符的函数，
*           如果是汉字，则调用显示汉字字符的函数。同时，根据字符类型，调整水平坐标，实现字符
*           的横向排列。
******************************************************************************************/
void LCD_DisplayText(uint16_t x, uint16_t y, char *pText)
{
    while (*pText != 0) // 判断是否为空字符
    {
        if (*pText <= 0x7F) // 判断是否为ASCII码
        {
            LCD_DisplayChar(x, y, *pText);      // 显示ASCII
            x += LCD_AsciiFonts->Width;         // 水平坐标调到下一个字符处
            pText++;                            // 字符串地址+1
        }
        else // 若字符为汉字
        {
            LCD_DisplayChinese(x, y, pText);    // 显示汉字
            x += LCD_CHFonts->Width;            // 水平坐标调到下一个字符处
            pText += 2;                         // 字符串地址+2，汉字的编码要2字节
        }
    }
}


/*****************************************************************************************
* 函 数 名: LCD_DrawLine
* 入口参数: uint16_t x1 - 起始点横坐标
*           uint16_t y1 - 起始点纵坐标
*           uint16_t x2 - 终点横坐标
*           uint16_t y2 - 终点纵坐标
* 返 回 值: 无
* 函数功能: 在LCD上绘制直线
* 说    明: 使用 Bresenham 算法绘制直线，算法基于整数运算，效率较高。
*           具体步骤为计算直线的差值、方向、增量等参数，然后在循环中按照算法逐点绘制直线。
*           函数中涉及到的宏 ABS 用于获取一个数的绝对值。
******************************************************************************************/
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0,
            yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0,
            curpixel = 0;

    deltax = ABS(x2 - x1);        /* 计算 x 方向的差值 */
    deltay = ABS(y2 - y1);        /* 计算 y 方向的差值 */
    x = x1;                       /* 将 x 起始值设为第一个像素点的横坐标 */
    y = y1;                       /* 将 y 起始值设为第一个像素点的纵坐标 */

    /* 确定 x 和 y 的增量方向，以及步进值 */
    if (x2 >= x1)
    {
        xinc1 = 1;
        xinc2 = 1;
    }
    else
    {
        xinc1 = -1;
        xinc2 = -1;
    }

    if (y2 >= y1)
    {
        yinc1 = 1;
        yinc2 = 1;
    }
    else
    {
        yinc1 = -1;
        yinc2 = -1;
    }

    /* 确定绘制直线的方向 */
    if (deltax >= deltay)
    {
        xinc1 = 0;
        yinc2 = 0;
        den = deltax;
        num = deltax / 2;
        numadd = deltay;
        numpixels = deltax;
    }
    else
    {
        xinc2 = 0;
        yinc1 = 0;
        den = deltay;
        num = deltay / 2;
        numadd = deltax;
        numpixels = deltay;
    }

    /* 使用 Bresenham 算法绘制直线 */
    for (curpixel = 0; curpixel <= numpixels; curpixel++)
    {
        LCD_DrawPoint(x, y, LCD.Color); /* 绘制当前像素点 */
        num += numadd;              /* 更新分子，增加分子值 */
        if (num >= den)             /* 判断是否需要调整坐标值 */
        {
            num -= den;               /* 更新分子值 */
            x += xinc1;               /* 调整 x 坐标 */
            y += yinc1;               /* 调整 y 坐标 */
        }
        x += xinc2;                 /* 调整 x 坐标 */
        y += yinc2;                 /* 调整 y 坐标 */
    }
}

/*****************************************************************************************
* 函 数 名: LCD_DrawLine_V
* 入口参数: uint16_t x - 直线起始横坐标
*           uint16_t y - 直线起始纵坐标
*           uint16_t height - 直线高度
* 返 回 值: 无
* 函数功能: 在LCD上绘制垂直直线
* 说    明: 将指定起始坐标处指定高度的垂直直线写入缓冲区，然后通过 LCD_SetAddress 和
*           LCD_WriteBuff 函数将缓冲区的内容写入LCD显存，实现垂直直线的绘制。
******************************************************************************************/
void LCD_DrawLine_V(uint16_t x, uint16_t y, uint16_t height)
{
		uint16_t i = 0;
    for (i = 0; i < height; i++)
    {
        LCD_Buff[i] = LCD.Color;  // 写入缓冲区
    }   
    LCD_SetAddress(x, y, x, y + height - 1);  // 设置坐标
    LCD_WriteBuff(LCD_Buff, height);          // 写入显存
}

/*****************************************************************************************
* 函 数 名: LCD_DrawLine_H
* 入口参数: uint16_t x - 直线起始横坐标
*           uint16_t y - 直线起始纵坐标
*           uint16_t width - 直线宽度
* 返 回 值: 无
* 函数功能: 在LCD上绘制水平直线
* 说    明: 将指定起始坐标处指定宽度的水平直线写入缓冲区，然后通过 LCD_SetAddress 和
*           LCD_WriteBuff 函数将缓冲区的内容写入LCD显存，实现水平直线的绘制。
******************************************************************************************/
void LCD_DrawLine_H(uint16_t x, uint16_t y, uint16_t width)
{
		uint16_t i = 0;
    for (i = 0; i < width; i++)
    {
        LCD_Buff[i] = LCD.Color;  // 写入缓冲区
    }   
    LCD_SetAddress(x, y, x + width - 1, y);  // 设置坐标
    LCD_WriteBuff(LCD_Buff, width);          // 写入显存
}


/*****************************************************************************************
* 函 数 名: LCD_DrawRect
* 入口参数: uint16_t x - 矩形左上角横坐标
*           uint16_t y - 矩形左上角纵坐标
*           uint16_t width - 矩形宽度
*           uint16_t height - 矩形高度
* 返 回 值: 无
* 函数功能: 在LCD上绘制矩形
* 说    明: 绘制矩形的过程是通过绘制四条边框线来实现的，分别绘制上、下、左、右四条线。
*           函数中调用了水平线和垂直线的绘制函数 LCD_DrawLine_H 和 LCD_DrawLine_V。
******************************************************************************************/
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    // 绘制上边框线
    LCD_DrawLine_H(x, y, width);

    // 绘制下边框线
    LCD_DrawLine_H(x, y + height - 1, width);

    // 绘制左边框线
    LCD_DrawLine_V(x, y, height);

    // 绘制右边框线
    LCD_DrawLine_V(x + width - 1, y, height);
}


/*****************************************************************************************
* 函 数 名: LCD_DrawCircle
* 入口参数: uint16_t x - 圆心横坐标
*           uint16_t y - 圆心纵坐标
*           uint16_t r - 圆的半径
* 返 回 值: 无
* 函数功能: 在LCD上绘制圆
* 说    明: 使用 Bresenham 算法绘制圆，该算法适用于绘制任意位置和半径的圆。
*           函数中调用了绘制点的函数 LCD_DrawPoint。
******************************************************************************************/
void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r)
{
    int Xadd = -r, Yadd = 0, err = 2 - 2 * r, e2;

    do {
        // 绘制圆上的四个对称点
        LCD_DrawPoint(x - Xadd, y + Yadd, LCD.Color);
        LCD_DrawPoint(x + Xadd, y + Yadd, LCD.Color);
        LCD_DrawPoint(x + Xadd, y - Yadd, LCD.Color);
        LCD_DrawPoint(x - Xadd, y - Yadd, LCD.Color);

        e2 = err;
        if (e2 <= Yadd) {
            err += ++Yadd * 2 + 1;
            if (-Xadd == Yadd && e2 <= Xadd) e2 = 0;
        }
        if (e2 > Xadd) err += ++Xadd * 2 + 1;
    } while (Xadd <= 0);
}



/*****************************************************************************************
* 函 数 名: LCD_DrawEllipse
* 入口参数: int x - 椭圆中心横坐标
*           int y - 椭圆中心纵坐标
*           int r1 - 椭圆长半轴
*           int r2 - 椭圆短半轴
* 返 回 值: 无
* 函数功能: 在LCD上绘制椭圆
* 说    明: 使用 Bresenham 算法绘制椭圆，该算法适用于绘制任意位置和长短半轴的椭圆。
*           函数中调用了绘制点的函数 LCD_DrawPoint。
******************************************************************************************/
void LCD_DrawEllipse(int x, int y, int r1, int r2)
{
    int Xadd = -r1, Yadd = 0, err = 2 - 2 * r1, e2;
    float K = 0, rad1 = 0, rad2 = 0;

    rad1 = r1;
    rad2 = r2;

    if (r1 > r2)
    {
        do {
            K = (float)(rad1 / rad2);

            // 绘制椭圆上的四个对称点
            LCD_DrawPoint(x - Xadd, y + (uint16_t)(Yadd / K), LCD.Color);
            LCD_DrawPoint(x + Xadd, y + (uint16_t)(Yadd / K), LCD.Color);
            LCD_DrawPoint(x + Xadd, y - (uint16_t)(Yadd / K), LCD.Color);
            LCD_DrawPoint(x - Xadd, y - (uint16_t)(Yadd / K), LCD.Color);

            e2 = err;
            if (e2 <= Yadd) {
                err += ++Yadd * 2 + 1;
                if (-Xadd == Yadd && e2 <= Xadd) e2 = 0;
            }
            if (e2 > Xadd) err += ++Xadd * 2 + 1;
        } while (Xadd <= 0);
    }
    else
    {
        Yadd = -r2;
        Xadd = 0;
        do {
            K = (float)(rad2 / rad1);

            // 绘制椭圆上的四个对称点
            LCD_DrawPoint(x - (uint16_t)(Xadd / K), y + Yadd, LCD.Color);
            LCD_DrawPoint(x + (uint16_t)(Xadd / K), y + Yadd, LCD.Color);
            LCD_DrawPoint(x + (uint16_t)(Xadd / K), y - Yadd, LCD.Color);
            LCD_DrawPoint(x - (uint16_t)(Xadd / K), y - Yadd, LCD.Color);

            e2 = err;
            if (e2 <= Xadd) {
                err += ++Xadd * 3 + 1;
                if (-Yadd == Xadd && e2 <= Yadd) e2 = 0;
            }
            if (e2 > Yadd) err += ++Yadd * 3 + 1;
        } while (Yadd <= 0);
    }
}



/*****************************************************************************************
* 函 数 名: LCD_FillCircle
* 入口参数: uint16_t x - 圆心横坐标
*           uint16_t y - 圆心纵坐标
*           uint16_t r - 圆半径
* 返 回 值: 无
* 函数功能: 在LCD上填充圆形
* 说    明: 使用 Bresenham 算法绘制填充圆形。函数中调用了绘制垂直线的函数 LCD_DrawLine_V。
******************************************************************************************/
void LCD_FillCircle(uint16_t x, uint16_t y, uint16_t r)
{
    int32_t D;      /* 决策变量 */
    uint32_t CurX;  /* 当前 X 值 */
    uint32_t CurY;  /* 当前 Y 值 */

    D = 3 - (r << 1);

    CurX = 0;
    CurY = r;

    while (CurX <= CurY)
    {
        if (CurY > 0)
        {
            // 绘制两条垂直线，形成一个圆环的一部分
            LCD_DrawLine_V(x - CurX, y - CurY, 2 * CurY);
            LCD_DrawLine_V(x + CurX, y - CurY, 2 * CurY);
        }

        if (CurX > 0)
        {
            // 绘制两条垂直线，形成一个圆环的一部分
            LCD_DrawLine_V(x - CurY, y - CurX, 2 * CurX);
            LCD_DrawLine_V(x + CurY, y - CurX, 2 * CurX);
        }

        if (D < 0)
        {
            D += (CurX << 2) + 6;
        }
        else
        {
            D += ((CurX - CurY) << 2) + 10;
            CurY--;
        }
        CurX++;
    }
}


/*****************************************************************************************
* 函 数 名: LCD_FillRect
* 入口参数: uint16_t x - 矩形左上角横坐标
*           uint16_t y - 矩形左上角纵坐标
*           uint16_t width - 矩形宽度
*           uint16_t height - 矩形高度
* 返 回 值: 无
* 函数功能: 在LCD上填充矩形
* 说    明: 使用SPI通信，通过设置坐标范围，将矩形区域的像素点全部填充为指定颜色。函数中使用
*           Bresenham 算法绘制填充矩形。
******************************************************************************************/
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    uint16_t i;

    LCD_SetAddress(x, y, x + width - 1, y + height - 1); // 设置坐标

    LCD_SPI->CR1 &= 0xFFBF;             // 关闭SPI
    LCD_SPI->CR1 |= SPI_DataSize_16b;   // 切换成16位数据格式
    LCD_SPI->CR1 |= 0x0040;             // 使能SPI

    LCD_CS_L;    // 片选拉低，使能IC

    for (i = 0; i < width * height; i++)
    {
        LCD_SPI->DR = LCD.Color;
        while ((LCD_SPI->SR & SPI_I2S_FLAG_TXE) == 0);
    }

    while ((LCD_SPI->SR & SPI_I2S_FLAG_BSY) != RESET); // 等待通信完成
    LCD_CS_H;     // 片选拉高

    LCD_SPI->CR1 &= 0xFFBF;  // 关闭SPI
    LCD_SPI->CR1 &= 0xF7FF;  // 切换成8位数据格式
    LCD_SPI->CR1 |= 0x0040;  // 使能SPI
}


/*****************************************************************************************
* 函 数 名: LCD_DrawImage
* 入口参数: uint16_t x - 图像左上角横坐标
*           uint16_t y - 图像左上角纵坐标
*           uint16_t width - 图像宽度
*           uint16_t height - 图像高度
*           const uint8_t *pImage - 图像数据数组指针
* 返 回 值: 无
* 函数功能: 在LCD上绘制图像
* 说    明: 使用SPI通信，通过设置坐标范围，将图像数据写入显存。因为缓冲区大小有限，需要
*           分多次写入，通过计算缓冲区能够写入图片的多少行，实现分段写入。函数中使用
*           Bresenham 算法绘制图像。
******************************************************************************************/
void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *pImage)
{
    uint8_t disChar;          // 字模的值
    uint16_t Xaddress = x;    // 水平坐标
    uint16_t Yaddress = y;    // 垂直坐标
    uint16_t i = 0, j = 0, m = 0;    // 计数变量
    uint16_t BuffCount = 0;          // 缓冲区计数
    uint16_t Buff_Height = 0;        // 缓冲区的行数

    // 因为缓冲区大小有限，需要分多次写入
    Buff_Height = (sizeof(LCD_Buff) / 2) / height; // 计算缓冲区能够写入图片的多少行

    for (i = 0; i < height; i++) // 循环按行写入
    {
        for (j = 0; j < (float)width / 8; j++)
        {
            disChar = *pImage;

            for (m = 0; m < 8; m++)
            {
                if (disChar & 0x01)
                {
                    LCD_Buff[BuffCount] = LCD.Color;    // 当前模值不为0时，使用画笔色绘点
                }
                else
                {
                    LCD_Buff[BuffCount] = LCD.BackColor; // 否则使用背景色绘制点
                }
                disChar >>= 1;      // 模值移位
                Xaddress++;         // 水平坐标自加
                BuffCount++;        // 缓冲区计数
                if ((Xaddress - x) == width) // 如果水平坐标达到了字符宽度，则退出当前循环,进入下一行的绘制
                {
                    Xaddress = x;
                    break;
                }
            }
            pImage++;
        }
        if (BuffCount == Buff_Height * width) // 达到缓冲区所能容纳的最大行数时
        {
            BuffCount = 0; // 缓冲区计数清0

            LCD_SetAddress(x, Yaddress, x + width - 1, Yaddress + Buff_Height - 1); // 设置坐标
            LCD_WriteBuff(LCD_Buff, width * Buff_Height);                            // 写入显存

            Yaddress = Yaddress + Buff_Height; // 计算行偏移，开始写入下一部分数据
        }
        if ((i + 1) == height) // 到了最后一行时
        {
            LCD_SetAddress(x, Yaddress, x + width - 1, i + y);                   // 设置坐标
            LCD_WriteBuff(LCD_Buff, width * (i + 1 + y - Yaddress));            // 写入显存
        }
    }
}


/*****************************************************************************************
* 函 数 名: DrawRoundRect
* 入口参数: int x - 圆角矩形左上角横坐标
*           int y - 圆角矩形左上角纵坐标
*           unsigned char w - 圆角矩形宽度
*           unsigned char h - 圆角矩形高度
*           unsigned char r - 圆角半径
* 返 回 值: 无
* 函数功能: 在LCD上绘制圆角矩形
* 说    明: 通过绘制水平和垂直线，以及在四个角上绘制圆弧，实现圆角矩形的绘制。函数中使
*           用 DrawCircleHelper 函数绘制圆角矩形的四个圆角。
******************************************************************************************/
void DrawRoundRect(int x, int y, unsigned char w, unsigned char h, unsigned char r)
{
    // 绘制上下两条水平线
    LCD_DrawLine_H(x + r, y, w - 2 * r);        // 上边
    LCD_DrawLine_H(x + r, y + h - 1, w - 2 * r); // 下边

    // 绘制左右两条垂直线
    LCD_DrawLine_V(x, y + r, h - 2 * r);        // 左边
    LCD_DrawLine_V(x + w - 1, y + r, h - 2 * r); // 右边

    // 绘制四个圆角
    DrawCircleHelper(x + r, y + r, r, 1);                 // 左上角
    DrawCircleHelper(x + w - r - 1, y + r, r, 2);         // 右上角
    DrawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4); // 右下角
    DrawCircleHelper(x + r, y + h - r - 1, r, 8);         // 左下角
}

/*****************************************************************************************
* 函 数 名: DrawfillRoundRect
* 入口参数: int x - 圆角矩形左上角横坐标
*           int y - 圆角矩形左上角纵坐标
*           unsigned char w - 圆角矩形宽度
*           unsigned char h - 圆角矩形高度
*           unsigned char r - 圆角半径
* 返 回 值: 无
* 函数功能: 在LCD上绘制填充的圆角矩形
* 说    明: 通过绘制一条水平线和在两个角上绘制填充的圆弧，实现填充的圆角矩形的绘制。函数
*           中使用 DrawFillCircleHelper 函数绘制两个角的填充圆弧，同时使用 LCD_FillRect
*           函数绘制矩形的中间部分。
******************************************************************************************/
void DrawfillRoundRect(int x, int y, unsigned char w, unsigned char h, unsigned char r)
{
    // 绘制矩形的中间部分
    LCD_FillRect(x + r, y, w - 2 * r, h);

    // 绘制两个角的填充圆弧
    DrawFillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1); // 右上角
    DrawFillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1);         // 左上角
}

/*****************************************************************************************
* 函 数 名: DrawCircleHelper
* 入口参数: int x0 - 圆心横坐标
*           int y0 - 圆心纵坐标
*           unsigned char r - 圆半径
*           unsigned char cornername - 圆的四个角的组合值
* 返 回 值: 无
* 函数功能: 在LCD上绘制圆的一部分，可选择绘制的圆角
* 说    明: 使用 Bresenham 算法绘制圆的一部分，可以选择在四个角中的一个或多个绘制圆角。
*           圆角的组合值由 cornername 决定，每个位代表一个角，1 代表绘制，0 代表不绘制。
*           函数中使用 LCD_DrawPoint 函数绘制每个像素点。
******************************************************************************************/
void DrawCircleHelper(int x0, int y0, unsigned char r, unsigned char cornername)
{
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }

        x++;
        ddF_x += 2;
        f += ddF_x;

        if (cornername & 0x4)
        {
            LCD_DrawPoint(x0 + x, y0 + y, LIGHT_GREEN);
            LCD_DrawPoint(x0 + y, y0 + x, LIGHT_GREEN);
        }
        if (cornername & 0x2)
        {
            LCD_DrawPoint(x0 + x, y0 - y, LIGHT_GREEN);
            LCD_DrawPoint(x0 + y, y0 - x, LIGHT_GREEN);
        }
        if (cornername & 0x8)
        {
            LCD_DrawPoint(x0 - y, y0 + x, LIGHT_GREEN);
            LCD_DrawPoint(x0 - x, y0 + y, LIGHT_GREEN);
        }
        if (cornername & 0x1)
        {
            LCD_DrawPoint(x0 - y, y0 - x, LIGHT_GREEN);
            LCD_DrawPoint(x0 - x, y0 - y, LIGHT_GREEN);
        }
    }
}

/*****************************************************************************************
* 函 数 名: DrawFillCircleHelper
* 入口参数: int x0 - 圆心横坐标
*           int y0 - 圆心纵坐标
*           unsigned char r - 圆半径
*           unsigned char cornername - 圆的四个角的组合值
*           int delta - 修正值，用于微调绘制效果
* 返 回 值: 无
* 函数功能: 在LCD上绘制填充圆的一部分，可选择绘制的圆角
* 说    明: 使用 Bresenham 算法绘制填充圆的一部分，可以选择在两个角中的一个或两个绘制圆角。
*           圆角的组合值由 cornername 决定，每个位代表一个角，1 代表绘制，0 代表不绘制。
*           函数中使用 LCD_DrawLine_V 函数绘制每列像素点。
******************************************************************************************/
void DrawFillCircleHelper(int x0, int y0, unsigned char r, unsigned char cornername, int delta)
{
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }

        x++;
        ddF_x += 2;
        f += ddF_x;

        if (cornername & 0x1)
        {
            LCD_DrawLine_V(x0 + x, y0 - y, 2 * y + 1 + delta);
            LCD_DrawLine_V(x0 + y, y0 - x, 2 * x + 1 + delta);
        }

        if (cornername & 0x2)
        {
            LCD_DrawLine_V(x0 - x, y0 - y, 2 * y + 1 + delta);
            LCD_DrawLine_V(x0 - y, y0 - x, 2 * x + 1 + delta);
        }
    }
}

/*****************************************************************************************
* 函 数 名: DrawFillEllipse
* 入口参数: int x0 - 椭圆中心横坐标
*           int y0 - 椭圆中心纵坐标
*           int rx - 椭圆 x 轴半径
*           int ry - 椭圆 y 轴半径
* 返 回 值: 无
* 函数功能: 在LCD上绘制填充椭圆
* 说    明: 使用 Bresenham 算法绘制填充椭圆。函数中使用 LCD_DrawLine_V 函数绘制每列像素点。
******************************************************************************************/
void DrawFillEllipse(int x0, int y0, int rx, int ry)
{
    int x, y;
    int xchg, ychg;
    int err;
    int rxrx2;
    int ryry2;
    int stopx, stopy;

    rxrx2 = rx;
    rxrx2 *= rx;
    rxrx2 *= 2;

    ryry2 = ry;
    ryry2 *= ry;
    ryry2 *= 2;

    x = rx;
    y = 0;

    xchg = 1;
    xchg -= rx;
    xchg -= rx;
    xchg *= ry;
    xchg *= ry;

    ychg = rx;
    ychg *= rx;

    err = 0;

    stopx = ryry2;
    stopx *= rx;
    stopy = 0;

    while (stopx >= stopy)
    {
        // 绘制椭圆的四个象限的一列像素点
        LCD_DrawLine_V(x0 + x, y0 - y, y + 1);
        LCD_DrawLine_V(x0 - x, y0 - y, y + 1);
        LCD_DrawLine_V(x0 + x, y0, y + 1);
        LCD_DrawLine_V(x0 - x, y0, y + 1);

        y++;
        stopy += rxrx2;
        err += ychg;
        ychg += rxrx2;

        if (2 * err + xchg > 0)
        {
            x--;
            stopx -= ryry2;
            err += xchg;
            xchg += ryry2;
        }
    }

    x = 0;
    y = ry;

    xchg = ry;
    xchg *= ry;

    ychg = 1;
    ychg -= ry;
    ychg -= ry;
    ychg *= rx;
    ychg *= rx;

    err = 0;

    stopx = 0;
    stopy = rxrx2;
    stopy *= ry;

    while (stopx <= stopy)
    {
        // 绘制椭圆的四个象限的一列像素点
        LCD_DrawLine_V(x0 + x, y0 - y, y + 1);
        LCD_DrawLine_V(x0 - x, y0 - y, y + 1);
        LCD_DrawLine_V(x0 + x, y0, y + 1);
        LCD_DrawLine_V(x0 - x, y0, y + 1);

        x++;
        stopx += ryry2;
        err += xchg;
        xchg += ryry2;

        if (2 * err + ychg > 0)
        {
            y--;
            stopy -= rxrx2;
            err += ychg;
            ychg += rxrx2;
        }
    }
}

/*****************************************************************************************
* 函 数 名: DrawTriangle
* 入口参数: unsigned char x0 - 三角形第一个顶点横坐标
*           unsigned char y0 - 三角形第一个顶点纵坐标
*           unsigned char x1 - 三角形第二个顶点横坐标
*           unsigned char y1 - 三角形第二个顶点纵坐标
*           unsigned char x2 - 三角形第三个顶点横坐标
*           unsigned char y2 - 三角形第三个顶点纵坐标
* 返 回 值: 无
* 函数功能: 在LCD上绘制三角形
* 说    明: 使用 LCD_DrawLine 函数绘制三角形的三条边。
******************************************************************************************/
void DrawTriangle(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2)
{
    LCD_DrawLine(x0, y0, x1, y1);
    LCD_DrawLine(x1, y1, x2, y2);
    LCD_DrawLine(x2, y2, x0, y0);
}

/*****************************************************************************************
* 函 数 名: DrawFillTriangle
* 入口参数: int x0 - 三角形第一个顶点横坐标
*           int y0 - 三角形第一个顶点纵坐标
*           int x1 - 三角形第二个顶点横坐标
*           int y1 - 三角形第二个顶点纵坐标
*           int x2 - 三角形第三个顶点横坐标
*           int y2 - 三角形第三个顶点纵坐标
* 返 回 值: 无
* 函数功能: 在LCD上绘制填充的三角形
* 说    明: 使用 LCD_DrawLine_H 函数绘制三角形的各个水平扫描线。
******************************************************************************************/
void DrawFillTriangle(int x0, int y0, int x1, int y1, int x2, int y2)
{
    int a, b, y, last;
    int dx01, dy01, dx02, dy02, dx12, dy12, sa = 0, sb = 0;

    // 保证y0<=y1<=y2
    if (y0 > y1) { SWAP(y0, y1); SWAP(x0, x1); }
    if (y1 > y2) { SWAP(y2, y1); SWAP(x2, x1); }
    if (y0 > y1) { SWAP(y0, y1); SWAP(x0, x1); }

    // 三角形退化成线段或为单点
    if (y0 == y2)
    {
        a = b = x0;
        if (x1 < a) a = x1;
        else if (x1 > b) b = x1;
        if (x2 < a) a = x2;
        else if (x2 > b) b = x2;

        LCD_DrawLine_H(a, y0, b - a + 1);
        return;
    }

    dx01 = x1 - x0;
    dy01 = y1 - y0;
    dx02 = x2 - x0;
    dy02 = y2 - y0;
    dx12 = x2 - x1;
    dy12 = y2 - y1;
    sa = 0;
    sb = 0;

    if (y1 == y2)
    {
        last = y1;   // Include y1 scanline
    }
    else
    {
        last = y1 - 1; // Skip it
    }

    // 绘制上半部分三角形
    for (y = y0; y <= last; y++)
    {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;

        if (a > b) {SWAP(a, b);}

        LCD_DrawLine_H(a, y, b - a + 1);
    }

    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);

    // 绘制下半部分三角形
    for (; y <= y2; y++)
    {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;

        if (a > b) {SWAP(a, b);}

        LCD_DrawLine_H(a, y, b - a + 1);
    }
  
}


/*****************************************************************************************
* 函 数 名: DrawArc
* 入口参数: int x - 圆心横坐标
*           int y - 圆心纵坐标
*           unsigned char r - 弧形半径
*           int angle_start - 弧形起始角度
*           int angle_end - 弧形终止角度
* 返 回 值: 无
* 函数功能: 绘制指定圆心、半径的弧形。
* 说    明: 该函数从起始角度到终止角度以5度步进绘制弧形，使用了旋转函数和直线绘制函数。
******************************************************************************************/
void DrawArc(int x, int y, unsigned char r, int angle_start, int angle_end)
{
    float i = 0;
    TypeXY m, temp;
    temp = GetXY();
    SetRotateCenter(x, y);
    SetAngleDir(0);
    
    if (angle_end > 360)
        angle_end = 360;
    
    SetAngle(0);
    m = GetRotateXY(x, y + r);
    MoveTo(m.x, m.y);
    
    for (i = angle_start; i < angle_end; i += 5)
    {
        SetAngle(i);
        m = GetRotateXY(x, y + r);
        LineTo(m.x, m.y);
    }
    
    MoveTo(temp.x, temp.y);
}

/*****************************************************************************************
* 函 数 名: GetXY
* 返 回 值: TypeXY - 包含横坐标和纵坐标的二维坐标结构体
* 函数功能: 获取坐标值的函数
* 说    明: 返回一个 TypeXY 结构体，其中包含横坐标和纵坐标的值。
******************************************************************************************/
TypeXY GetXY(void)
{
    TypeXY m;
    m.x = _pointx;  // 获取横坐标值
    m.y = _pointy;  // 获取纵坐标值
    return m;       // 返回包含坐标值的结构体
}

/*****************************************************************************************
* 函 数 名: SetRotateCenter
* 入口参数: int x0 - 旋转中心的横坐标
*           int y0 - 旋转中心的纵坐标
* 返 回 值: 无
* 函数功能: 设置旋转中心的坐标。
* 说    明: 通过调用该函数，可以设置旋转操作的中心坐标，即围绕哪个点进行旋转。
******************************************************************************************/
void SetRotateCenter(int x0, int y0)
{
    _RoateValue.center.x = x0;
    _RoateValue.center.y = y0;
}

/*****************************************************************************************
* 函 数 名: SetAngleDir
* 入口参数: int direction - 旋转方向，1为顺时针，-1为逆时针
* 返 回 值: 无
* 函数功能: 设置旋转的方向。
* 说    明: 通过调用该函数，可以设置旋转的方向，1表示顺时针，-1表示逆时针。
******************************************************************************************/
void SetAngleDir(int direction)
{
    _RoateValue.direct = direction;
}


/*****************************************************************************************
* 函 数 名: SetAngle
* 入口参数: float angle - 旋转角度（单位：度）
* 返 回 值: 无
* 函数功能: 设置旋转的角度。
* 说    明: 通过调用该函数，可以设置旋转的角度，单位为度。内部会将角度转换为弧度进行处理。
******************************************************************************************/
void SetAngle(float angle)
{
    _RoateValue.angle = RADIAN(angle);
}

/*****************************************************************************************
* 函 数 名: Rotate
* 入口参数: int x0 - 旋转中心的横坐标
*           int y0 - 旋转中心的纵坐标
*           int *x - 待旋转点的横坐标
*           int *y - 待旋转点的纵坐标
*           double angle - 旋转角度（弧度）
*           int direction - 旋转方向，非零表示顺时针，零表示逆时针
* 返 回 值: 无
* 函数功能: 对指定点进行旋转。
* 说    明: 通过调用该函数，可以对给定的点围绕指定中心进行旋转。旋转角度由 angle 参数指定，
*           direction 参数指定旋转方向，非零表示顺时针，零表示逆时针。
******************************************************************************************/
static void Rotate(int x0, int y0, int *x, int *y, double angle, int direction)
{
    int temp = (*y - y0) * (*y - y0) + (*x - x0) * (*x - x0);
    double r = mySqrt(temp);
    double a0 = atan2(*x - x0, *y - y0);
    
    if (direction)
    {
        *x = x0 + r * cos(a0 + angle);
        *y = y0 + r * sin(a0 + angle);
    }
    else
    {
        *x = x0 + r * cos(a0 - angle);
        *y = y0 + r * sin(a0 - angle);
    }
}


/*****************************************************************************************
* 函 数 名: mySqrt
* 入口参数: float x - 待求平方根的数值
* 返 回 值: float - 计算得到的平方根值
* 函数功能: 计算给定数值的平方根。
* 说    明: 该函数采用牛顿迭代法计算平方根，经过多次迭代逼近平方根的真实值。
******************************************************************************************/
float mySqrt(float x)
{
    float a = x;
    unsigned int i = *(unsigned int *)&x;
    i = (i + 0x3f76cf62) >> 1;
    x = *(float *)&i;
    x = (x + a / x) * 0.5;
    return x;
}


/*****************************************************************************************
* 函 数 名: GetRotateXY
* 入口参数: int x - 待旋转的点的原始横坐标
*           int y - 待旋转的点的原始纵坐标
* 返 回 值: TypeXY - 旋转后的点坐标
* 函数功能: 获取经过旋转变换后的点坐标。
* 说    明: 如果旋转角度不为0，通过调用 Rotate 函数实现对给定点的旋转，
*          然后将旋转后的坐标保存在 TypeXY 结构体中返回。
******************************************************************************************/
TypeXY GetRotateXY(int x, int y)
{
    TypeXY temp;
    int m = x, n = y;
    Rotate(_RoateValue.center.x, _RoateValue.center.y, &m, &n, _RoateValue.angle, _RoateValue.direct);
    temp.x = m;
    temp.y = n;
    return temp;
}


/*****************************************************************************************
* 函 数 名: MoveTo
* 入口参数: int x - 移动到的目标横坐标
*           int y - 移动到的目标纵坐标
* 返 回 值: 无
* 函数功能: 移动绘图光标到指定的目标位置。
* 说    明: 将绘图光标的当前位置更新为给定的目标位置，以便后续的绘图操作发生在该目标位置上。
******************************************************************************************/
void MoveTo(int x, int y)
{
    _pointx = x;
    _pointy = y;
}


/*****************************************************************************************
* 函 数 名: LineTo
* 入口参数: int x - 目标横坐标
*           int y - 目标纵坐标
* 返 回 值: 无
* 函数功能: 在当前光标位置与指定目标位置之间绘制直线。
* 说    明: 该函数绘制从当前光标位置到目标位置的直线，并将光标更新为目标位置。
******************************************************************************************/
void LineTo(int x, int y)
{
    LCD_DrawLine(_pointx, _pointy, x, y);
    _pointx = x;
    _pointy = y;
}


/*****************************************************************************************
* 函 数 名: SetRotateValue
* 入口参数: int x - 旋转中心横坐标
*           int y - 旋转中心纵坐标
*           float angle - 旋转角度（弧度）
*           int direct - 旋转方向（0为顺时针，1为逆时针）
* 返 回 值: 无
* 函数功能: 设置旋转参数，包括旋转中心、旋转角度和旋转方向。
* 说    明: 该函数通过调用其他设置函数，实现了对旋转参数的一次性设置。
******************************************************************************************/
void SetRotateValue(int x, int y, float angle, int direct)
{
    SetRotateCenter(x, y);
    SetAngleDir(direct);
    SetAngle(angle);
}

