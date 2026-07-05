#ifndef __SORTING_TASK_H
#define __SORTING_TASK_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t target_box_id;
    uint32_t trigger_tick;
    bool is_active;
} SortingTask;

void Sorting_Init(UART_HandleTypeDef *huart);
void Sorting_Service(void);
void Sorting_OnUartRxCplt(UART_HandleTypeDef *huart);

#endif
