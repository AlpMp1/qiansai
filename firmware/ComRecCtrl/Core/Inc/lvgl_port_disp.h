#ifndef LVGL_PORT_DISP_H
#define LVGL_PORT_DISP_H

/**
 * @file  lvgl_port_disp.h
 * @brief LVGL 显示驱动移植层：双缓冲 + DMA 非阻塞刷新
 *
 * 内存约束：
 *   buf1/buf2 位于主 SRAM (0x20xxxxxx)，DMA1 可访问。
 *   禁止将缓冲区放在 CCM RAM (0x10000000)，否则 DMA 无法访问！
 *
 * 字节序：
 *   在 lv_conf.h 中必须设置 LV_COLOR_16_SWAP 1，
 *   使 SPI 8-bit DMA 字节模式下 RGB565 高字节先发。
 */

#include "stm32f4xx_hal.h"  /* for HAL_SPI_TxCpltCallback */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL 显示驱动，在 lv_init() 之后调用一次
 */
void lvgl_port_disp_init(void);

/**
 * @brief SPI DMA TX 完成回调（HAL 弱函数重写）
 *        由 SPI3_IRQHandler → HAL_SPI_IRQHandler 自动调用，无需手动调用
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_PORT_DISP_H */
