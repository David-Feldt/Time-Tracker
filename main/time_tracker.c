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
#include <stdarg.h>

#define TAG "tembed"
#define POWER_ON_GPIO 46

// Forward declarations for dialog callbacks
static void dialog_yes_cb(lv_event_t *e);
static void dialog_no_cb(lv_event_t *e);

// Forward declarations for dialog functions
void update_dialog_selection();
void trigger_dialog_action();
void show_confirmation_dialog(const char *format, ...);

// Forward declarations for UI update functions
void update_label_info_display();
void update_time_panel();
void update_selected_label_visuals();
void update_label_positions();

// Track which dialog button is selected (0 = Yes, 1 = No)
int dialog_selected_button = 0;
lv_obj_t *dialog_yes_btn;
lv_obj_t *dialog_no_btn;

// Add with other global variables
static lv_timer_t *timer;
static lv_timer_t *bg_refresh_timer;

// Add this function prototype before lvgl_demo_ui
static void bg_refresh_timer_cb(lv_timer_t *timer);

// Add these to store direct references
static lv_obj_t *main_container;
static lv_obj_t *left_panel;
static lv_obj_t *right_panel;

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
    {"Work", 0, 0, false, NULL, 0x444444},
    {"Study", 0, 0, false, NULL, 0x444444},
    {"Exercise", 0, 0, false, NULL, 0x444444},
    {"Reading", 0, 0, false, NULL, 0x444444},
    {"Break", 0, 0, false, NULL, 0x444444},
};

int selected_label_index = 0;  // Currently selected label index
dialog_state_t current_dialog = DIALOG_NONE;
int running_label_index = -1;  // Track which label is currently running

lv_obj_t *info_label;
lv_obj_t *dialog_box;
lv_obj_t *active_task_label;
lv_disp_t *lvgl_disp;

void update_selected_label_visuals()
{
    for (int i = 0; i < LABEL_COUNT; i++) {
        // First reset all to default state
        lv_obj_set_style_bg_color(labels[i].lv_label, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_text_color(labels[i].lv_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        
        // Then apply special styling
        if (i == selected_label_index) {
            // Selected label gets white background with black text
            lv_obj_set_style_bg_color(labels[i].lv_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_text_color(labels[i].lv_label, lv_color_hex(0x000000), LV_PART_MAIN);
        } else if (i == running_label_index) {
            // Running task gets light gray
            lv_obj_set_style_bg_color(labels[i].lv_label, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
            lv_obj_set_style_text_color(labels[i].lv_label, lv_color_hex(0x000000), LV_PART_MAIN);
        }
    }
}

void update_label_positions()
{
    int item_height = 40;  // Height of each item in the list
    int vertical_spacing = 10;  // Space between items
    int total_item_height = item_height + vertical_spacing;
    int middle_y = 85;  // Middle of the screen vertically (170/2)
    
    for (int i = 0; i < LABEL_COUNT; i++) {
        // Calculate index relative to the selected item
        int relative_idx = i - selected_label_index;
        
        // Handle wrap-around for infinite scroll effect
        if (relative_idx < -(LABEL_COUNT/2)) relative_idx += LABEL_COUNT;
        if (relative_idx > LABEL_COUNT/2) relative_idx -= LABEL_COUNT;
        
        // Position vertically with the selected item in the middle
        int y = middle_y + (relative_idx * total_item_height);
        
        // Position all items on the right panel, properly right-aligned
        lv_obj_align(labels[i].lv_label, LV_ALIGN_RIGHT_MID, 0, y - middle_y);
        
        // Resize labels to use full width
        lv_obj_set_size(labels[i].lv_label, 160, item_height); 
        
        // Update the label text to include total minutes spent
        uint32_t total_mins = labels[i].total_time_ms / (60 * 1000);
        lv_label_set_text_fmt(labels[i].lv_label, "%s [%dm]", labels[i].name, total_mins);
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
    update_label_positions();
}

// Create and show a confirmation dialog
void show_confirmation_dialog(const char *format, ...) {
    if (dialog_box != NULL) {
        lv_obj_del(dialog_box);
    }
    
    dialog_selected_button = 0; // Default select "Yes"
    
    // Create dialog directly on main container, not screen
    dialog_box = lv_obj_create(main_container);
    lv_obj_set_size(dialog_box, 200, 120);
    lv_obj_align(dialog_box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(dialog_box, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dialog_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(dialog_box, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(dialog_box, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    
    // Format the message with variable arguments
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    lv_obj_t *msg_label = lv_label_create(dialog_box);
    lv_label_set_text(msg_label, buffer);
    lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 10);
    
    dialog_yes_btn = lv_btn_create(dialog_box);
    lv_obj_set_size(dialog_yes_btn, 70, 40);
    lv_obj_align(dialog_yes_btn, LV_ALIGN_BOTTOM_LEFT, 20, -10);
    lv_obj_add_event_cb(dialog_yes_btn, dialog_yes_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(dialog_yes_btn, lv_color_hex(0x444444), LV_PART_MAIN);
    
    lv_obj_t *yes_label = lv_label_create(dialog_yes_btn);
    lv_label_set_text(yes_label, "Yes");
    lv_obj_center(yes_label);
    
    dialog_no_btn = lv_btn_create(dialog_box);
    lv_obj_set_size(dialog_no_btn, 70, 40);
    lv_obj_align(dialog_no_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
    lv_obj_add_event_cb(dialog_no_btn, dialog_no_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(dialog_no_btn, lv_color_hex(0x444444), LV_PART_MAIN);
    
    lv_obj_t *no_label = lv_label_create(dialog_no_btn);
    lv_label_set_text(no_label, "No");
    lv_obj_center(no_label);
    
    // Set initial selection highlight
    update_dialog_selection();
}

// Update the visual selection in the dialog
void update_dialog_selection() {
    if (dialog_box == NULL) return;
    
    // Reset both buttons to default state (dark gray background with white text)
    lv_obj_set_style_bg_color(dialog_yes_btn, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_bg_color(dialog_no_btn, lv_color_hex(0x444444), LV_PART_MAIN);
    
    // Get references to the label objects
    lv_obj_t *yes_label = lv_obj_get_child(dialog_yes_btn, 0);
    lv_obj_t *no_label = lv_obj_get_child(dialog_no_btn, 0);
    
    // Set default text color for both buttons to white
    lv_obj_set_style_text_color(yes_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_color(no_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    
    // Highlight the selected button with white background and black text
    if (dialog_selected_button == 0) {
        // Yes button selected
       lv_obj_set_style_bg_color(dialog_no_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_color(no_label, lv_color_hex(0x000000), LV_PART_MAIN);
    } else {
        // No button selected
        lv_obj_set_style_bg_color(dialog_yes_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_color(yes_label, lv_color_hex(0x000000), LV_PART_MAIN);
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
        
        // Update the active task display (gray instead of color)
        lv_label_set_text_fmt(active_task_label, "Active: %s", label->name);
        lv_obj_set_style_bg_color(active_task_label, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
        lv_obj_set_style_text_color(active_task_label, lv_color_hex(0x000000), LV_PART_MAIN);
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
        lv_obj_set_style_bg_color(active_task_label, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_text_color(active_task_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    }
    
    // Close the dialog - ensure this happens in all cases
    if (dialog_box != NULL) {
        lv_obj_del(dialog_box);
        dialog_box = NULL;
    }
    current_dialog = DIALOG_NONE;
    
    // Update visuals
    update_selected_label_visuals();
    update_label_positions();
    update_time_panel();
}

// Process dialog "No" response
static void dialog_no_cb(lv_event_t *e) {
    // Just close the dialog without taking action
    if (dialog_box != NULL) {
        lv_obj_del(dialog_box);
        dialog_box = NULL;
    }
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
        show_confirmation_dialog("Stop %s?", label->name);
    } else {
        // Show start confirmation dialog
        current_dialog = DIALOG_START_TASK;
        show_confirmation_dialog("Start %s?", label->name);
    }
}

void timer_callback(lv_timer_t * timer)
{
    for (int i = 0; i < LABEL_COUNT; i++) {
        if (labels[i].timer_running) {
            labels[i].current_time_ms += 100;  // Update every 100 ms
            
            // Always update the time panel when any task is running
            update_time_panel();
            
            // Update the minute count in the label text
            uint32_t total_mins = labels[i].total_time_ms / (60 * 1000);
            uint32_t current_mins = labels[i].current_time_ms / (60 * 1000);
            lv_label_set_text_fmt(labels[i].lv_label, "%s [%dm]", 
                                 labels[i].name, total_mins + current_mins);
        }
    }
}

// Update the time panel to always show the active task
void update_time_panel() {
    // If no task is running, show a message
    if (running_label_index < 0) {
        lv_label_set_text(info_label, "No Active Session");
        return;
    }
    
    // Always show the running task's time, regardless of selection
    label_info_t *active_label = &labels[running_label_index];
    uint32_t current_time_sec = active_label->current_time_ms / 1000;
    
    // Calculate hours, minutes, seconds for better readability
    uint32_t current_hours = current_time_sec / 3600;
    uint32_t current_mins = (current_time_sec % 3600) / 60;
    uint32_t current_secs = current_time_sec % 60;
    
    lv_label_set_text_fmt(info_label, "%s\n%02d:%02d:%02d", 
                         active_label->name,
                         current_hours, current_mins, current_secs);
}

// Update the background refresh timer to explicitly set opacity
static void bg_refresh_timer_cb(lv_timer_t *timer) {
    ESP_LOGI(TAG, "Refreshing background colors");
    
    // Force update screen background to pure black
    lv_obj_set_style_bg_color(lv_disp_get_scr_act(lvgl_disp), lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_disp_get_scr_act(lvgl_disp), LV_OPA_COVER, LV_PART_MAIN);
    
    // Brute force approach - reset ALL objects' backgrounds
    uint32_t child_cnt = lv_obj_get_child_cnt(lv_disp_get_scr_act(lvgl_disp));
    for(uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(lv_disp_get_scr_act(lvgl_disp), i);
        // Set all panel backgrounds to black
        lv_obj_set_style_bg_color(child, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(child, LV_OPA_COVER, LV_PART_MAIN);
    }
    
    // Force update all labels
    for (int i = 0; i < LABEL_COUNT; i++) {
        if (i == running_label_index) {
            lv_obj_set_style_bg_color(labels[i].lv_label, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(labels[i].lv_label, lv_color_hex(0x333333), LV_PART_MAIN);
        }
        lv_obj_set_style_bg_opa(labels[i].lv_label, LV_OPA_COVER, LV_PART_MAIN);
    }
    
    // Force update active task label
    if (running_label_index >= 0) {
        lv_obj_set_style_bg_color(active_task_label, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(active_task_label, lv_color_hex(0x333333), LV_PART_MAIN);
    }
    lv_obj_set_style_bg_opa(active_task_label, LV_OPA_COVER, LV_PART_MAIN);
}

void lvgl_demo_ui(lv_disp_t *disp) {
    // Store display for later reference
    lvgl_disp = disp;
    
    // First, disable any built-in themes completely
    lv_disp_set_theme(disp, NULL);
    
    // Get screen and set it to pure black
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    
    // Create a single main container that covers the whole screen
    main_container = lv_obj_create(scr);
    lv_obj_set_size(main_container, 320, 170);
    lv_obj_align(main_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(main_container, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(main_container, 0, LV_PART_MAIN);
    
    // Create left panel (for active task and time display)
    left_panel = lv_obj_create(main_container);
    lv_obj_set_size(left_panel, 160, 170);
    lv_obj_align(left_panel, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(left_panel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(left_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(left_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(left_panel, 0, LV_PART_MAIN);
    
    // Create right panel (for tasks)
    right_panel = lv_obj_create(main_container);
    lv_obj_set_size(right_panel, 160, 170);
    lv_obj_align(right_panel, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(right_panel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(right_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(right_panel, 0, LV_PART_MAIN);
    
    // Create active task label directly on left panel 
    active_task_label = lv_label_create(left_panel);
    lv_obj_set_size(active_task_label, 140, 40);
    lv_label_set_text(active_task_label, "No Active Task");
    lv_obj_set_style_text_color(active_task_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(active_task_label, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(active_task_label, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(active_task_label, 5, LV_PART_MAIN);
    lv_obj_align(active_task_label, LV_ALIGN_TOP_MID, 0, 15);
    
    // Create time info label
    info_label = lv_label_create(left_panel);
    lv_obj_set_size(info_label, 140, 80);
    lv_label_set_text(info_label, "");
    lv_obj_set_style_text_color(info_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -15);
    
    // Create task labels directly on right panel
    for (int i = 0; i < LABEL_COUNT; i++) {
        // Force color to normal gray in the struct
        labels[i].color = 0x333333;
        
        // Create the label object directly - no style inheritance
        lv_obj_t *label = lv_label_create(right_panel);
        lv_obj_set_size(label, 160, 30);  // Full width of right panel
        
        // Set very explicit styles
        lv_obj_set_style_bg_color(label, lv_color_hex(0x333333), LV_PART_MAIN);  
        lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_radius(label, 5, LV_PART_MAIN);
        
        // Add initial text
        lv_label_set_text_fmt(label, "%s [0m]", labels[i].name);
        
        // Store for later reference
        labels[i].lv_label = label;
        
        ESP_LOGI(TAG, "Created label %d with explicit gray bg (0x333333)", i);
    }
    
    // Update positions and visuals
    update_label_positions();
    update_selected_label_visuals();
    update_time_panel();
    
    // Create the timer for updating the running timer display
    timer = lv_timer_create(timer_callback, 100, NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG,"Hello lcd!");

    // Initialize the T-Embed
    tembed_t tembed = tembed_init(notify_lvgl_flush_ready, &lvgl_disp_drv);

    // Register button and knob callbacks
    iot_button_register_cb(tembed->dial.btn, BUTTON_PRESS_DOWN, button_press_down_cb, NULL);
    iot_knob_register_cb(tembed->dial.knob, KNOB_LEFT, knob_left_cb, NULL);
    iot_knob_register_cb(tembed->dial.knob, KNOB_RIGHT, knob_right_cb, NULL);

    // Initialize LVGL
    lvgl_disp = tembed_lvgl_init(tembed);
    
    // DISABLE ANY THEMES
    lv_disp_set_theme(lvgl_disp, NULL);

    ESP_LOGI(TAG, "Display LVGL");
    lvgl_demo_ui(lvgl_disp);

    while (1) {
        // LVGL timer handler
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_timer_handler();
    }
}

// Replace the existing update_label_info_display with a call to update_time_panel
void update_label_info_display()
{
    update_time_panel();
}
