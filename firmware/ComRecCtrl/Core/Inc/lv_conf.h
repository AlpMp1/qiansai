/**
 * @file lv_conf.h
 * LVGL v8.3.x 配置文件 — STM32F407VET (192KB RAM, 168MHz)
 * LCD: 240x280, ST7789, RGB565
 */

#if 1  /* 必须为 1 才会启用本配置 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   颜色设置
 *====================*/
/* 颜色深度：16位 RGB565，与 ST7789 匹配 */
#define LV_COLOR_DEPTH 16

/* RGB565 字节序：16-bit DMA + SPI 16-bit 模式下无需软件交换 */
#define LV_COLOR_16_SWAP 0

/* 启用透明通道支持（如不需要保持 0 节省内存） */
#define LV_COLOR_SCREEN_TRANSP 0

/* 画布透明色 */
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)

/*====================
   内存设置
 *====================*/
/* LVGL 内部堆（动态分配对象用），32KB 适合 STM32F407 */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE   (32U * 1024U)

/* 若要将堆放到 CCM RAM (0x10000000, 64KB)，取消注释下行 */
/* #define LV_MEM_ADR 0x10000000 */
#define LV_MEM_ADR 0

/* 内存对齐 */
#define LV_MEM_POOL_INCLUDE <stdlib.h>
#define LV_MEM_POOL_ALLOC   malloc
#define LV_MEM_POOL_FREE    free

/*====================
   HAL 设置
 *====================*/
/* 显示刷新周期（ms）：10ms → 上限 ~100fps，实际帧率由 SPI 速度决定 */
#define LV_DISP_DEF_REFR_PERIOD 10

/* 输入设备读取周期（ms） */
#define LV_INDEV_DEF_READ_PERIOD 33

/* 使用 HAL_GetTick() 提供时间戳，无需在 SysTick 中手动调用 lv_tick_inc */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "stm32f4xx_hal.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (HAL_GetTick())

/* 默认显示刷新任务优先级 */
#define LV_ATTRIBUTE_TASK_HANDLER
#define LV_ATTRIBUTE_FLUSH_READY

/*====================
   分辨率
 *====================*/
#define LV_HOR_RES_MAX 280
#define LV_VER_RES_MAX 240

/*====================
   特性开关（节省 Flash/RAM）
 *====================*/
#define LV_USE_LOG      0   /* 调试日志，生产环境关闭 */
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/*====================
   字体
 *====================*/
/* 内置字体（Montserrat） */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* 默认字体 */
#define LV_FONT_DEFAULT &lv_font_montserrat_16

#define LV_FONT_FMT_TXT_LARGE 0
#define LV_USE_FONT_SUBPX     0

/*====================
   组件开关
 *====================*/
#define LV_USE_ARC          1
#define LV_USE_BAR          1
#define LV_USE_BTN          1
#define LV_USE_BTNMATRIX    1
#define LV_USE_CANVAS       1
#define LV_USE_CHECKBOX     1
#define LV_USE_DROPDOWN     1
#define LV_USE_IMG          1
#define LV_USE_LABEL        1
#define LV_USE_LINE         1
#define LV_USE_ROLLER       1
#define LV_USE_SLIDER       1
#define LV_USE_SWITCH       1
#define LV_USE_TEXTAREA     1
#define LV_USE_TABLE        1

/* 扩展组件（较大，按需开启） */
#define LV_USE_ANIMIMG      0
#define LV_USE_CALENDAR     0
#define LV_USE_CHART        0
#define LV_USE_COLORWHEEL   0
#define LV_USE_IMGBTN       0
#define LV_USE_KEYBOARD     0
#define LV_USE_LED          1
#define LV_USE_LIST         1
#define LV_USE_MENU         0
#define LV_USE_METER        0
#define LV_USE_MSGBOX       0
#define LV_USE_SPAN         0
#define LV_USE_SPINBOX      0
#define LV_USE_SPINNER      1
#define LV_USE_TABVIEW      0
#define LV_USE_TILEVIEW     0
#define LV_USE_WIN          0

/*====================
   主题
 *====================*/
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

#define LV_USE_THEME_BASIC 1
#define LV_USE_THEME_MONO  0

/*====================
   布局
 *====================*/
#define LV_USE_FLEX  1
#define LV_USE_GRID  1

/*====================
   性能 / 内存监视器
 *====================*/
/* 右下角自动显示实际 FPS（LVGL 内部统计） */
#define LV_USE_PERF_MONITOR 0
#if LV_USE_PERF_MONITOR
    #define LV_USE_PERF_MONITOR_POS LV_ALIGN_BOTTOM_RIGHT
#endif

/* 左下角显示 LVGL 堆内存占用 */
#define LV_USE_MEM_MONITOR 0
#if LV_USE_MEM_MONITOR
    #define LV_USE_MEM_MONITOR_POS LV_ALIGN_BOTTOM_LEFT
#endif

/*====================
   演示 Demo (关闭节省 Flash)
 *====================*/
#define LV_USE_DEMO_WIDGETS        0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK      0
#define LV_USE_DEMO_STRESS         0
#define LV_USE_DEMO_MUSIC          0

#endif /* LV_CONF_H */
#endif /* End of "Content enable" */
