#include "servo.h"
#include "tim.h"   /* extern TIM_HandleTypeDef htim3, htim4 */

/* ── 舵机与 TIM 通道映射表 ── */
typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t           ch;
} Servo_Map_t;

static const Servo_Map_t MAP[SERVO_NUM] = {
    {&htim3, TIM_CHANNEL_1},   /* SERVO_1 PA6 */
    {&htim3, TIM_CHANNEL_2},   /* SERVO_2 PA7 */
    {&htim3, TIM_CHANNEL_3},   /* SERVO_3 PB0 */
    {&htim3, TIM_CHANNEL_4},   /* SERVO_4 PB1 */
    {&htim4, TIM_CHANNEL_1},   /* SERVO_5 PB6 */
};

/**
 * @brief 启动全部 PWM 通道，输出停止脉宽
 *
 * 注意：CubeMX 生成的 TIM3_CH2/CH3/CH4 初始 Pulse=0，
 * 必须先写 CCR=1500 再 Start，避免引脚出现 0μs 异常脉冲。
 */
void Servo_Init(void)
{
    /* 先预置 CCR，防止 Start 瞬间输出异常脉宽 */
    for (int i = 0; i < SERVO_NUM; i++) {
        __HAL_TIM_SET_COMPARE(MAP[i].htim, MAP[i].ch, SERVO_PULSE_STOP);
    }
    /* 再使能各通道输出 */
    for (int i = 0; i < SERVO_NUM; i++) {
        HAL_TIM_PWM_Start(MAP[i].htim, MAP[i].ch);
    }
}

/**
 * @brief 直接设置脉宽（μs），范围 1000~2000
 */
void Servo_SetPulse(Servo_ID id, uint16_t pulse_us)
{
    if (id >= SERVO_NUM) return;
    if (pulse_us < 1000U) pulse_us = 1000U;
    if (pulse_us > 2000U) pulse_us = 2000U;
    /* TIM tick = 1μs，CCR 直接等于脉宽 μs */
    __HAL_TIM_SET_COMPARE(MAP[id].htim, MAP[id].ch, pulse_us);
}

/**
 * @brief 按方向和速度百分比控制（0~100%）
 *
 * 正转：1500 → 1000（脉宽随速度减小）
 * 反转：1500 → 2000（脉宽随速度增大）
 */
void Servo_SetSpeed(Servo_ID id, Servo_Dir dir, uint8_t speed_pct)
{
    if (speed_pct > 100U) speed_pct = 100U;

    uint16_t pulse;
    switch (dir) {
        case SERVO_FORWARD:
            pulse = (uint16_t)(SERVO_PULSE_STOP -
                    (uint32_t)(SERVO_PULSE_STOP - SERVO_PULSE_FWD_MAX)
                    * speed_pct / 100U);
            break;
        case SERVO_REVERSE:
            pulse = (uint16_t)(SERVO_PULSE_STOP +
                    (uint32_t)(SERVO_PULSE_REV_MAX - SERVO_PULSE_STOP)
                    * speed_pct / 100U);
            break;
        default:
            pulse = SERVO_PULSE_STOP;
            break;
    }
    Servo_SetPulse(id, pulse);
}

/**
 * @brief 停止单个舵机
 */
void Servo_Stop(Servo_ID id)
{
    Servo_SetPulse(id, SERVO_PULSE_STOP);
}

/**
 * @brief 停止全部舵机
 */
void Servo_StopAll(void)
{
    for (int i = 0; i < SERVO_NUM; i++) {
        Servo_SetPulse((Servo_ID)i, SERVO_PULSE_STOP);
    }
}
