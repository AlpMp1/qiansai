#ifndef UI_APP_H
#define UI_APP_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_app_init(void);

void update_box_count(uint8_t id, uint16_t count);
void update_conveyor_status(bool is_running);
void update_processing_component_id(const char *id_str);
void update_pending_queue(const char *queue_str);

#ifdef __cplusplus
}
#endif

#endif /* UI_APP_H */
