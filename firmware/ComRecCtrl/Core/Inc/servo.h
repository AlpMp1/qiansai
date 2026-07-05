#ifndef __SERVO_H
#define __SERVO_H

/**
 * @file  servo.h
 * @brief 360° 连续旋转舵机控制
 *
 * 引脚 / TIM 通道映射（ioc 确认）：
 *   Servo1  PA6  TIM3_CH1  APB1TimClk=84MHz, PSC=83 → 1MHz tick, ARR=19999 → 20ms/50Hz
 *   Servo2  PA7  TIM3_CH2
 *   Servo3  PB0  TIM3_CH3
 *   Servo4  PB1  TIM3_CH4
 *   Servo5  PB6  TIM4_CH1
 *
 * 360°舵机响应：
 *   ~1500μs → 停止（实测可能需微调 ±20μs）
 *   <1500μs → 正转（越小越快，最快约 1000μs）
 *   >1500μs → 反转（越大越快，最快约 2000μs）
 */

#include "stm32f4xx_hal.h"

/* ── 舵机编号 ── */
typedef enum {
    SERVO_1 = 0,  /* PA6  TIM3_CH1 */
    SERVO_2,      /* PA7  TIM3_CH2 */
    SERVO_3,      /* PB0  TIM3_CH3 */
    SERVO_4,      /* PB1  TIM3_CH4 */
    SERVO_5,      /* PB6  TIM4_CH1 */
    SERVO_NUM
} Servo_ID;

/* ── 方向枚举 ── */
typedef enum {
    SERVO_STOP    = 0,
    SERVO_FORWARD = 1,
    SERVO_REVERSE = 2,
} Servo_Dir;

/* ── 脉宽常量（μs），可根据实物微调 SERVO_PULSE_STOP ── */
#define SERVO_PULSE_STOP     1500U   /* 停止死区中心 */
#define SERVO_PULSE_FWD_MAX  1000U   /* 正转最快 */
#define SERVO_PULSE_REV_MAX  2000U   /* 反转最快 */

/* ── API ── */
void Servo_Init(void);
void Servo_SetPulse(Servo_ID id, uint16_t pulse_us);
void Servo_SetSpeed(Servo_ID id, Servo_Dir dir, uint8_t speed_pct);
void Servo_Stop(Servo_ID id);
void Servo_StopAll(void);

#endif /* __SERVO_H */
