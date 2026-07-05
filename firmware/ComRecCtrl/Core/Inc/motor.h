#ifndef __MOTOR_H
#define __MOTOR_H

/**
 * @file  motor.h
 * @brief TB6612FNG 直流电机驱动
 *
 * 引脚（ioc 确认）：
 *   STBY  PE2  GPIO Output  — 高电平使能驱动芯片
 *   AIN1  PE3  GPIO Output  — 方向控制 A
 *   AIN2  PE4  GPIO Output  — 方向控制 B
 *   Motor PE5  TIM9_CH1     — PWM 调速
 *
 * TIM9：APB2TimClk=168MHz, PSC=83 → 2MHz tick, ARR=99 → 20kHz PWM
 * CCR 范围 0~99 对应占空比 0~100%
 *
 * TB6612 真值表：
 *   AIN1=0 AIN2=0  → 滑行停止
 *   AIN1=0 AIN2=1  → 正转（占空比由 PWM 决定）
 *   AIN1=1 AIN2=0  → 反转
 *   AIN1=1 AIN2=1  → 短路制动
 */

#include "stm32f4xx_hal.h"
#include "main.h"

typedef enum {
    MOTOR_STOP    = 0,
    MOTOR_FORWARD = 1,
    MOTOR_REVERSE = 2,
    MOTOR_BRAKE   = 3,
} Motor_Dir;

void Motor_Init(void);
void Motor_SetSpeed(Motor_Dir dir, uint8_t speed_pct);
void Motor_Stop(void);
void Motor_Brake(void);

#endif /* __MOTOR_H */
