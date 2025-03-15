// /*
//  * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
//  *
//  * SPDX-License-Identifier: CC0-1.0
//  */

// #include <stdio.h>
// #include "sdkconfig.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_chip_info.h"
// #include "esp_flash.h"
// #include "driver/gpio.h"
// #include "esp_err.h"
// #include "esp_log.h"
// #include "math.h"
// #include "iot_button.h"
// #include "iot_knob.h"
// #include "tembed.h"
// #include "apa102.h"
// #include "tembed_lvgl.h"

// #define TAG "tembed"
// #define POWER_ON_GPIO 46

// void turn_off_device() {
//     // Set the GPIO pin as output
//     gpio_set_direction(POWER_ON_GPIO, GPIO_MODE_OUTPUT);
    
//     // Set the GPIO pin low to turn off the device
//     gpio_set_level(POWER_ON_GPIO, 0);
// }

// // Set the number of LEDs to control.
// const uint16_t ledCount = CONFIG_APA102_LED_COUNT;

// // We define "power" in this sketch to be the product of the
// // 8-bit color channel value and the 5-bit brightness register.
// // The maximum possible power is 255 * 31 (7905).
// const uint16_t maxPower = 255 * 31;

// // The power we want to use on the first LED is 1, which
// // corresponds to the dimmest possible white.
// const uint16_t minPower = 1;

// // This function sends a white color with the specified power,
// // which should be between 0 and 7905.
// void sendWhite(const apa102_t *apa102,uint16_t power)
// {
//   // Choose the lowest possible 5-bit brightness that will work.
//   uint8_t brightness5Bit = 1;
//   while(brightness5Bit * 255 < power && brightness5Bit < 31)
//   {
//     brightness5Bit++;
//   }

//   // Uncomment this line to simulate an LED strip that does not
//   // have the extra 5-bit brightness register.  You will notice
//   // that roughly the first third of the LED strip turns off
//   // because the brightness8Bit equals zero.
//   //brightness = 31;

//   // Set brightness8Bit to be power divided by brightness5Bit,
//   // rounded to the nearest whole number.
//   uint8_t brightness8Bit = (power + (brightness5Bit / 2)) / brightness5Bit;

//   // Send the white color to the LED strip.  At this point,
//   // brightness8Bit multiplied by brightness5Bit should be
//   // approximately equal to power.
//   apa102_sendColor(apa102,brightness8Bit, brightness8Bit, brightness8Bit, brightness5Bit);
// }

// void leds(tembed_t tembed) {
//     ESP_LOGI(TAG, "LEDS");
// // Calculate what the ratio between the powers of consecutive
// // LEDs needs to be in order to reach the max power on the last
// // LED of the strip.
//     float multiplier = pow(maxPower / minPower, 1.0 / (ledCount - 1));
//     apa102_startFrame(&tembed->leds);
//     float power = minPower;
//     for(uint16_t i = 0; i < ledCount; i++)
//     {
//         sendWhite(&tembed->leds,power);
//         power = power * multiplier;
//     }
//     apa102_endFrame(&tembed->leds,ledCount);
// }

// lv_obj_t *count_label;
// int state = 0;
// int n = 3;
// lv_obj_t *labels[3];

// void update_labels(){
//     int num_labels = 3;
//     int radius = 75;
//     for (int i = 0; i < num_labels; i++) { 
//         int j = (i + state) % num_labels;

//         // Calculate the angle for the label
//         float angle = (2 * M_PI / num_labels) * j; // Evenly distribute around the circle
//         int x = radius * cos(angle) -75;
//         int y = radius * sin(angle);

//         // Align the label to the center of the calculated position
//         lv_obj_align(labels[i], LV_ALIGN_CENTER, x, y);
//     }
// }

// static void knob_left_cb(void *arg, void *data)
// {
//     ESP_LOGI(TAG, "KNOB: KNOB_LEFT Count is %d", iot_knob_get_count_value((knob_handle_t)arg));
//     state = (state + 1) % n;
//     update_labels();
//     //lv_label_set_text_fmt(count_label,"%d",iot_knob_get_count_value((knob_handle_t)arg));
// }

// static void knob_right_cb(void *arg, void *data)
// {
//     ESP_LOGI(TAG, "KNOB: KNOB_RIGHT Count is %d", iot_knob_get_count_value((knob_handle_t)arg));
//     //v_label_set_text_fmt(count_label,"%d",iot_knob_get_count_value((knob_handle_t)arg));
//     state = (state - 1) % n;
//     update_labels();

// }

// static void button_press_down_cb(void *arg, void *data) {
//     ESP_LOGI(TAG, "Down!");
// }

// void lvgl_demo_ui(lv_disp_t *disp) {
//     /*Change the active screen's background color*/
//     lv_obj_set_style_bg_color(lv_disp_get_scr_act(disp), lv_color_hex(0xFF00E1), LV_PART_MAIN);

//     static lv_style_t l_style;
//     lv_style_init(&l_style);
//     lv_style_set_width(&l_style, 75);
//     lv_style_set_height(&l_style, 40);
//     lv_style_set_text_color(&l_style, lv_color_white());
//     lv_style_set_text_align(&l_style, LV_TEXT_ALIGN_CENTER);
//     lv_style_set_bg_color(&l_style, lv_palette_main(LV_PALETTE_BLUE));
//     lv_style_set_bg_opa(&l_style,LV_OPA_COVER);

//     /*Create a container with ROW flex direction*/
//     lv_obj_t * cont_row = lv_obj_create(lv_disp_get_scr_act(disp));
//     lv_obj_set_size(cont_row, 300, 75);
//     lv_obj_align(cont_row, LV_ALIGN_TOP_MID, 0, 5);
//     lv_obj_set_flex_flow(cont_row, LV_FLEX_FLOW_ROW);
//     lv_obj_set_style_bg_color(cont_row, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);

// }
// void lvgl_circle_ui(lv_disp_t *disp) {
//     /* Change the active screen's background color */
//     lv_obj_set_style_bg_color(lv_disp_get_scr_act(disp), lv_color_hex(0x000000), LV_PART_MAIN);

//     /* Create a circular background */
//     lv_obj_t *circle_bg = lv_obj_create(lv_disp_get_scr_act(disp));
//     lv_obj_set_size(circle_bg, 125, 125); // Adjust size as needed
//     lv_obj_align(circle_bg, LV_ALIGN_CENTER, -75, 0);
//     lv_obj_set_style_bg_color(circle_bg, lv_color_hex(0xCCCCCC), LV_PART_MAIN); // Light gray
//     lv_obj_set_style_bg_opa(circle_bg, LV_OPA_COVER, LV_PART_MAIN);
//     lv_obj_set_style_radius(circle_bg, 150, LV_PART_MAIN); // Make it circular

//     static lv_style_t l_style;
//     lv_style_init(&l_style);
//     lv_style_set_width(&l_style, 75);
//     lv_style_set_height(&l_style, 40);
//     lv_style_set_text_color(&l_style, lv_color_white());
//     lv_style_set_text_align(&l_style, LV_TEXT_ALIGN_CENTER);
//     lv_style_set_bg_color(&l_style, lv_palette_main(LV_PALETTE_BLUE));
//     lv_style_set_bg_opa(&l_style, LV_OPA_COVER);

//     /* Create labels around the circle */
//     const char *labels_text[] = {"Task 1", "Task 2", "Task 3"};
//     const lv_color_t colors_hex[] = {lv_color_hex(0xff0000), lv_palette_main(LV_PALETTE_GREEN), lv_palette_main(LV_PALETTE_BLUE)};
//     int radius = 75;
//     int num_labels = 3;
//     for (int i = 0; i < num_labels; i++) {
//         lv_obj_t *label = lv_label_create(lv_disp_get_scr_act(disp));
//         labels[i] = label;
//         lv_label_set_text_static(label, labels_text[i]);
//         lv_obj_add_style(label, &l_style, LV_PART_MAIN);
//         lv_obj_set_style_bg_color(label, colors_hex[i], LV_PART_MAIN);

//         // Calculate the angle for the label
//         float angle = (2 * M_PI / num_labels) * i; // Evenly distribute around the circle
//         int x = radius * cos(angle) -75;
//         int y = radius * sin(angle);

//         // Align the label to the center of the calculated position
//         lv_obj_align(label, LV_ALIGN_CENTER, x, y);
//         lv_obj_set_size(label, 60, 20); // Adjust size as needed
//         lv_obj_set_style_radius(label, 20, LV_PART_MAIN); // Rounded corners
//     }
//     update_labels();

// }

// void app_main(void)
// {
//     ESP_LOGI(TAG,"Hello lcd!");

//     /* Print chip information */
//     // esp_chip_info_t chip_info;
//     // uint32_t flash_size;
//     // esp_chip_info(&chip_info);
//     // printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
//     //        CONFIG_IDF_TARGET,
//     //        chip_info.cores,
//     //        (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
//     //        (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

//     // unsigned major_rev = chip_infoProject - Time Tracker Screen.revision / 100;
//     // unsigned minor_rev = chip_info.revision % 100;
//     // printf("silicon revision v%d.%d, ", major_rev, minor_rev);
//     // if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
//     //     printf("Get flash size failed");
//     //     return;
//     // }

//     // printf("%uMB %s flash\n", flash_size / (1024 * 1024),
//     //        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

//     // printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());

//     // Initialize the T-Embed
//     tembed_t tembed = tembed_init(notify_lvgl_flush_ready, &lvgl_disp_drv);

//     leds(tembed);

//     iot_button_register_cb(tembed->dial.btn, BUTTON_PRESS_DOWN, button_press_down_cb, NULL);

//     iot_knob_register_cb(tembed->dial.knob, KNOB_LEFT, knob_left_cb, NULL);
//     iot_knob_register_cb(tembed->dial.knob, KNOB_RIGHT, knob_right_cb, NULL);

//     lv_disp_t *lvgl_disp = tembed_lvgl_init(tembed);

//     ESP_LOGI(TAG, "Display LVGL");
//     lvgl_circle_ui(lvgl_disp);
//     //turn_off_device();

//     while (1) {
//         // raise the task priority of LVGL and/or reduce the handler period can improve the performance
//         vTaskDelay(pdMS_TO_TICKS(10));
//         // The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
//         lv_timer_handler();
//     }
//     // turn_off_device();

// }
