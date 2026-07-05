#include "motor.h"
#include "tim.h"   /* extern TIM_HandleTypeDef htim9 */

/* ── GPIO 控制宏（引脚来自 main.h） ── */
#define AIN1_H()  HAL_GPIO_WritePin(AIN1_GPIO_Port,  AIN1_Pin,  GPIO_PIN_SET)
#define AIN1_L()  HAL_GPIO_WritePin(AIN1_GPIO_Port,  AIN1_Pin,  GPIO_PIN_RESET)
#define AIN2_H()  HAL_GPIO_WritePin(AIN2_GPIO_Port,  AIN2_Pin,  GPIO_PIN_SET)
#define AIN2_L()  HAL_GPIO_WritePin(AIN2_GPIO_Port,  AIN2_Pin,  GPIO_PIN_RESET)
#define STBY_H()  HAL_GPIO_WritePin(STBY_GPIO_Port,  STBY_Pin,  GPIO_PIN_SET)
#define STBY_L()  HAL_GPIO_WritePin(STBY_GPIO_Port,  STBY_Pin,  GPIO_PIN_RESET)

/* ── TIM9 ARR=99，CCR=0~99 对应 0~100% ── */
static void set_pwm(uint8_t pct)
{
    if (pct > 100U) pct = 100U;
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, (uint32_t)pct * 99U / 100U);
}

/**
 * @brief 初始化：启动 PWM，使能 STBY，默认停止
 */
void Motor_Init(void)
{
    HAL_TIM_PWM_Start(&htim9, TIM_CHANNEL_1);
    STBY_H();
    Motor_Stop();
}

/**
 * @brief 设置方向和速度（0~100%）
 */
void Motor_SetSpeed(Motor_Dir dir, uint8_t speed_pct)
{
    switch (dir) {
        case MOTOR_FORWARD:
            AIN1_L(); AIN2_H();
            set_pwm(speed_pct);
            break;
        case MOTOR_REVERSE:
            AIN1_H(); AIN2_L();
            set_pwm(speed_pct);
            break;
        case MOTOR_BRAKE:
            Motor_Brake();
            break;
        default:
            Motor_Stop();
            break;
    }
}

/**
 * @brief 滑行停止（AIN1=AIN2=0）
 */
void Motor_Stop(void)
{
    AIN1_L(); AIN2_L();
    set_pwm(0);
}

/**
 * @brief 短路制动（快速停止）
 */
void Motor_Brake(void)
{
    AIN1_H(); AIN2_H();
    set_pwm(100);
}
