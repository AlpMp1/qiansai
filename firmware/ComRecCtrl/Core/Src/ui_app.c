#include "ui_app.h"

#include <stdio.h>
#include <string.h>

#define BOX_NUM 5

static lv_obj_t *s_splash_scr = NULL;
static lv_obj_t *s_main_scr = NULL;

static lv_obj_t *s_logo_label = NULL;
static lv_obj_t *s_progress_bar = NULL;

static lv_obj_t *s_status_dot = NULL;
static lv_obj_t *s_status_text = NULL;
static lv_obj_t *s_queue_label = NULL;
static lv_obj_t *s_belt_obj = NULL;
static lv_obj_t *s_belt_queue_label = NULL;
static lv_obj_t *s_box_cnt_label[BOX_NUM] = {0};

static lv_timer_t *s_splash_timer = NULL;
static lv_timer_t *s_queue_timer = NULL;

static uint8_t s_splash_progress = 0;
static uint16_t s_box_count_cache[BOX_NUM] = {0};
static bool s_conveyor_running = false;
static char s_current_id[24] = "--";
static char s_pending_queue[64] = "Queue: --";
static uint8_t s_dot_phase = 0;

static void logo_text_opa_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_text_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void refresh_queue_text(void)
{
    if (s_queue_label == NULL) {
        return;
    }

    char dots[4] = {0};
    if (s_conveyor_running) {
        for (uint8_t i = 0; i < s_dot_phase; i++) {
            dots[i] = '.';
        }
        lv_label_set_text_fmt(s_queue_label, "Processing Box: %s %s", s_current_id, dots);
    } else {
        lv_label_set_text_fmt(s_queue_label, "Processing Box: %s (Paused)", s_current_id);
    }
}

static void refresh_pending_queue_text(void)
{
    if (s_belt_queue_label == NULL) {
        return;
    }
    lv_label_set_text(s_belt_queue_label, s_pending_queue);
}

static void queue_anim_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    s_dot_phase = (s_dot_phase + 1) & 0x03;
    refresh_queue_text();
}

static void apply_status_style(void)
{
    if (s_status_dot == NULL || s_status_text == NULL || s_belt_obj == NULL) {
        return;
    }

    if (s_conveyor_running) {
        lv_obj_set_style_bg_color(s_status_dot, lv_color_hex(0x2ecc71), 0);
        lv_label_set_text(s_status_text, "RUNNING");
        lv_obj_set_style_bg_color(s_belt_obj, lv_color_hex(0x5d6d7e), 0);
    } else {
        lv_obj_set_style_bg_color(s_status_dot, lv_color_hex(0xe74c3c), 0);
        lv_label_set_text(s_status_text, "STOPPED");
        lv_obj_set_style_bg_color(s_belt_obj, lv_color_hex(0x4b5563), 0);
    }
}

static void create_main_dashboard(void)
{
    s_main_scr = lv_obj_create(NULL);
    lv_obj_clear_flag(s_main_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_main_scr, lv_color_hex(0x0f172a), 0);
    lv_obj_set_style_bg_opa(s_main_scr, LV_OPA_COVER, 0);

    lv_obj_t *top = lv_obj_create(s_main_scr);
    lv_obj_set_size(top, lv_pct(94), 52);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(top, 8, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_bg_color(top, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_pad_all(top, 4, 0);

    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(top);
    lv_label_set_text(title, "Component Recognition System");
    lv_obj_set_style_text_color(title, lv_color_hex(0xecf0f1), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    lv_obj_t *status_wrap = lv_obj_create(top);
    lv_obj_set_size(status_wrap, 116, 18);
    lv_obj_set_style_bg_opa(status_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_wrap, 0, 0);
    lv_obj_set_style_pad_all(status_wrap, 0, 0);

    lv_obj_set_flex_flow(status_wrap, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(status_wrap, 6, 0);

    s_status_dot = lv_obj_create(status_wrap);
    lv_obj_set_size(s_status_dot, 8, 8);
    lv_obj_set_style_radius(s_status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_status_dot, 0, 0);

    s_status_text = lv_label_create(status_wrap);
    lv_obj_set_style_text_color(s_status_text, lv_color_hex(0xcbd5e1), 0);
    lv_obj_set_style_text_font(s_status_text, &lv_font_montserrat_12, 0);

    lv_obj_t *conveyor_card = lv_obj_create(s_main_scr);
    lv_obj_set_size(conveyor_card, lv_pct(94), 74);
    lv_obj_align(conveyor_card, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_clear_flag(conveyor_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(conveyor_card, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_border_width(conveyor_card, 0, 0);
    lv_obj_set_style_radius(conveyor_card, 8, 0);
    lv_obj_set_style_pad_all(conveyor_card, 8, 0);

    s_queue_label = lv_label_create(conveyor_card);
    lv_obj_align(s_queue_label, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_text_color(s_queue_label, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(s_queue_label, &lv_font_montserrat_12, 0);

    s_belt_obj = lv_obj_create(conveyor_card);
    lv_obj_set_size(s_belt_obj, lv_pct(92), 20);
    lv_obj_align(s_belt_obj, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_radius(s_belt_obj, 4, 0);
    lv_obj_set_style_border_width(s_belt_obj, 0, 0);

    s_belt_queue_label = lv_label_create(s_belt_obj);
    lv_obj_center(s_belt_queue_label);
    lv_obj_set_style_text_color(s_belt_queue_label, lv_color_hex(0xe2e8f0), 0);
    lv_obj_set_style_text_font(s_belt_queue_label, &lv_font_montserrat_12, 0);

    lv_obj_t *panel = lv_obj_create(s_main_scr);
    lv_obj_set_size(panel, lv_pct(94), 92);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 4, 0);
    lv_obj_set_style_pad_column(panel, 2, 0);

    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    for (uint8_t i = 0; i < BOX_NUM; i++) {
        lv_obj_t *col = lv_obj_create(panel);
        lv_obj_set_height(col, lv_pct(100));
        lv_obj_set_flex_grow(col, 1);
        lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(col, 0, 0);
        lv_obj_set_style_pad_all(col, 0, 0);
        lv_obj_set_style_pad_row(col, 2, 0);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *box_name = lv_label_create(col);
        lv_label_set_text_fmt(box_name, "Box %d", i + 1);
        lv_obj_set_style_text_color(box_name, lv_color_hex(0x94a3b8), 0);
        lv_obj_set_style_text_font(box_name, &lv_font_montserrat_12, 0);

        lv_obj_t *box_card = lv_obj_create(col);
        lv_obj_set_size(box_card, lv_pct(90), 62);
        lv_obj_set_style_radius(box_card, 6, 0);
        lv_obj_set_style_bg_color(box_card, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(box_card, 0, 0);
        lv_obj_clear_flag(box_card, LV_OBJ_FLAG_SCROLLABLE);

        s_box_cnt_label[i] = lv_label_create(box_card);
        lv_label_set_text_fmt(s_box_cnt_label[i], "%u", s_box_count_cache[i]);
        lv_obj_center(s_box_cnt_label[i]);
        lv_obj_set_style_text_color(s_box_cnt_label[i], lv_color_hex(0xf8fafc), 0);
        lv_obj_set_style_text_font(s_box_cnt_label[i], &lv_font_montserrat_16, 0);
    }

    apply_status_style();
    refresh_queue_text();
    refresh_pending_queue_text();

    if (s_queue_timer == NULL) {
        s_queue_timer = lv_timer_create(queue_anim_timer_cb, 450, NULL);
    }
}

static void splash_progress_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);

    if (s_progress_bar == NULL) {
        return;
    }

    if (s_splash_progress < 100U) {
        s_splash_progress = (uint8_t)(s_splash_progress + 4U);
        if (s_splash_progress > 100U) {
            s_splash_progress = 100U;
        }
        lv_bar_set_value(s_progress_bar, s_splash_progress, LV_ANIM_OFF);
    }

    if (s_splash_progress >= 100U) {
        if (s_splash_timer != NULL) {
            lv_timer_del(s_splash_timer);
            s_splash_timer = NULL;
        }

        if (s_main_scr == NULL) {
            create_main_dashboard();
        }

        lv_scr_load_anim(s_main_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 400, 80, true);
        s_splash_scr = NULL;
    }
}

static void create_splash_screen(void)
{
    s_splash_scr = lv_obj_create(NULL);
    lv_obj_clear_flag(s_splash_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_splash_scr, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_bg_opa(s_splash_scr, LV_OPA_COVER, 0);

    lv_obj_t *logo_card = lv_obj_create(s_splash_scr);
    lv_obj_set_size(logo_card, lv_pct(88), 72);
    lv_obj_align(logo_card, LV_ALIGN_CENTER, 0, -24);
    lv_obj_set_style_bg_color(logo_card, lv_color_hex(0x34495e), 0);
    lv_obj_set_style_border_width(logo_card, 0, 0);
    lv_obj_set_style_radius(logo_card, 8, 0);
    lv_obj_clear_flag(logo_card, LV_OBJ_FLAG_SCROLLABLE);

    s_logo_label = lv_label_create(logo_card);
    lv_label_set_text(s_logo_label, "COMPONENT RECOGNITION");
    lv_obj_center(s_logo_label);
    lv_obj_set_style_text_color(s_logo_label, lv_color_hex(0xecf0f1), 0);
    lv_obj_set_style_text_opa(s_logo_label, LV_OPA_TRANSP, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_logo_label);
    lv_anim_set_exec_cb(&a, logo_text_opa_anim_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 900);
    lv_anim_set_delay(&a, 120);
    lv_anim_start(&a);

    s_progress_bar = lv_bar_create(s_splash_scr);
    lv_obj_set_size(s_progress_bar, lv_pct(74), 10);
    lv_obj_align(s_progress_bar, LV_ALIGN_CENTER, 0, 34);
    lv_bar_set_range(s_progress_bar, 0, 100);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);

    lv_scr_load(s_splash_scr);

    s_splash_progress = 0;
    s_splash_timer = lv_timer_create(splash_progress_timer_cb, 60, NULL);
}

void ui_app_init(void)
{
    create_splash_screen();
}

void update_box_count(uint8_t id, uint16_t count)
{
    if (id < 1U || id > BOX_NUM) {
        return;
    }

    uint8_t idx = (uint8_t)(id - 1U);
    s_box_count_cache[idx] = count;

    if (s_box_cnt_label[idx] != NULL) {
        lv_label_set_text_fmt(s_box_cnt_label[idx], "%u", count);
    }
}

void update_conveyor_status(bool is_running)
{
    s_conveyor_running = is_running;
    apply_status_style();
    refresh_queue_text();
}

void update_processing_component_id(const char *id_str)
{
    if (id_str == NULL || id_str[0] == '\0') {
        strcpy(s_current_id, "--");
    } else {
        strncpy(s_current_id, id_str, sizeof(s_current_id) - 1U);
        s_current_id[sizeof(s_current_id) - 1U] = '\0';
    }
    refresh_queue_text();
}

void update_pending_queue(const char *queue_str)
{
    if (queue_str == NULL || queue_str[0] == '\0') {
        strcpy(s_pending_queue, "Queue: --");
    } else {
        strncpy(s_pending_queue, queue_str, sizeof(s_pending_queue) - 1U);
        s_pending_queue[sizeof(s_pending_queue) - 1U] = '\0';
    }
    refresh_pending_queue_text();
}
