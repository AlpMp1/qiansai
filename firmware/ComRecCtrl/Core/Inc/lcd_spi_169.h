/**
 * @file    lcd_spi_169.h
 * @brief   1.69寸 240x280 SPI LCD 驱动头文件（移植自 STM32F103 → STM32F407 HAL）
 *
 * 引脚配置（SPI1）：
 *   PB3 → SCK   (AF5)
 *   PB5 → MOSI  (AF5)
 *   PB4 → CS    (GPIO Output)
 *   PD13 → DC   (GPIO Output，与 main.h LCD_DC_Pin 一致)
 *   PD12 → BL   (GPIO Output，与 main.h LCD_BL_Pin 一致，高电平开背光)
 */

#ifndef __LCD_SPI_169_H
#define __LCD_SPI_169_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "lcd_fonts.h"
#include "lcd_image.h"

/* -------------------------------- 屏幕分辨率 -------------------------------- */
#define LCD_Width     240
#define LCD_Height    280

/* -------------------------------- 工具宏 ------------------------------------ */
#define RADIAN(angle)  ((angle == 0) ? 0 : (3.14159f * (angle) / 180.0f))
#define MAX(x, y)      ((x) > (y) ? (x) : (y))
#define MIN(x, y)      ((x) < (y) ? (x) : (y))
#define SWAP(x, y)     do { (y) = (x)+(y); (x) = (y)-(x); (y) = (y)-(x); } while(0)
#define ABS(X)         ((X) > 0 ? (X) : -(X))

/* -------------------------------- 坐标结构体 -------------------------------- */
typedef struct COORDINATE {
    int x;
    int y;
} TypeXY;

#define point TypeXY

typedef struct ROATE {
    TypeXY center;
    float  angle;
    int    direct;
} TypeRoate;

/* -------------------------------- 显示方向 ---------------------------------- */
#define Direction_H      0   /* 横屏 */
#define Direction_H_Flip 1   /* 横屏翻转 */
#define Direction_V      2   /* 竖屏 */
#define Direction_V_Flip 3   /* 竖屏翻转 */

/* -------------------------------- 颜色定义（RGB888） ----------------------- */
#define LCD_WHITE    0xFFFFFF
#define LCD_BLACK    0x000000
#define LCD_BLUE     0x0000FF
#define LCD_GREEN    0x00FF00
#define LCD_RED      0xFF0000
#define LCD_CYAN     0x00FFFF
#define LCD_MAGENTA  0xFF00FF
#define LCD_YELLOW   0xFFFF00
#define LCD_GREY     0x2C2C2C

#define LIGHT_BLUE    0x8080FF
#define LIGHT_GREEN   0x80FF80
#define LIGHT_RED     0xFF8080
#define LIGHT_CYAN    0x80FFFF
#define LIGHT_MAGENTA 0xFF80FF
#define LIGHT_YELLOW  0xFFFF80
#define LIGHT_GREY    0xA3A3A3

#define DARK_BLUE    0x000080
#define DARK_GREEN   0x008000
#define DARK_RED     0x800000
#define DARK_CYAN    0x008080
#define DARK_MAGENTA 0x800080
#define DARK_YELLOW  0x808000
#define DARK_GREY    0x404040

/* -------------------------------- SPI 与引脚配置 ---------------------------- */
/* SPI 外设（APB1，最高42MHz） */
#define LCD_SPI_INSTANCE     SPI3
#define LCD_SPI_CLK_ENABLE() __HAL_RCC_SPI3_CLK_ENABLE()

/* SCK: PB3, AF6 */
#define LCD_SCK_PIN          GPIO_PIN_3
#define LCD_SCK_PORT         GPIOB
#define LCD_SCK_AF           GPIO_AF6_SPI3

/* MOSI: PB5, AF6 */
#define LCD_SDA_PIN          GPIO_PIN_5
#define LCD_SDA_PORT         GPIOB
#define LCD_SDA_AF           GPIO_AF6_SPI3

/* CS: PB4 */
#define LCD_CS_PIN           GPIO_PIN_4
#define LCD_CS_PORT          GPIOB

/* DC (Data/Command): PD13，与 main.h LCD_DC_Pin / LCD_DC_GPIO_Port 一致 */
#define LCD_DC_PIN           GPIO_PIN_13
#define LCD_DC_PORT          GPIOD

/* 背光: PD12，与 main.h LCD_BL_Pin / LCD_BL_GPIO_Port 一致 */
#define LCD_BL_PIN           GPIO_PIN_12
#define LCD_BL_PORT          GPIOD

/* GPIO 时钟使能（SCK/MOSI/CS 在 GPIOB，DC/BL 在 GPIOD） */
#define LCD_GPIO_CLK_ENABLE() do { __HAL_RCC_GPIOB_CLK_ENABLE(); \
                                   __HAL_RCC_GPIOD_CLK_ENABLE(); } while(0)

/* -------------------------------- 控制宏 ------------------------------------ */
#define LCD_CS_H         HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET)
#define LCD_CS_L         HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET)

#define LCD_DC_Command   HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET)
#define LCD_DC_Data      HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET)

#define LCD_Backlight_ON  HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_SET)
#define LCD_Backlight_OFF HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_RESET)

/* -------------------------------- 函数声明 ---------------------------------- */
void LCD_GPIO_Config(void);
void LCD_SPI_Config(void);
void LCD_WriteCommand(uint8_t lcd_command);
void LCD_WriteData_8bit(uint8_t lcd_data);
void LCD_WriteData_16bit(uint16_t lcd_data);
void LCD_WriteBuff(uint16_t *DataBuff, uint16_t DataSize);
void SPI_LCD_Init(void);

void LCD_SetAddress(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_SetColor(uint32_t Color);
void LCD_SetBackColor(uint32_t Color);
void LCD_SetDirection(uint8_t direction);
void LCD_SetAsciiFont(pFONT *Asciifonts);
void LCD_SetTextFont(pFONT *fonts);
void LCD_Clear(void);
void LCD_ClearRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint32_t color);
void LCD_DisplayChar(uint16_t x, uint16_t y, uint8_t c);
void LCD_DisplayString(uint16_t x, uint16_t y, char *p);
void LCD_DisplayChinese(uint16_t x, uint16_t y, char *pText);
void LCD_DisplayText(uint16_t x, uint16_t y, char *pText);

void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_DrawLine_V(uint16_t x, uint16_t y, uint16_t height);
void LCD_DrawLine_H(uint16_t x, uint16_t y, uint16_t width);
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void LCD_DrawCircle(uint16_t x, uint16_t y, uint16_t r);
void LCD_FillCircle(uint16_t x, uint16_t y, uint16_t r);
void LCD_DrawEllipse(int x, int y, int r1, int r2);
void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *pImage);

void DrawRoundRect(int x, int y, unsigned char w, unsigned char h, unsigned char r);
void DrawfillRoundRect(int x, int y, unsigned char w, unsigned char h, unsigned char r);
void DrawCircleHelper(int x0, int y0, unsigned char r, unsigned char cornername);
void DrawFillCircleHelper(int x0, int y0, unsigned char r, unsigned char cornername, int delta);
void DrawFillEllipse(int x0, int y0, int rx, int ry);
void DrawTriangle(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2);
void DrawFillTriangle(int x0, int y0, int x1, int y1, int x2, int y2);
void DrawArc(int x, int y, unsigned char r, int angle_start, int angle_end);

TypeXY GetXY(void);
void SetRotateCenter(int x0, int y0);
void SetAngleDir(int direction);
void SetAngle(float angle);
float mySqrt(float x);
TypeXY GetRotateXY(int x, int y);
void MoveTo(int x, int y);
void LineTo(int x, int y);
void SetRotateValue(int x, int y, float angle, int direct);

#endif /* __LCD_SPI_169_H */
