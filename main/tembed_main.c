#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "math.h"
#include "iot_button.h"
#include "iot_knob.h"
#include "tembed.h"
#include "apa102.h"
#include "tembed_lvgl.h"

#define TAG "tembed"

// ... [LED code remains unchanged] ...

#define LABEL_COUNT 3

typedef struct {
    char *name;                // Label name
    uint32_t total_time_ms;    // Total time in milliseconds
    uint32_t current_time_ms;  // Current session time in milliseconds
    bool timer_running;        // Is the timer running?
    lv_obj_t *lv_label;
    uint32_t color;   
} label_info_t;

// Initialize labels
label_info_t labels[LABEL_COUNT] = {
    {"RED", 0, 0, false, NULL, 0xFF0000},
    {"GREEN", 0, 0, false, NULL, 0x00FF00},
    {"BLUE", 0, 0, false, NULL, 0x0000FF},
};

int selected_label_index = 0;  // Currently selected label index

lv_obj_t *info_label;
lv_timer_t *timer;
lv_disp_t *lvgl_disp;

void update_selected_label_visuals()
{
    for (int i = 0; i < LABEL_COUNT; i++) {
        if (i == selected_label_index) {
            // Highlight the selected label
            lv_obj_set_style_outline_width(labels[i].lv_label, 2, LV_PART_MAIN);
            lv_obj_set_style_outline_color(labels[i].lv_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_outline_opa(labels[i].lv_label, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_bg_color(lv_disp_get_scr_act(lvgl_disp), lv_color_hex(labels[i].color), LV_PART_MAIN);

        } else {
            // Remove highlight from other labels
            lv_obj_set_style_outline_width(labels[i].lv_label, 0, LV_PART_MAIN);
        }
    }
}

void update_label_info_display()
{
    label_info_t *label = &labels[selected_label_index];
    uint32_t total_time_sec = label->total_time_ms / 1000;
    uint32_t current_time_sec = label->current_time_ms / 1000;
    lv_label_set_text_fmt(info_label, "%s\nTotal Time: %d s\nCurrent Time: %d s",
                          label->name, total_time_sec, current_time_sec);
}

void update_label_positions()
{
    int radius = 75;
    for (int i = 0; i < LABEL_COUNT; i++) {
        int index = (i - selected_label_index + LABEL_COUNT) % LABEL_COUNT;
        float angle = (2 * M_PI / LABEL_COUNT) * index;
        int x = radius * cos(angle)-75;
        int y = radius * sin(angle);
        lv_obj_align(labels[i].lv_label, LV_ALIGN_CENTER, x, y);
        //lv_obj_set_size(labels[i].lv_label, 60, 20); 
        //lv_obj_center(labels[i].lv_label);
    }
}


static void knob_left_cb(void *arg, void *data)
{
    // Decrease selected_label_index with wrap-around
    selected_label_index = (selected_label_index - 1 + LABEL_COUNT) % LABEL_COUNT;
    ESP_LOGI(TAG, "KNOB: KNOB_LEFT Selected Label Index is %d", selected_label_index);
    update_selected_label_visuals();
    update_label_info_display();
    update_label_positions();

}

static void knob_right_cb(void *arg, void *data)
{
    // Increase selected_label_index with wrap-around
    selected_label_index = (selected_label_index + 1) % LABEL_COUNT;
    ESP_LOGI(TAG, "KNOB: KNOB_RIGHT Selected Label Index is %d", selected_label_index);
    update_selected_label_visuals();
    update_label_info_display();
    update_label_positions();

}

static void button_press_down_cb(void *arg, void *data) {
    ESP_LOGI(TAG, "Button Pressed Down!");
    label_info_t *label = &labels[selected_label_index];

    if (label->timer_running) {
        // Stop the timer for this label
        label->timer_running = false;
        label->total_time_ms += label->current_time_ms;
        label->current_time_ms = 0;
        ESP_LOGI(TAG, "Stopped timer for label %s", label->name);
    } else {
        // Stop any other running timers
        for (int i = 0; i < LABEL_COUNT; i++) {
            if (labels[i].timer_running) {
                labels[i].timer_running = false;
                labels[i].total_time_ms += labels[i].current_time_ms;
                labels[i].current_time_ms = 0;
                break;
            }
        }
        // Start the timer for the selected label
        label->timer_running = true;
        label->current_time_ms = 0;
        ESP_LOGI(TAG, "Started timer for label %s", label->name);
    }

    update_label_info_display();
}

void timer_callback(lv_timer_t * timer)
{
    for (int i = 0; i < LABEL_COUNT; i++) {
        if (labels[i].timer_running) {
            labels[i].current_time_ms += 100;  // Update every 100 ms
            if (i == selected_label_index) {
                update_label_info_display();
            }
        }
    }
}

void lvgl_demo_ui(lv_disp_t *disp) {
    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_disp_get_scr_act(disp), lv_color_hex(0x00FF00), LV_PART_MAIN);

    // static lv_style_t l_style;
    // lv_style_init(&l_style);
    // lv_style_set_width(&l_style, 75);
    // lv_style_set_height(&l_style, 40);
    // lv_style_set_text_color(&l_style, lv_color_white());
    // lv_style_set_text_align(&l_style, LV_TEXT_ALIGN_CENTER);
    // lv_style_set_bg_opa(&l_style, LV_OPA_COVER);

    // /*Create a container with ROW flex direction*/
    // lv_obj_t * cont_row = lv_obj_create(lv_disp_get_scr_act(disp));
    // lv_obj_set_size(cont_row, 300, 75);
    // lv_obj_align(cont_row, LV_ALIGN_TOP_MID, 0, 5);
    // lv_obj_set_flex_flow(cont_row, LV_FLEX_FLOW_ROW);
    // lv_obj_set_style_bg_color(cont_row, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_t *circle_bg = lv_obj_create(lv_disp_get_scr_act(disp));
    lv_obj_set_size(circle_bg, 125, 125); // Adjust size as needed
    lv_obj_align(circle_bg, LV_ALIGN_CENTER, -75, 0);
    lv_obj_set_style_bg_color(circle_bg, lv_color_hex(0xCCCCCC), LV_PART_MAIN); // Light gray
    lv_obj_set_style_bg_opa(circle_bg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(circle_bg, 150, LV_PART_MAIN); // Make it circular

    static lv_style_t l_style;
    lv_style_init(&l_style);
    lv_style_set_width(&l_style, 75);
    lv_style_set_height(&l_style, 40);
    lv_style_set_text_color(&l_style, lv_color_white());
    lv_style_set_text_align(&l_style, LV_TEXT_ALIGN_CENTER);
    lv_style_set_bg_color(&l_style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_opa(&l_style, LV_OPA_COVER);
    lv_obj_t * label;

    /*Add items to the row*/
    for (int i = 0; i < LABEL_COUNT; i++) {
        label = lv_label_create(lv_disp_get_scr_act(disp));
        lv_label_set_text_static(label, labels[i].name);
        lv_obj_add_style(label, &l_style, LV_PART_MAIN);
        // Set background color based on label
        // if (i == 0) {
        //     lv_obj_set_style_bg_color(label, lv_color_hex(0xff0000), LV_PART_MAIN);
        // } else if (i == 1) {
        //     lv_obj_set_style_bg_color(label, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
        // } else if (i == 2) {
        //     lv_obj_set_style_bg_color(label, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
        // }
        lv_obj_set_style_bg_color(label, lv_color_hex(labels[i].color), LV_PART_MAIN);
        // float angle = (2 * M_PI / LABEL_COUNT) * i; // Evenly distribute around the circle
        // int x = 75 * cos(angle) -75;
        // int y = 75 * sin(angle);
        // lv_obj_align(label, LV_ALIGN_CENTER, x, y);
        lv_obj_set_size(label, 60, 20); 
        // lv_obj_center(label);
        labels[i].lv_label = label;
    }
        update_label_positions();

    // Create a label to display the current title and times
    info_label = lv_label_create(lv_disp_get_scr_act(disp));
    lv_label_set_text(info_label, "");
    lv_obj_set_style_text_color(info_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 50, -10);
    lv_obj_set_style_bg_color(info_label, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(info_label, LV_OPA_COVER, LV_PART_MAIN);

    update_selected_label_visuals();
    update_label_info_display();

    // Create a timer to update the running timer's current time
    timer = lv_timer_create(timer_callback, 100, NULL);  // Call every 100 ms
}

void app_main(void)
{
    ESP_LOGI(TAG,"Hello lcd!");

    // ... [Existing initialization code remains unchanged] ...

    // Initialize the T-Embed
    tembed_t tembed = tembed_init(notify_lvgl_flush_ready, &lvgl_disp_drv);

    // leds(tembed);

    iot_button_register_cb(tembed->dial.btn, BUTTON_PRESS_DOWN, button_press_down_cb, NULL);

    iot_knob_register_cb(tembed->dial.knob, KNOB_LEFT, knob_left_cb, NULL);
    iot_knob_register_cb(tembed->dial.knob, KNOB_RIGHT, knob_right_cb, NULL);

    lvgl_disp = tembed_lvgl_init(tembed);

    ESP_LOGI(TAG, "Display LVGL");
    lvgl_demo_ui(lvgl_disp);

    while (1) {
        // LVGL timer handler
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_timer_handler();
    }
}
