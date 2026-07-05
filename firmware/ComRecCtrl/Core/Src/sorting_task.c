#include "sorting_task.h"
#include "servo.h"
#include "ui_app.h"
#include <stdio.h>
#include <string.h>

#define SORT_TASK_MAX 10U
#define SORT_BOX_NUM 5U

#define BOX1_DELAY_MS 8750U
#define ADJ_BOX_SPACING_MS 10500U

#define PUSH_SPEED_PCT 99U
#define PUSH_CW_TIME_MS 1000U
#define PUSH_CCW_TIME_MS 1000U

#define PUSH_CW_DIR SERVO_FORWARD
#define PUSH_CCW_DIR SERVO_REVERSE

static const int16_t BOX_DELAY_TRIM_MS[SORT_BOX_NUM] = {0, 10, -5, 15, 0};

typedef enum {
    RX_WAIT_AA = 0,
    RX_WAIT_ID,
    RX_WAIT_55
} RxParseState;

typedef struct {
    bool active;
    uint8_t phase;
    uint32_t next_tick;
} ServoActionState;

enum {
    SERVO_PHASE_IDLE = 0,
    SERVO_PHASE_CW,
    SERVO_PHASE_CCW
};

static UART_HandleTypeDef *s_huart = NULL;
static uint8_t s_rx_byte = 0;

static SortingTask s_tasks[SORT_TASK_MAX];
static ServoActionState s_servo_actions[SORT_BOX_NUM];
static uint16_t s_box_counts[SORT_BOX_NUM] = {0};

static RxParseState s_rx_state = RX_WAIT_AA;
static uint8_t s_pending_box_id = 0;
static volatile uint8_t s_latest_rx_id = 0;
static volatile bool s_latest_rx_id_dirty = false;

static void publish_current_id_from_queue(void)
{
    uint8_t next_id = 0;

    for (uint32_t i = 0; i < SORT_TASK_MAX; i++) {
        if (s_tasks[i].is_active) {
            next_id = s_tasks[i].target_box_id;
            break;
        }
    }

    if (next_id == 0U) {
        update_processing_component_id("--");
    } else {
        char id_text[20];
        (void)snprintf(id_text, sizeof(id_text), "ID-%u", next_id);
        update_processing_component_id(id_text);
    }
}

static void publish_pending_queue_to_ui(void)
{
    char queue_text[64];
    uint32_t pos = 0;
    uint8_t active_cnt = 0;

    pos += (uint32_t)snprintf(queue_text + pos, sizeof(queue_text) - pos, "Queue:");

    for (uint32_t i = 0; i < SORT_TASK_MAX; i++) {
        if (!s_tasks[i].is_active) {
            continue;
        }

        if ((sizeof(queue_text) - pos) <= 4U) {
            break;
        }

        pos += (uint32_t)snprintf(queue_text + pos,
                                  sizeof(queue_text) - pos,
                                  " %u",
                                  s_tasks[i].target_box_id);
        active_cnt++;
    }

    if (active_cnt == 0U) {
        (void)snprintf(queue_text, sizeof(queue_text), "Queue: --");
    }

    update_pending_queue(queue_text);
}

static void publish_latest_rx_id_to_ui(void)
{
    if (!s_latest_rx_id_dirty) {
        return;
    }

    uint8_t latest_id;

    __disable_irq();
    latest_id = s_latest_rx_id;
    s_latest_rx_id_dirty = false;
    __enable_irq();

    char id_text[20];
    (void)snprintf(id_text, sizeof(id_text), "ID-%u", latest_id);
    update_processing_component_id(id_text);
}

static inline bool tick_reached(uint32_t now, uint32_t target)
{
    return ((int32_t)(now - target) >= 0);
}

static bool box_id_to_servo(uint8_t box_id, Servo_ID *sid)
{
    if (box_id < 1U || box_id > SORT_BOX_NUM) {
        return false;
    }
    *sid = (Servo_ID)(box_id - 1U);
    return true;
}

static uint32_t calc_trigger_tick(uint8_t box_id, uint32_t now)
{
    int32_t delay = (int32_t)BOX1_DELAY_MS;
    if (box_id >= 1U && box_id <= SORT_BOX_NUM) {
        delay += (int32_t)((uint32_t)(box_id - 1U) * ADJ_BOX_SPACING_MS);
        delay += BOX_DELAY_TRIM_MS[box_id - 1U];
    }
    if (delay < 0) {
        delay = 0;
    }
    return now + (uint32_t)delay;
}

static void enqueue_task(uint8_t box_id)
{
    if (box_id < 1U || box_id > SORT_BOX_NUM) {
        return;
    }

    uint32_t now = HAL_GetTick();
    for (uint32_t i = 0; i < SORT_TASK_MAX; i++) {
        if (!s_tasks[i].is_active) {
            s_tasks[i].target_box_id = box_id;
            s_tasks[i].trigger_tick = calc_trigger_tick(box_id, now);
            s_tasks[i].is_active = true;
            publish_pending_queue_to_ui();
            publish_current_id_from_queue();
            return;
        }
    }
}

/* Serial protocol from upper host: 0xAA, box_id, 0x55.
 * box_id is the physical sorting box ID in 1..5.
 * Invalid IDs, including the old ceramic model class 0, are ignored.
 */
static void parse_byte(uint8_t value)
{
    switch (s_rx_state) {
    case RX_WAIT_AA:
        if (value == 0xAAU) {
            s_rx_state = RX_WAIT_ID;
        }
        break;

    case RX_WAIT_ID:
        s_pending_box_id = value;
        s_rx_state = RX_WAIT_55;
        break;

    case RX_WAIT_55:
        if (value == 0x55U) {
            enqueue_task(s_pending_box_id);
            if (s_pending_box_id >= 1U && s_pending_box_id <= SORT_BOX_NUM) {
                s_latest_rx_id = s_pending_box_id;
                s_latest_rx_id_dirty = true;
            }
        }
        s_rx_state = RX_WAIT_AA;
        break;

    default:
        s_rx_state = RX_WAIT_AA;
        break;
    }
}

void Sorting_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    s_rx_state = RX_WAIT_AA;
    memset(s_tasks, 0, sizeof(s_tasks));
    memset(s_servo_actions, 0, sizeof(s_servo_actions));
    memset(s_box_counts, 0, sizeof(s_box_counts));
    s_latest_rx_id = 0;
    s_latest_rx_id_dirty = false;

    update_processing_component_id("--");
    update_pending_queue("Queue: --");

    if (s_huart != NULL) {
        HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1U);
    }
}

void Sorting_OnUartRxCplt(UART_HandleTypeDef *huart)
{
    if (huart != s_huart) {
        return;
    }

    parse_byte(s_rx_byte);
    HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1U);
}

void Sorting_Service(void)
{
    uint32_t now = HAL_GetTick();

    publish_latest_rx_id_to_ui();

    for (uint8_t i = 0; i < SORT_BOX_NUM; i++) {
        if (!s_servo_actions[i].active || !tick_reached(now, s_servo_actions[i].next_tick)) {
            continue;
        }

        if (s_servo_actions[i].phase == SERVO_PHASE_CW) {
            Servo_SetSpeed((Servo_ID)i, PUSH_CCW_DIR, PUSH_SPEED_PCT);
            s_servo_actions[i].phase = SERVO_PHASE_CCW;
            s_servo_actions[i].next_tick = now + PUSH_CCW_TIME_MS;
        } else {
            Servo_Stop((Servo_ID)i);
            s_servo_actions[i].active = false;
            s_servo_actions[i].phase = SERVO_PHASE_IDLE;
        }
    }

    for (uint32_t i = 0; i < SORT_TASK_MAX; i++) {
        if (!s_tasks[i].is_active || !tick_reached(now, s_tasks[i].trigger_tick)) {
            continue;
        }

        Servo_ID servo_id;
        if (box_id_to_servo(s_tasks[i].target_box_id, &servo_id)) {
            if (s_servo_actions[servo_id].active) {
                continue;
            }

            Servo_SetSpeed(servo_id, PUSH_CW_DIR, PUSH_SPEED_PCT);
            s_servo_actions[servo_id].active = true;
            s_servo_actions[servo_id].phase = SERVO_PHASE_CW;
            s_servo_actions[servo_id].next_tick = now + PUSH_CW_TIME_MS;

            uint8_t box_id = s_tasks[i].target_box_id;
            uint8_t box_idx = (uint8_t)(box_id - 1U);
            s_box_counts[box_idx]++;
            update_box_count(box_id, s_box_counts[box_idx]);
        }

        s_tasks[i].is_active = false;
        publish_pending_queue_to_ui();
        publish_current_id_from_queue();
    }
}
