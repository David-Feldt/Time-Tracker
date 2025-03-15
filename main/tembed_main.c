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
#define POWER_ON_GPIO 46

// Forward declarations for dialog callbacks
static void dialog_yes_cb(lv_event_t *e);
static void dialog_no_cb(lv_event_t *e);

// Forward declarations for dialog functions
void update_dialog_selection();
void trigger_dialog_action();
void show_confirmation_dialog(const char *message);

// Track which dialog button is selected (0 = Yes, 1 = No)
int dialog_selected_button = 0;
lv_obj_t *dialog_yes_btn;
lv_obj_t *dialog_no_btn;

// ... [LED code remains unchanged] ...

void turn_off_device() {
    // Set the GPIO pin as output
    gpio_set_direction(POWER_ON_GPIO, GPIO_MODE_OUTPUT);
    
    // Set the GPIO pin low to turn off the device
    gpio_set_level(POWER_ON_GPIO, 0);
}

// Add confirmation dialog state
typedef enum {
    DIALOG_NONE,
    DIALOG_START_TASK,
    DIALOG_STOP_TASK
} dialog_state_t;

typedef struct {
    char *name;                // Label name
    uint32_t total_time_ms;    // Total time in milliseconds
    uint32_t current_time_ms;  // Current session time in milliseconds
    bool timer_running;        // Is the timer running?
    lv_obj_t *lv_label;
    uint32_t color;   
} label_info_t;

// Increase number of labels for more activities
#define LABEL_COUNT 5

// Initialize labels with more meaningful activity names
label_info_t labels[LABEL_COUNT] = {
    {"Work", 0, 0, false, NULL, 0xFF0000},
    {"Study", 0, 0, false, NULL, 0x00FF00},
    {"Exercise", 0, 0, false, NULL, 0x0000FF},
    {"Reading", 0, 0, false, NULL, 0xFFAA00},
    {"Break", 0, 0, false, NULL, 0xAA00FF},
};

int selected_label_index = 0;  // Currently selected label index
dialog_state_t current_dialog = DIALOG_NONE;
int running_label_index = -1;  // Track which label is currently running

lv_obj_t *info_label;
lv_obj_t *dialog_box;
lv_obj_t *active_task_label;
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
    
    // Calculate hours, minutes, seconds for better readability
    uint32_t total_hours = total_time_sec / 3600;
    uint32_t total_mins = (total_time_sec % 3600) / 60;
    uint32_t total_secs = total_time_sec % 60;
    
    uint32_t current_hours = current_time_sec / 3600;
    uint32_t current_mins = (current_time_sec % 3600) / 60;
    uint32_t current_secs = current_time_sec % 60;
    
    lv_label_set_text_fmt(info_label, "%s\nTotal: %02d:%02d:%02d\nCurrent: %02d:%02d:%02d",
                          label->name, 
                          total_hours, total_mins, total_secs,
                          current_hours, current_mins, current_secs);
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
    if (dialog_box != NULL) {
        // If dialog is active, change selection
        dialog_selected_button = 0; // Select "Yes"
        update_dialog_selection();
        return;
    }
    
    // Existing code for when no dialog is active
    selected_label_index = (selected_label_index - 1 + LABEL_COUNT) % LABEL_COUNT;
    ESP_LOGI(TAG, "KNOB: KNOB_LEFT Selected Label Index is %d", selected_label_index);
    update_selected_label_visuals();
    update_label_info_display();
    update_label_positions();
}

static void knob_right_cb(void *arg, void *data)
{
    if (dialog_box != NULL) {
        // If dialog is active, change selection
        dialog_selected_button = 1; // Select "No"
        update_dialog_selection();
        return;
    }
    
    // Existing code for when no dialog is active
    selected_label_index = (selected_label_index + 1) % LABEL_COUNT;
    ESP_LOGI(TAG, "KNOB: KNOB_RIGHT Selected Label Index is %d", selected_label_index);
    update_selected_label_visuals();
    update_label_info_display();
    update_label_positions();
}

// Create and show a confirmation dialog
void show_confirmation_dialog(const char *message) {
    if (dialog_box != NULL) {
        lv_obj_del(dialog_box);
    }
    
    dialog_selected_button = 0; // Default select "Yes"
    
    dialog_box = lv_obj_create(lv_disp_get_scr_act(lvgl_disp));
    lv_obj_set_size(dialog_box, 200, 120);
    lv_obj_align(dialog_box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(dialog_box, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_border_width(dialog_box, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(dialog_box, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    
    lv_obj_t *msg_label = lv_label_create(dialog_box);
    lv_label_set_text(msg_label, message);
    lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 10);
    
    dialog_yes_btn = lv_btn_create(dialog_box);
    lv_obj_set_size(dialog_yes_btn, 70, 40);
    lv_obj_align(dialog_yes_btn, LV_ALIGN_BOTTOM_LEFT, 20, -10);
    lv_obj_add_event_cb(dialog_yes_btn, dialog_yes_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *yes_label = lv_label_create(dialog_yes_btn);
    lv_label_set_text(yes_label, "Yes");
    lv_obj_center(yes_label);
    
    dialog_no_btn = lv_btn_create(dialog_box);
    lv_obj_set_size(dialog_no_btn, 70, 40);
    lv_obj_align(dialog_no_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
    lv_obj_add_event_cb(dialog_no_btn, dialog_no_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *no_label = lv_label_create(dialog_no_btn);
    lv_label_set_text(no_label, "No");
    lv_obj_center(no_label);
    
    // Set initial selection highlight
    update_dialog_selection();
}

// Update the visual selection in the dialog
void update_dialog_selection() {
    if (dialog_box == NULL) return;
    
    // Reset both buttons to default state
    lv_obj_set_style_bg_color(dialog_yes_btn, lv_color_hex(0x2196F3), LV_PART_MAIN); // Default blue
    lv_obj_set_style_bg_color(dialog_no_btn, lv_color_hex(0x2196F3), LV_PART_MAIN); // Default blue
    
    // Highlight the selected button
    if (dialog_selected_button == 0) {
        lv_obj_set_style_bg_color(dialog_yes_btn, lv_color_hex(0x4CAF50), LV_PART_MAIN); // Green for selection
    } else {
        lv_obj_set_style_bg_color(dialog_no_btn, lv_color_hex(0x4CAF50), LV_PART_MAIN); // Green for selection
    }
}

// Handle dialog action based on current selection
void trigger_dialog_action() {
    if (dialog_box == NULL) return;
    
    if (dialog_selected_button == 0) {
        dialog_yes_cb(NULL); // Trigger Yes action
    } else {
        dialog_no_cb(NULL); // Trigger No action
    }
}

// Process dialog "Yes" response
static void dialog_yes_cb(lv_event_t *e) {
    if (current_dialog == DIALOG_START_TASK) {
        // Stop any currently running timer
        if (running_label_index >= 0) {
            label_info_t *prev_label = &labels[running_label_index];
            prev_label->timer_running = false;
            prev_label->total_time_ms += prev_label->current_time_ms;
            prev_label->current_time_ms = 0;
            ESP_LOGI(TAG, "Stopped timer for label %s", prev_label->name);
        }
        
        // Start the new timer
        label_info_t *label = &labels[selected_label_index];
        label->timer_running = true;
        label->current_time_ms = 0;
        running_label_index = selected_label_index;
        ESP_LOGI(TAG, "Started timer for label %s", label->name);
        
        // Update the active task display
        lv_label_set_text_fmt(active_task_label, "Active: %s", label->name);
        lv_obj_set_style_bg_color(active_task_label, lv_color_hex(label->color), LV_PART_MAIN);
    } 
    else if (current_dialog == DIALOG_STOP_TASK) {
        // Stop the current timer
        label_info_t *label = &labels[selected_label_index];
        label->timer_running = false;
        label->total_time_ms += label->current_time_ms;
        label->current_time_ms = 0;
        running_label_index = -1;
        ESP_LOGI(TAG, "Stopped timer for label %s", label->name);
        
        // Update the active task display
        lv_label_set_text(active_task_label, "No Active Task");
        lv_obj_set_style_bg_color(active_task_label, lv_color_hex(0x808080), LV_PART_MAIN);
    }
    
    // Close the dialog
    lv_obj_del(dialog_box);
    dialog_box = NULL;
    current_dialog = DIALOG_NONE;
    
    update_label_info_display();
}

// Process dialog "No" response
static void dialog_no_cb(lv_event_t *e) {
    // Just close the dialog without taking action
    lv_obj_del(dialog_box);
    dialog_box = NULL;
    current_dialog = DIALOG_NONE;
}

static void button_press_down_cb(void *arg, void *data) {
    ESP_LOGI(TAG, "Button Pressed Down!");
    
    // If dialog is open, trigger the selected action
    if (dialog_box != NULL) {
        trigger_dialog_action();
        return;
    }
    
    // Existing code for when no dialog is active
    label_info_t *label = &labels[selected_label_index];

    if (label->timer_running) {
        // Show stop confirmation dialog
        current_dialog = DIALOG_STOP_TASK;
        show_confirmation_dialog("Stop timer for this task?");
    } else {
        // Show start confirmation dialog
        current_dialog = DIALOG_START_TASK;
        show_confirmation_dialog("Start timer for this task?");
    }
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

    // Create a top panel for active task display
    lv_obj_t *top_panel = lv_obj_create(lv_disp_get_scr_act(disp));
    lv_obj_set_size(top_panel, 240, 40);
    lv_obj_align(top_panel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_panel, lv_color_hex(0x404040), LV_PART_MAIN);
    
    active_task_label = lv_label_create(top_panel);
    lv_label_set_text(active_task_label, "No Active Task");
    lv_obj_set_style_text_color(active_task_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(active_task_label);

    // Create task selection circle
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
        lv_obj_set_style_bg_color(label, lv_color_hex(labels[i].color), LV_PART_MAIN);
        lv_obj_set_size(label, 60, 20); 
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
    // turn_off_device();

    while (1) {
        // LVGL timer handler
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_timer_handler();
    }
}
