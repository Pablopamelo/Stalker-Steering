#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DIR1A_PIN GPIO_NUM_2
#define DIR2A_PIN GPIO_NUM_3
#define PWMA_PIN  GPIO_NUM_4

#define DIR1B_PIN GPIO_NUM_5
#define DIR2B_PIN GPIO_NUM_6
#define PWMB_PIN  GPIO_NUM_7

char response[128];

void set_motor_direction(const char *direction_left, const char *direction_right) {
    if (strcmp(direction_left, "BRAKE") == 0) {
        gpio_set_level(DIR1A_PIN, 1);
        gpio_set_level(DIR2A_PIN, 1);
    } else if (strcmp(direction_left, "FORWARD") == 0) {
        gpio_set_level(DIR1A_PIN, 1);
        gpio_set_level(DIR2A_PIN, 0);
    } else if (strcmp(direction_left, "BACKWARD") == 0) {
        gpio_set_level(DIR1A_PIN, 0);
        gpio_set_level(DIR2A_PIN, 1);
    } else {
        gpio_set_level(DIR1A_PIN, 0);
        gpio_set_level(DIR2A_PIN, 0);
    }

    if (strcmp(direction_right, "BRAKE") == 0) {
        gpio_set_level(DIR1B_PIN, 1);
        gpio_set_level(DIR2B_PIN, 1);
    } else if (strcmp(direction_right, "FORWARD") == 0) {
        gpio_set_level(DIR1B_PIN, 1);
        gpio_set_level(DIR2B_PIN, 0);
    } else if (strcmp(direction_right, "BACKWARD") == 0) {
        gpio_set_level(DIR1B_PIN, 0);
        gpio_set_level(DIR2B_PIN, 1);
    } else {
        gpio_set_level(DIR1B_PIN, 0);
        gpio_set_level(DIR2B_PIN, 0);
    }
}

void set_motor_speed(int speed_left, int speed_right) {
    if (speed_left < 0) speed_left = 0;
    if (speed_left > 100) speed_left = 100;
    if (speed_right < 0) speed_right = 0;
    if (speed_right > 100) speed_right = 100;

    uint32_t duty_cycle_A = (speed_left * 2047) / 100;
    uint32_t duty_cycle_B = (speed_right * 2047) / 100;

    esp_err_t duty_error_A = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_cycle_A);
    esp_err_t duty_error_B = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_cycle_B);

    esp_err_t update_error_A = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    esp_err_t update_error_B = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

    int response_length = snprintf(
        response,
        sizeof(response),
        "\r\nPWM left: %lu / 2047, right: %lu / 2047\r\n",
        duty_cycle_A,
        duty_cycle_B
    );
    if (response_length > 0 && response_length < sizeof(response)) {
        usb_serial_jtag_write_bytes(response, response_length, 0);
    }

    if (duty_error_A != ESP_OK || duty_error_B != ESP_OK ||
        update_error_A != ESP_OK || update_error_B != ESP_OK) {
        response_length = snprintf(
            response,
            sizeof(response),
            "Blad ustawiania PWM: duty A=%s, duty B=%s, update A=%s, update B=%s\r\n",
            esp_err_to_name(duty_error_A),
            esp_err_to_name(duty_error_B),
            esp_err_to_name(update_error_A),
            esp_err_to_name(update_error_B)
        );
        if (response_length > 0 && response_length < sizeof(response)) {
            usb_serial_jtag_write_bytes(response, response_length, 0);
        }
    }
}

void motor_control_task(void *arg) {
    (void)arg;

    gpio_reset_pin(DIR1A_PIN);
    gpio_reset_pin(DIR2A_PIN);

    gpio_reset_pin(DIR1B_PIN);
    gpio_reset_pin(DIR2B_PIN);

    gpio_set_direction(DIR1A_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIR2A_PIN, GPIO_MODE_OUTPUT);

    gpio_set_direction(DIR1B_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIR2B_PIN, GPIO_MODE_OUTPUT);

    const char startup_message[] = "\r\n=== System started ===\r\n";
    const char prompt_message[] = "Enter value between -100 and 100 for pwm and direction ex: 50 -20 makes rover go forward at 50 percent speed and slighly left\r\n>.\r\n";
    usb_serial_jtag_write_bytes(startup_message, sizeof(startup_message) - 1, 0);
    usb_serial_jtag_write_bytes(prompt_message, sizeof(prompt_message) - 1, 0);

    // Konfiguracja PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_11_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 10000,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };

    esp_err_t timer_error = ledc_timer_config(&ledc_timer);
    if (timer_error != ESP_OK) {
        printf("Blad timera PWM: %s\n", esp_err_to_name(timer_error));
        vTaskDelete(NULL);
        return;
    }

    ledc_channel_config_t ledcA_channel = {
        .gpio_num = PWMA_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = {},
        .deconfigure = false,
    };

    ledc_channel_config_t ledcB_channel = {
        .gpio_num = PWMB_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = {},
        .deconfigure = false,
    };

    esp_err_t channelA_error = ledc_channel_config(&ledcA_channel);
    if (channelA_error != ESP_OK) {
        printf("PWM channelA error %d: %s\n", PWMA_PIN, esp_err_to_name(channelA_error));
        vTaskDelete(NULL);
        return;
    }

    esp_err_t channelB_error = ledc_channel_config(&ledcB_channel);
    if (channelB_error != ESP_OK) {
        printf("PWM channelB error %d: %s\n", PWMA_PIN, esp_err_to_name(channelB_error));
        vTaskDelete(NULL);
        return;
    }

    char rx_buffer[32];
    size_t buffer_index = 0;

    while (true) {
        uint8_t data;
        int length = usb_serial_jtag_read_bytes(&data, 1, portMAX_DELAY);

        bool execute_command = false;

        if (length > 0) {
            if (data == 8 || data == 127) {
                if (buffer_index > 0) {
                    --buffer_index;
                }
            } else if (data == '\n' || data == '\r') {
                if (buffer_index > 0) execute_command = true;
            } else if (buffer_index < sizeof(rx_buffer) - 1) {
                rx_buffer[buffer_index++] = static_cast<char>(data);
                usb_serial_jtag_write_bytes(&data, 1, 0);
            }
        }

        if (execute_command) {
            rx_buffer[buffer_index] = '\0'; // Zamknięcie stringa

            char direction_buffer[32];
            char pwm_buffer[32];

            size_t pwm_index = 0;
            size_t direction_index = 0;
            bool after_space = false;

            for(int i = 0; i < buffer_index; i++) {
                if(rx_buffer[i] == ' ') {
                    after_space = true;
                } else if(after_space) {
                    if (direction_index < sizeof(direction_buffer) - 1) {
                        direction_buffer[direction_index++] = rx_buffer[i];
                    }
                } else {
                    if (pwm_index < sizeof(pwm_buffer) - 1) {
                        pwm_buffer[pwm_index++] = rx_buffer[i];
                    }
                }
            }

            pwm_buffer[pwm_index] = '\0';
            direction_buffer[direction_index] = '\0';

            // Zabezpieczenie limitów
            int pwm_value = atoi(pwm_buffer);
            int direction_value = atoi(direction_buffer);

            if (pwm_value > 100) pwm_value = 100;
            if (pwm_value < -100) pwm_value = -100;
            if (direction_value > 100) direction_value = 100;
            if (direction_value < -100) direction_value = -100;

            const char *motor_mode_left = "BRAKE";
            const char *motor_mode_right = "BRAKE";

            int left_pwm = 0;
            int right_pwm = 0;
            int pwm_magnitude = abs(pwm_value);

            if (pwm_value == 0) {
                left_pwm = 0;
                right_pwm = 0;
                motor_mode_left = "BRAKE";
                motor_mode_right = "BRAKE";
            } else if (pwm_value > 0) {
                if (direction_value == 0) {
                    left_pwm = pwm_magnitude;
                    right_pwm = pwm_magnitude;
                    motor_mode_left = "FORWARD";
                    motor_mode_right = "FORWARD";
                } else if (direction_value > 0) {
                    left_pwm = pwm_magnitude;
                    right_pwm = ((100 - 2 * direction_value) * pwm_magnitude) / 100;
                    motor_mode_left = "FORWARD";
                    right_pwm > 0 ? motor_mode_right = "FORWARD" : motor_mode_right = "BACKWARD";
                } else {
                    left_pwm = ((100 + 2 * direction_value) * pwm_magnitude) / 100;
                    right_pwm = pwm_magnitude;
                    motor_mode_right = "FORWARD";
                    left_pwm > 0 ? motor_mode_left = "FORWARD" : motor_mode_left = "BACKWARD";
                }
            } else if (pwm_value < 0) {
                if (direction_value == 0) {
                    left_pwm = pwm_magnitude;
                    right_pwm = pwm_magnitude;
                    motor_mode_left = "BACKWARD";
                    motor_mode_right = "BACKWARD";
                } else if (direction_value > 0) {
                    left_pwm = pwm_magnitude;
                    right_pwm = ((100 - 2 * direction_value) * pwm_magnitude) / 100;
                    motor_mode_left = "BACKWARD";
                    right_pwm > 0 ? motor_mode_right = "BACKWARD" : motor_mode_right = "FORWARD";
                } else {
                    left_pwm = ((100 + 2 * direction_value) * pwm_magnitude) / 100;
                    right_pwm = pwm_magnitude;
                    motor_mode_right = "BACKWARD";
                    left_pwm > 0 ? motor_mode_left = "BACKWARD" : motor_mode_left = "FORWARD";
                }
            }

            left_pwm = abs(left_pwm);
            right_pwm = abs(right_pwm);

            char debug_message[128];

            int length = snprintf(
                debug_message,
                sizeof(debug_message),
                "pwm=%d direction=%d left_pwm=%d right_pwm=%d left_mode=%s right_mode=%s\r\n",
                pwm_value,
                direction_value,
                left_pwm,
                right_pwm,
                motor_mode_left,
                motor_mode_right
            );

            usb_serial_jtag_write_bytes(debug_message, length, 0);

            set_motor_direction(motor_mode_left, motor_mode_right);
            set_motor_speed(left_pwm, right_pwm);

            buffer_index = 0;
        }
    }
}

extern "C" void app_main(void) {
    xTaskCreate(motor_control_task, "motor_control_task", 4096, NULL, 5, NULL);
}