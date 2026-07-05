/**
 * @file  lvgl_port_disp.c
 * @brief LVGL 显示驱动移植层 — STM32F407 + ST7789 (240×280, RGB565)
 *
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  双缓冲 + DMA 非阻塞刷新                                        │
 * │                                                                 │
 * │  buf1 / buf2：各 240×20 像素 = 9600 字节，存于主 SRAM          │
 * │  注意：CCM RAM (0x10000000) DMA 无法访问，不可将缓冲放在那里！  │
 * │                                                                 │
 * │  工作流程：                                                     │
 * │  1. LVGL 渲染完成 → disp_flush_cb 被调用                       │
 * │  2. LCD_SetAddress 设定写入窗口（阻塞，极短）                   │
 * │  3. HAL_SPI_Transmit_DMA 启动（立即返回，非阻塞）               │
 * │  4. DMA 传输完成 → HAL_SPI_TxCpltCallback 被调用               │
 * │  5. CS 拉高，lv_disp_flush_ready 通知 LVGL 可渲染下一帧        │
 * │                                                                 │
 * │  字节序：LV_COLOR_16_SWAP=1（见 lv_conf.h），使 DMA 字节模式   │
 * │  发出正确的 RGB565 高字节在前的顺序                             │
 * └─────────────────────────────────────────────────────────────────┘
 */

#include "lvgl_port_disp.h"
#include "lvgl/lvgl.h"
#include "lcd_spi_169.h"
#include "spi.h"   /* extern SPI_HandleTypeDef hspi3 */

/* ─────────────────────────────────────────────────────────────────── */
/*  分辨率                                                             */
/* ─────────────────────────────────────────────────────────────────── */
#define DISP_HOR_RES   280          /* 横屏宽 */
#define DISP_VER_RES   240          /* 横屏高 */

/* 每次刷新 40 行：flush 次数 280/40=7 次 vs 之前 14 次，SetAddress 开销减半 */
#define DISP_BUF_LINES 40
#define DISP_BUF_SIZE  (DISP_HOR_RES * DISP_BUF_LINES)

/* ─────────────────────────────────────────────────────────────────── */
/*  缓冲区：必须在主 SRAM，DMA 才能访问                               */
/*  不要加 __attribute__((section(".ccmram"))) !                       */
/* ─────────────────────────────────────────────────────────────────── */
static lv_color_t buf1[DISP_BUF_SIZE];   /* 主 SRAM，DMA 可访问 */
static lv_color_t buf2[DISP_BUF_SIZE];   /* 主 SRAM，DMA 可访问 */

static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t      disp_drv;

/* 保存 drv 指针供 DMA 回调使用 */
static lv_disp_drv_t *p_disp_drv = NULL;

/* ─────────────────────────────────────────────────────────────────── */
/*  DMA 刷新回调（由 HAL_SPI_TxCpltCallback 调用）                    */
/* ─────────────────────────────────────────────────────────────────── */

/**
 * @brief  SPI DMA 传输完成中断回调（弱函数重写）
 *         自动被 SPI3_IRQHandler → HAL_SPI_IRQHandler 调用
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI3)
    {
        /* SPI 恢复 8-bit 模式（DFF=0），供后续 LCD 命令使用 */
        __HAL_SPI_DISABLE(hspi);
        hspi->Instance->CR1 &= ~SPI_CR1_DFF;  /* DFF=0: 8-bit */
        __HAL_SPI_ENABLE(hspi);

        /* 拉高 CS，结束本次 LCD 写入 */
        LCD_CS_H;

        /* 通知 LVGL：本次 flush 完成，可渲染下一块 */
        if (p_disp_drv != NULL)
        {
            lv_disp_flush_ready(p_disp_drv);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────── */
/*  disp_flush_cb：非阻塞 DMA 版本                                     */
/* ─────────────────────────────────────────────────────────────────── */

static void disp_flush_cb(lv_disp_drv_t *drv,
                          const lv_area_t *area,
                          lv_color_t *color_p)
{
    uint32_t pixel_count = (uint32_t)(area->x2 - area->x1 + 1)
                         * (uint32_t)(area->y2 - area->y1 + 1);

    /* 保存 drv 指针，供 TxCpltCallback 使用 */
    p_disp_drv = drv;

    /* 1. 设置 ST7789 写入窗口（阻塞，约 8 字节，极短）
     *    LCD_SetAddress 内部：CS_L → 写 2A/2B/2C → CS_H
     *    完成后 SPI 处于 8-bit 模式，BSY=0                          */
    LCD_SetAddress((uint16_t)area->x1, (uint16_t)area->y1,
                   (uint16_t)area->x2, (uint16_t)area->y2);

    /* 2. 拉低 CS，开始像素流传输 */
    LCD_CS_L;

    /* 3. 切换 SPI 到 16-bit 模式（DFF=1），与 DMA HALFWORD 对齐
     *    DMA 每次搬 16-bit → SPI DR 每次发 16-bit（1 个像素）
     *    LV_COLOR_16_SWAP=0：ARM 小端下 uint16_t 高字节在高地址，
     *    16-bit SPI MSB-first 会自动先发高字节，字节序天然正确     */
    __HAL_SPI_DISABLE(&hspi3);
    hspi3.Instance->CR1 |= SPI_CR1_DFF;    /* DFF=1: 16-bit */
    __HAL_SPI_ENABLE(&hspi3);

    /* 4. 启动 DMA 非阻塞传输
     *    传输个数 = pixel_count（每次 1 个 16-bit 像素，非字节数） */
    HAL_SPI_Transmit_DMA(&hspi3,
                         (uint8_t *)color_p,
                         (uint16_t)pixel_count);

    /* 立即返回 —— DMA 在后台传输，TxCpltCallback 中完成收尾 */
}

/* ─────────────────────────────────────────────────────────────────── */
/*  初始化入口                                                          */
/* ─────────────────────────────────────────────────────────────────── */

/**
 * @brief 初始化 LVGL 显示驱动，在 lv_init() 之后调用一次
 */
void lvgl_port_disp_init(void)
{
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_BUF_SIZE);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = DISP_HOR_RES;
    disp_drv.ver_res  = DISP_VER_RES;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf;

    lv_disp_drv_register(&disp_drv);
}
