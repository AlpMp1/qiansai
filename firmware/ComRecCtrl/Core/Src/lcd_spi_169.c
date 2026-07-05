/**
 * @file    lcd_spi_169.c
 * @brief   1.69寸 240x280 SPI LCD 驱动（移植自 STM32F103 → STM32F407 HAL）
 *
 * 移植说明：
 *   - 原驱动基于 STM32F10x 标准库，本文件改为 STM32F4xx HAL 库
 *   - SPI 低层操作保留直接寄存器访问以保证刷屏性能
 *   - 引脚：PB3=SCK, PB5=MOSI, PB4=CS, PD13=DC, PD12=BL（SPI3/AF6）
 */

#include "lcd_spi_169.h"

/* ======================== 静态变量 ======================== */
static SPI_HandleTypeDef hspi_lcd;         /* SPI 句柄（内部使用） */
static pFONT *LCD_AsciiFonts;             /* ASCII 字体 */
static pFONT *LCD_CHFonts;               /* 中文字体 */
static uint16_t LCD_Buff[1024];           /* 批量写入缓冲区 */
static int  _pointx = 0;
static int  _pointy = 0;
static TypeRoate _RoateValue = {{0, 0}, 0, 1};

/* LCD 参数结构体 */
static struct {
    uint32_t Color;
    uint32_t BackColor;
    uint8_t  ShowNum_Mode;
    uint8_t  Direction;
    uint16_t Width;
    uint16_t Height;
    uint8_t  X_Offset;
    uint8_t  Y_Offset;
} LCD;

/* ======================== 底层接口 ======================== */

/**
 * @brief  GPIO 初始化
 *         PB3(SCK/AF5), PB5(MOSI/AF5): SPI 复用
 *         PB4(CS):                     GPIOB 推挽输出
 *         PD13(DC), PD12(BL):          GPIOD 推挽输出
 */
void LCD_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    LCD_GPIO_CLK_ENABLE();   /* 同时使能 GPIOB 和 GPIOD 时钟 */
    LCD_SPI_CLK_ENABLE();

    /* SCK PB3, MOSI PB5 — 复用推挽 */
    GPIO_InitStruct.Pin       = LCD_SCK_PIN | LCD_SDA_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = LCD_SCK_AF;          /* AF5 = SPI1 */
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* CS PB4 — GPIOB 推挽输出 */
    GPIO_InitStruct.Pin       = LCD_CS_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(LCD_CS_PORT, &GPIO_InitStruct);

    /* DC PD13, BL PD12 — GPIOD 推挽输出 */
    GPIO_InitStruct.Pin       = LCD_DC_PIN | LCD_BL_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* 初始状态 */
    LCD_DC_Data;         /* DC 高，默认数据模式 */
    LCD_CS_H;            /* CS 高，禁止通信 */
    LCD_Backlight_OFF;   /* 背光先关，初始化完成后再开 */
}

/**
 * @brief  SPI1 配置
 *         main.c 中 MX_SPI1_Init() 已完成初始化，此处仅设置内部句柄实例
 *         并确保 SPI 处于使能状态。
 */
void LCD_SPI_Config(void)
{
    /* 使用 CubeMX 已初始化的 SPI3，只需设置 Instance 并确保已使能 */
    hspi_lcd.Instance = LCD_SPI_INSTANCE;
    /* 单线发送模式：使能 BIDIOE */
    LCD_SPI_INSTANCE->CR1 |= SPI_CR1_BIDIOE;
    __HAL_SPI_ENABLE(&hspi_lcd);
}

/**
 * @brief  向 LCD 发送命令字节
 */
void LCD_WriteCommand(uint8_t lcd_command)
{
    /* 等待上一次传输完成 */
    while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);

    LCD_DC_Command;  /* DC 低 → 命令 */

    LCD_SPI_INSTANCE->DR = lcd_command;
    while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));
    while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);

    LCD_DC_Data;     /* DC 高 → 数据 */
}

/**
 * @brief  向 LCD 发送 8bit 数据
 */
void LCD_WriteData_8bit(uint8_t lcd_data)
{
    LCD_SPI_INSTANCE->DR = lcd_data;
    while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));
}

/**
 * @brief  向 LCD 发送 16bit 数据（高字节先）
 */
void LCD_WriteData_16bit(uint16_t lcd_data)
{
    LCD_SPI_INSTANCE->DR = lcd_data >> 8;
    while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));

    LCD_SPI_INSTANCE->DR = lcd_data & 0xFF;
    while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));
}

/**
 * @brief  批量写入 16bit 像素数据（切换为 16bit SPI 提升速度）
 */
void LCD_WriteBuff(uint16_t *DataBuff, uint16_t DataSize)
{
    uint32_t i;

    /* 切换为 16bit 数据格式 */
    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 |= SPI_CR1_DFF;   /* DFF=1: 16bit */
    __HAL_SPI_ENABLE(&hspi_lcd);

    LCD_CS_L;

    for (i = 0; i < DataSize; i++) {
        LCD_SPI_INSTANCE->DR = DataBuff[i];
        while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));
    }
    while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);

    LCD_CS_H;

    /* 切换回 8bit */
    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 &= ~SPI_CR1_DFF;  /* DFF=0: 8bit */
    __HAL_SPI_ENABLE(&hspi_lcd);
}

/**
 * @brief  LCD 完整初始化（GPIO + SPI + 寄存器配置）
 */
void SPI_LCD_Init(void)
{
    LCD_GPIO_Config();
    LCD_SPI_Config();

    HAL_Delay(5);   /* 等待屏幕上电复位稳定 */

    LCD_CS_L;

    /* Memory Access Control: Top→Bottom, Left→Right, RGB */
    LCD_WriteCommand(0x36);
    LCD_WriteData_8bit(0x00);

    /* 接口像素格式: 16bit RGB565 */
    LCD_WriteCommand(0x3A);
    LCD_WriteData_8bit(0x05);

    /* Porch Setting */
    LCD_WriteCommand(0xB2);
    LCD_WriteData_8bit(0x0C);
    LCD_WriteData_8bit(0x0C);
    LCD_WriteData_8bit(0x00);
    LCD_WriteData_8bit(0x33);
    LCD_WriteData_8bit(0x33);

    /* Gate Control: VGH=13.26V, VGL=-10.43V */
    LCD_WriteCommand(0xB7);
    LCD_WriteData_8bit(0x35);

    /* VCOM: 1.35V */
    LCD_WriteCommand(0xBB);
    LCD_WriteData_8bit(0x19);

    LCD_WriteCommand(0xC0);
    LCD_WriteData_8bit(0x2C);

    /* VDV and VRH Command Enable */
    LCD_WriteCommand(0xC2);
    LCD_WriteData_8bit(0x01);

    /* VRH: 4.6+(vcom+vcom offset+vdv) */
    LCD_WriteCommand(0xC3);
    LCD_WriteData_8bit(0x12);

    /* VDV: 0V */
    LCD_WriteCommand(0xC4);
    LCD_WriteData_8bit(0x20);

    /* Frame Rate: 60Hz */
    LCD_WriteCommand(0xC6);
    LCD_WriteData_8bit(0x0F);

    /* Power Control: AVDD=6.8V, AVDD=-4.8V, VDS=2.3V */
    LCD_WriteCommand(0xD0);
    LCD_WriteData_8bit(0xA4);
    LCD_WriteData_8bit(0xA1);

    /* Positive Gamma */
    LCD_WriteCommand(0xE0);
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

    /* Negative Gamma */
    LCD_WriteCommand(0xE1);
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

    LCD_WriteCommand(0x21);   /* 反色开（屏幕为常白型） */

    LCD_WriteCommand(0x11);   /* Sleep Out */
    HAL_Delay(120);           /* 等待电源稳定 */

    LCD_WriteCommand(0x29);   /* 开显示 */

    while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);
    LCD_CS_H;

    /* 设置默认参数 */
    LCD_SetDirection(Direction_H);
    LCD_SetBackColor(LCD_BLACK);
    LCD_SetColor(LCD_WHITE);
    LCD_Clear();
    LCD_SetAsciiFont(&ASCII_Font24);

    LCD_Backlight_ON;         /* 开背光 */
}

/* ======================== 地址 / 颜色 / 方向 ======================== */

void LCD_SetAddress(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD_CS_L;

    LCD_WriteCommand(0x2A);   /* 列地址 */
    LCD_WriteData_16bit(x1 + LCD.X_Offset);
    LCD_WriteData_16bit(x2 + LCD.X_Offset);

    LCD_WriteCommand(0x2B);   /* 行地址 */
    LCD_WriteData_16bit(y1 + LCD.Y_Offset);
    LCD_WriteData_16bit(y2 + LCD.Y_Offset);

    LCD_WriteCommand(0x2C);   /* 开始写显存 */

    while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);
    LCD_CS_H;
}

void LCD_SetColor(uint32_t Color)
{
    uint16_t r = (uint16_t)((Color & 0x00F80000) >> 8);
    uint16_t g = (uint16_t)((Color & 0x0000FC00) >> 5);
    uint16_t b = (uint16_t)((Color & 0x000000F8) >> 3);
    LCD.Color = r | g | b;
}

void LCD_SetBackColor(uint32_t Color)
{
    uint16_t r = (uint16_t)((Color & 0x00F80000) >> 8);
    uint16_t g = (uint16_t)((Color & 0x0000FC00) >> 5);
    uint16_t b = (uint16_t)((Color & 0x000000F8) >> 3);
    LCD.BackColor = r | g | b;
}

void LCD_SetDirection(uint8_t direction)
{
    LCD.Direction = direction;
    LCD_CS_L;

    switch (direction) {
        case Direction_H:
            LCD_WriteCommand(0x36);
            LCD_WriteData_8bit(0x70);
            LCD.X_Offset = 20;
            LCD.Y_Offset = 0;
            LCD.Width    = LCD_Height;
            LCD.Height   = LCD_Width;
            break;
        case Direction_H_Flip:
            LCD_WriteCommand(0x36);
            LCD_WriteData_8bit(0xB0);
            LCD.X_Offset = 20;
            LCD.Y_Offset = 0;
            LCD.Width    = LCD_Height;
            LCD.Height   = LCD_Width;
            break;
        case Direction_V:
            LCD_WriteCommand(0x36);
            LCD_WriteData_8bit(0x00);
            LCD.X_Offset = 0;
            LCD.Y_Offset = 20;
            LCD.Width    = LCD_Width;
            LCD.Height   = LCD_Height;
            break;
        case Direction_V_Flip:
        default:
            LCD_WriteCommand(0x36);
            LCD_WriteData_8bit(0xC0);
            LCD.X_Offset = 0;
            LCD.Y_Offset = 20;
            LCD.Width    = LCD_Width;
            LCD.Height   = LCD_Height;
            break;
    }

    while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);
    LCD_CS_H;
}

void LCD_SetAsciiFont(pFONT *Asciifonts)
{
    LCD_AsciiFonts = Asciifonts;
}

void LCD_SetTextFont(pFONT *fonts)
{
    LCD_CHFonts = fonts;
}

/* ======================== 清屏 ======================== */

void LCD_Clear(void)
{
    uint32_t i;
    uint32_t total = (uint32_t)LCD.Width * LCD.Height;

    LCD_SetAddress(0, 0, LCD.Width - 1, LCD.Height - 1);

    /* 填充缓冲区为背景色 */
    for (i = 0; i < 1024; i++) {
        LCD_Buff[i] = (uint16_t)LCD.BackColor;
    }

    LCD_CS_L;
    LCD_DC_Data;

    /* 切换为 16bit SPI */
    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 |= SPI_CR1_DFF;
    __HAL_SPI_ENABLE(&hspi_lcd);

    for (i = 0; i < total / 1024; i++) {
        uint32_t j;
        for (j = 0; j < 1024; j++) {
            LCD_SPI_INSTANCE->DR = LCD_Buff[j];
            while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));
        }
    }
    /* 剩余像素 */
    for (i = 0; i < (total % 1024); i++) {
        LCD_SPI_INSTANCE->DR = (uint16_t)LCD.BackColor;
        while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));
    }
    while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);

    LCD_CS_H;

    /* 切回 8bit */
    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 &= ~SPI_CR1_DFF;
    __HAL_SPI_ENABLE(&hspi_lcd);
}

void LCD_ClearRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    uint32_t total = (uint32_t)width * height;
    uint32_t i;

    LCD_SetAddress(x, y, x + width - 1, y + height - 1);

    LCD_CS_L;
    LCD_DC_Data;

    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 |= SPI_CR1_DFF;
    __HAL_SPI_ENABLE(&hspi_lcd);

    for (i = 0; i < total; i++) {
        LCD_SPI_INSTANCE->DR = (uint16_t)LCD.BackColor;
        while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));
    }
    while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);

    LCD_CS_H;

    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 &= ~SPI_CR1_DFF;
    __HAL_SPI_ENABLE(&hspi_lcd);
}

/* ======================== 画点 ======================== */

void LCD_DrawPoint(uint16_t x, uint16_t y, uint32_t color)
{
    uint16_t r = (uint16_t)((color & 0x00F80000) >> 8);
    uint16_t g = (uint16_t)((color & 0x0000FC00) >> 5);
    uint16_t b = (uint16_t)((color & 0x000000F8) >> 3);
    uint16_t c565 = r | g | b;

    LCD_SetAddress(x, y, x, y);

    LCD_CS_L;
    LCD_DC_Data;

    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 |= SPI_CR1_DFF;
    __HAL_SPI_ENABLE(&hspi_lcd);

    LCD_SPI_INSTANCE->DR = c565;
    while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));
    while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);

    LCD_CS_H;

    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 &= ~SPI_CR1_DFF;
    __HAL_SPI_ENABLE(&hspi_lcd);
}

/* ======================== 填充矩形 ======================== */

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    uint32_t total = (uint32_t)width * height;
    uint32_t i;

    LCD_SetAddress(x, y, x + width - 1, y + height - 1);

    LCD_CS_L;
    LCD_DC_Data;

    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 |= SPI_CR1_DFF;
    __HAL_SPI_ENABLE(&hspi_lcd);

    for (i = 0; i < total; i++) {
        LCD_SPI_INSTANCE->DR = (uint16_t)LCD.Color;
        while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));
    }
    while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);

    LCD_CS_H;

    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 &= ~SPI_CR1_DFF;
    __HAL_SPI_ENABLE(&hspi_lcd);
}

/* ======================== 显示字符 ======================== */

void LCD_DisplayChar(uint16_t x, uint16_t y, uint8_t c)
{
    uint16_t i, j;
    uint8_t  temp;
    uint16_t char_w = LCD_AsciiFonts->Width;
    uint16_t char_h = LCD_AsciiFonts->Height;
    const uint8_t *pChar = LCD_AsciiFonts->pTable +
                           (c - ' ') * LCD_AsciiFonts->Sizes;

    /* 一次性设置字符区域窗口，然后批量写入所有像素 */
    LCD_SetAddress(x, y, x + char_w - 1, y + char_h - 1);

    LCD_CS_L;
    LCD_DC_Data;

    /* 切换为 16bit SPI，批量写像素 */
    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 |= SPI_CR1_DFF;
    __HAL_SPI_ENABLE(&hspi_lcd);

    for (i = 0; i < char_h; i++) {
        for (j = 0; j < char_w; j++) {
            if (j % 8 == 0) {
                temp = *pChar++;
            }
            if (temp & 0x01) {                      /* LSB 在左 */
                LCD_SPI_INSTANCE->DR = (uint16_t)LCD.Color;
            } else {
                LCD_SPI_INSTANCE->DR = (uint16_t)LCD.BackColor;
            }
            while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));
            temp >>= 1;                              /* 右移，下一位到位0 */
        }
    }
    while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);

    LCD_CS_H;

    /* 切回 8bit */
    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 &= ~SPI_CR1_DFF;
    __HAL_SPI_ENABLE(&hspi_lcd);
}

void LCD_DisplayString(uint16_t x, uint16_t y, char *p)
{
    while (*p) {
        if (x + LCD_AsciiFonts->Width > LCD.Width) {
            x = 0;
            y += LCD_AsciiFonts->Height;
        }
        LCD_DisplayChar(x, y, *p);
        x += LCD_AsciiFonts->Width;
        p++;
    }
}

void LCD_DisplayChinese(uint16_t x, uint16_t y, char *pText)
{
    uint16_t i, j;
    uint8_t  temp;
    uint16_t font_w = LCD_CHFonts->Width;
    uint16_t font_h = LCD_CHFonts->Height;
    uint16_t font_s = LCD_CHFonts->Sizes;
    uint16_t cx = x;

    while (pText[0] && pText[1]) {
        const uint8_t *pChar = LCD_CHFonts->pTable;
        /* 查找汉字字模（每个汉字 2 字节编码 + 字模数据） */
        while (*pChar) {
            if ((pChar[0] == (uint8_t)pText[0]) &&
                (pChar[1] == (uint8_t)pText[1])) {
                pChar += 2;
                break;
            }
            pChar += 2 + font_s;
        }

        /* 一次性设置字符窗口，批量写像素 */
        LCD_SetAddress(cx, y, cx + font_w - 1, y + font_h - 1);

        LCD_CS_L;
        LCD_DC_Data;

        __HAL_SPI_DISABLE(&hspi_lcd);
        LCD_SPI_INSTANCE->CR1 |= SPI_CR1_DFF;
        __HAL_SPI_ENABLE(&hspi_lcd);

        for (i = 0; i < font_h; i++) {
            for (j = 0; j < font_w; j++) {
                if (j % 8 == 0) {
                    temp = *pChar++;
                }
                if (temp & 0x01) {                  /* LSB 在左 */
                    LCD_SPI_INSTANCE->DR = (uint16_t)LCD.Color;
                } else {
                    LCD_SPI_INSTANCE->DR = (uint16_t)LCD.BackColor;
                }
                while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));
                temp >>= 1;
            }
        }
        while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);

        LCD_CS_H;

        __HAL_SPI_DISABLE(&hspi_lcd);
        LCD_SPI_INSTANCE->CR1 &= ~SPI_CR1_DFF;
        __HAL_SPI_ENABLE(&hspi_lcd);

        pText += 2;
        cx    += font_w;
    }
}

void LCD_DisplayText(uint16_t x, uint16_t y, char *pText)
{
    while (*pText) {
        if ((uint8_t)*pText >= 0x80) {
            /* 中文（UTF-8 / GBK 双字节） */
            LCD_DisplayChinese(x, y, pText);
            x    += LCD_CHFonts->Width;
            pText += 2;
        } else {
            LCD_DisplayChar(x, y, *pText);
            x    += LCD_AsciiFonts->Width;
            pText++;
        }
        if (x >= LCD.Width) {
            x  = 0;
            y += LCD_CHFonts->Height;
        }
    }
}

/* ======================== 画线 ======================== */

void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    int dx = ABS((int)x2 - (int)x1);
    int dy = ABS((int)y2 - (int)y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    int e2;

    while (1) {
        LCD_DrawPoint(x1, y1, LCD.Color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 = (uint16_t)((int)x1 + sx); }
        if (e2 <  dx) { err += dx; y1 = (uint16_t)((int)y1 + sy); }
    }
}

void LCD_DrawLine_V(uint16_t x, uint16_t y, uint16_t height)
{
    uint16_t i;
    for (i = 0; i < height; i++) {
        LCD_DrawPoint(x, y + i, LCD.Color);
    }
}

void LCD_DrawLine_H(uint16_t x, uint16_t y, uint16_t width)
{
    uint16_t i;
    for (i = 0; i < width; i++) {
        LCD_DrawPoint(x + i, y, LCD.Color);
    }
}

/* ======================== 矩形 ======================== */

void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    LCD_DrawLine_H(x, y, width);
    LCD_DrawLine_H(x, y + height - 1, width);
    LCD_DrawLine_V(x, y, height);
    LCD_DrawLine_V(x + width - 1, y, height);
}

/* ======================== 圆 ======================== */

void LCD_DrawCircle(uint16_t x0, uint16_t y0, uint16_t r)
{
    int x = 0, y = (int)r;
    int d = 1 - (int)r;

    while (x <= y) {
        LCD_DrawPoint((uint16_t)(x0 + x), (uint16_t)(y0 + y), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 - x), (uint16_t)(y0 + y), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 + x), (uint16_t)(y0 - y), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 - x), (uint16_t)(y0 - y), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 + y), (uint16_t)(y0 + x), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 - y), (uint16_t)(y0 + x), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 + y), (uint16_t)(y0 - x), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 - y), (uint16_t)(y0 - x), LCD.Color);
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

void LCD_FillCircle(uint16_t x0, uint16_t y0, uint16_t r)
{
    int x = 0, y = (int)r;
    int d = 1 - (int)r;

    while (x <= y) {
        LCD_DrawLine_H((uint16_t)(x0 - x), (uint16_t)(y0 + y), (uint16_t)(2 * x + 1));
        LCD_DrawLine_H((uint16_t)(x0 - x), (uint16_t)(y0 - y), (uint16_t)(2 * x + 1));
        LCD_DrawLine_H((uint16_t)(x0 - y), (uint16_t)(y0 + x), (uint16_t)(2 * y + 1));
        LCD_DrawLine_H((uint16_t)(x0 - y), (uint16_t)(y0 - x), (uint16_t)(2 * y + 1));
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

/* ======================== 椭圆 ======================== */

void LCD_DrawEllipse(int x0, int y0, int r1, int r2)
{
    int x, y;
    long r1sq = (long)r1 * r1;
    long r2sq = (long)r2 * r2;
    long d1, d2;
    x = 0; y = r2;

    d1 = r2sq - r1sq * r2 + r1sq / 4;
    while ((long)r2sq * x < (long)r1sq * y) {
        LCD_DrawPoint((uint16_t)(x0 + x), (uint16_t)(y0 + y), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 - x), (uint16_t)(y0 + y), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 + x), (uint16_t)(y0 - y), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 - x), (uint16_t)(y0 - y), LCD.Color);
        if (d1 < 0) {
            d1 += 2 * r2sq * x + 3 * r2sq;
        } else {
            d1 += 2 * r2sq * x - 2 * r1sq * y + 3 * r2sq + 2 * r1sq;
            y--;
        }
        x++;
    }
    d2 = r2sq * (x * x + x) + r1sq * (y * y - y) - r1sq * r2sq;
    while (y >= 0) {
        LCD_DrawPoint((uint16_t)(x0 + x), (uint16_t)(y0 + y), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 - x), (uint16_t)(y0 + y), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 + x), (uint16_t)(y0 - y), LCD.Color);
        LCD_DrawPoint((uint16_t)(x0 - x), (uint16_t)(y0 - y), LCD.Color);
        if (d2 > 0) {
            d2 -= 2 * r1sq * y + r1sq;
        } else {
            d2 += 2 * r2sq * x - 2 * r1sq * y + 2 * r2sq + r1sq;
            x++;
        }
        y--;
    }
}

void DrawFillEllipse(int x0, int y0, int rx, int ry)
{
    int x, y;
    long rxsq = (long)rx * rx;
    long rysq = (long)ry * ry;
    long d1, d2;
    x = 0; y = ry;

    d1 = rysq - rxsq * ry + rxsq / 4;
    while ((long)rysq * x < (long)rxsq * y) {
        LCD_DrawLine_H((uint16_t)(x0 - x), (uint16_t)(y0 + y), (uint16_t)(2 * x + 1));
        LCD_DrawLine_H((uint16_t)(x0 - x), (uint16_t)(y0 - y), (uint16_t)(2 * x + 1));
        if (d1 < 0) {
            d1 += 2 * rysq * x + 3 * rysq;
        } else {
            d1 += 2 * rysq * x - 2 * rxsq * y + 3 * rysq + 2 * rxsq;
            y--;
        }
        x++;
    }
    d2 = rysq * (x * x + x) + rxsq * (y * y - y) - rxsq * rysq;
    while (y >= 0) {
        LCD_DrawLine_H((uint16_t)(x0 - x), (uint16_t)(y0 + y), (uint16_t)(2 * x + 1));
        LCD_DrawLine_H((uint16_t)(x0 - x), (uint16_t)(y0 - y), (uint16_t)(2 * x + 1));
        if (d2 > 0) {
            d2 -= 2 * rxsq * y + rxsq;
        } else {
            d2 += 2 * rysq * x - 2 * rxsq * y + 2 * rysq + rxsq;
            x++;
        }
        y--;
    }
}

/* ======================== 圆角矩形 ======================== */

void DrawCircleHelper(int x0, int y0, unsigned char r, unsigned char cornername)
{
    int f     = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x     = 0;
    int y     = r;

    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++; ddF_x += 2; f += ddF_x;

        if (cornername & 0x4) {
            LCD_DrawPoint((uint16_t)(x0 + x), (uint16_t)(y0 + y), LCD.Color);
            LCD_DrawPoint((uint16_t)(x0 + y), (uint16_t)(y0 + x), LCD.Color);
        }
        if (cornername & 0x2) {
            LCD_DrawPoint((uint16_t)(x0 + x), (uint16_t)(y0 - y), LCD.Color);
            LCD_DrawPoint((uint16_t)(x0 + y), (uint16_t)(y0 - x), LCD.Color);
        }
        if (cornername & 0x8) {
            LCD_DrawPoint((uint16_t)(x0 - y), (uint16_t)(y0 + x), LCD.Color);
            LCD_DrawPoint((uint16_t)(x0 - x), (uint16_t)(y0 + y), LCD.Color);
        }
        if (cornername & 0x1) {
            LCD_DrawPoint((uint16_t)(x0 - y), (uint16_t)(y0 - x), LCD.Color);
            LCD_DrawPoint((uint16_t)(x0 - x), (uint16_t)(y0 - y), LCD.Color);
        }
    }
}

void DrawFillCircleHelper(int x0, int y0, unsigned char r, unsigned char cornername, int delta)
{
    int f     = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x     = 0;
    int y     = r;

    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++; ddF_x += 2; f += ddF_x;

        if (cornername & 0x1) {
            LCD_DrawLine_V((uint16_t)(x0 + x), (uint16_t)(y0 - y), (uint16_t)(2 * y + 1 + delta));
            LCD_DrawLine_V((uint16_t)(x0 + y), (uint16_t)(y0 - x), (uint16_t)(2 * x + 1 + delta));
        }
        if (cornername & 0x2) {
            LCD_DrawLine_V((uint16_t)(x0 - x), (uint16_t)(y0 - y), (uint16_t)(2 * y + 1 + delta));
            LCD_DrawLine_V((uint16_t)(x0 - y), (uint16_t)(y0 - x), (uint16_t)(2 * x + 1 + delta));
        }
    }
}

void DrawRoundRect(int x, int y, unsigned char w, unsigned char h, unsigned char r)
{
    LCD_DrawLine_H((uint16_t)(x + r),       (uint16_t)(y),         (uint16_t)(w - 2 * r));
    LCD_DrawLine_H((uint16_t)(x + r),       (uint16_t)(y + h - 1), (uint16_t)(w - 2 * r));
    LCD_DrawLine_V((uint16_t)(x),           (uint16_t)(y + r),     (uint16_t)(h - 2 * r));
    LCD_DrawLine_V((uint16_t)(x + w - 1),   (uint16_t)(y + r),     (uint16_t)(h - 2 * r));
    DrawCircleHelper(x + r,         y + r,         r, 0x1);
    DrawCircleHelper(x + w - r - 1, y + r,         r, 0x2);
    DrawCircleHelper(x + w - r - 1, y + h - r - 1, r, 0x4);
    DrawCircleHelper(x + r,         y + h - r - 1, r, 0x8);
}

void DrawfillRoundRect(int x, int y, unsigned char w, unsigned char h, unsigned char r)
{
    LCD_FillRect((uint16_t)(x + r), (uint16_t)y, (uint16_t)(w - 2 * r), (uint16_t)h);
    DrawFillCircleHelper(x + w - r - 1, y + r, r, 0x1, h - 2 * r - 1);
    DrawFillCircleHelper(x + r,         y + r, r, 0x2, h - 2 * r - 1);
}

/* ======================== 三角形 ======================== */

void DrawTriangle(unsigned char x0, unsigned char y0,
                  unsigned char x1, unsigned char y1,
                  unsigned char x2, unsigned char y2)
{
    LCD_DrawLine(x0, y0, x1, y1);
    LCD_DrawLine(x1, y1, x2, y2);
    LCD_DrawLine(x2, y2, x0, y0);
}

void DrawFillTriangle(int x0, int y0, int x1, int y1, int x2, int y2)
{
    int a, b, y, last;
    int dx01, dy01, dx02, dy02, dx12, dy12;
    long sa = 0, sb = 0;

    if (y0 > y1) { SWAP(y0, y1); SWAP(x0, x1); }
    if (y1 > y2) { SWAP(y2, y1); SWAP(x2, x1); }
    if (y0 > y1) { SWAP(y0, y1); SWAP(x0, x1); }

    if (y0 == y2) {
        a = b = x0;
        if (x1 < a) a = x1; else if (x1 > b) b = x1;
        if (x2 < a) a = x2; else if (x2 > b) b = x2;
        LCD_DrawLine_H((uint16_t)a, (uint16_t)y0, (uint16_t)(b - a + 1));
        return;
    }

    dx01 = x1 - x0; dy01 = y1 - y0;
    dx02 = x2 - x0; dy02 = y2 - y0;
    dx12 = x2 - x1; dy12 = y2 - y1;

    if (y1 == y2) {
        last = y1;
    } else {
        last = y1 - 1;
    }

    for (y = y0; y <= last; y++) {
        a  = x0 + sa / dy01;
        b  = x0 + sb / dy02;
        sa += dx01; sb += dx02;
        if (a > b) { SWAP(a, b); }
        LCD_DrawLine_H((uint16_t)a, (uint16_t)y, (uint16_t)(b - a + 1));
    }

    sa = (long)dx12 * (y - y1);
    sb = (long)dx02 * (y - y0);
    for (; y <= y2; y++) {
        a  = x1 + sa / dy12;
        b  = x0 + sb / dy02;
        sa += dx12; sb += dx02;
        if (a > b) { SWAP(a, b); }
        LCD_DrawLine_H((uint16_t)a, (uint16_t)y, (uint16_t)(b - a + 1));
    }
}

/* ======================== 圆弧 ======================== */

void DrawArc(int x, int y, unsigned char r, int angle_start, int angle_end)
{
    int i;
    for (i = angle_start; i <= angle_end; i++) {
        int px = x + (int)((float)r * cosf(RADIAN(i)));
        int py = y + (int)((float)r * sinf(RADIAN(i)));
        LCD_DrawPoint((uint16_t)px, (uint16_t)py, LCD.Color);
    }
}

/* ======================== 图片 ======================== */

void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                   const uint8_t *pImage)
{
    uint32_t total = (uint32_t)width * height;
    uint32_t i;

    LCD_SetAddress(x, y, x + width - 1, y + height - 1);

    LCD_CS_L;
    LCD_DC_Data;

    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 |= SPI_CR1_DFF;
    __HAL_SPI_ENABLE(&hspi_lcd);

    for (i = 0; i < total; i++) {
        uint16_t color = ((uint16_t)pImage[2 * i] << 8) | pImage[2 * i + 1];
        LCD_SPI_INSTANCE->DR = color;
        while (!(LCD_SPI_INSTANCE->SR & SPI_SR_TXE));
    }
    while (LCD_SPI_INSTANCE->SR & SPI_SR_BSY);

    LCD_CS_H;

    __HAL_SPI_DISABLE(&hspi_lcd);
    LCD_SPI_INSTANCE->CR1 &= ~SPI_CR1_DFF;
    __HAL_SPI_ENABLE(&hspi_lcd);
}

/* ======================== 旋转工具 ======================== */

float mySqrt(float x)
{
    return sqrtf(x);
}

TypeXY GetXY(void)
{
    TypeXY p = {_pointx, _pointy};
    return p;
}

void SetRotateCenter(int x0, int y0)
{
    _RoateValue.center.x = x0;
    _RoateValue.center.y = y0;
}

void SetAngleDir(int direction)
{
    _RoateValue.direct = direction;
}

void SetAngle(float angle)
{
    _RoateValue.angle = angle;
}

void SetRotateValue(int x, int y, float angle, int direct)
{
    _RoateValue.center.x = x;
    _RoateValue.center.y = y;
    _RoateValue.angle    = angle;
    _RoateValue.direct   = direct;
}

static void Rotate(int x0, int y0, int *x, int *y, double angle, int direction)
{
    double rad    = angle * 3.14159265 / 180.0;
    int    dx     = *x - x0;
    int    dy     = *y - y0;
    double cosA   = cos(rad);
    double sinA   = sin(rad);

    if (direction >= 0) {
        *x = x0 + (int)(dx * cosA - dy * sinA);
        *y = y0 + (int)(dx * sinA + dy * cosA);
    } else {
        *x = x0 + (int)(dx * cosA + dy * sinA);
        *y = y0 + (int)(-dx * sinA + dy * cosA);
    }
}

TypeXY GetRotateXY(int x, int y)
{
    TypeXY result = {x, y};
    Rotate(_RoateValue.center.x, _RoateValue.center.y,
           &result.x, &result.y,
           (double)_RoateValue.angle, _RoateValue.direct);
    return result;
}

void MoveTo(int x, int y)
{
    _pointx = x;
    _pointy = y;
}

void LineTo(int x, int y)
{
    LCD_DrawLine((uint16_t)_pointx, (uint16_t)_pointy,
                 (uint16_t)x, (uint16_t)y);
    _pointx = x;
    _pointy = y;
}
