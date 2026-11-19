#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_err.h"

#define BTN_FLOOR_1 GPIO_NUM_25
#define BTN_FLOOR_2 GPIO_NUM_26
#define BTN_FLOOR_3 GPIO_NUM_27

#define SR_DATA GPIO_NUM_22
#define SR_CLOCK GPIO_NUM_18
#define SR_LATCH GPIO_NUM_5

#define SEG_A GPIO_NUM_4
#define SEG_B GPIO_NUM_16
#define SEG_C GPIO_NUM_17
#define SEG_D GPIO_NUM_19
#define SEG_E GPIO_NUM_21
#define SEG_F GPIO_NUM_23
#define SEG_G GPIO_NUM_32

#define BUZZER_PIN GPIO_NUM_33

#define DOOR_OPEN_TIME 2000
#define DOOR_CLOSE_TIME 1500
#define TRAVEL_TIME_PER_FLOOR 2000
#define ARRIVAL_TIME 700
#define DEBOUNCE_TIME 50

static const char *TAG = "ELEVATOR";

typedef enum {
    DOORS_OPEN,
    DOORS_CLOSING,
    MOVING,
    ARRIVING
} ElevatorState;

static ElevatorState current_state = DOORS_OPEN;

static int current_floor = 1;
static int target_floor = 1;
static int active_floor = 0;

static int request_queue[3];
static int queue_count = 0;

static int64_t state_start_time = 0;
static int64_t movement_start_time = 0;
static int64_t last_blink_time = 0;

static bool movement_led_state = false;
static int64_t buzzer_stop_time = 0;

static int64_t get_time_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void shift_register_write(uint8_t data)
{
    gpio_set_level(SR_LATCH, 0);

    for (int i = 7; i >= 0; i--) {
        gpio_set_level(SR_CLOCK, 0);
        gpio_set_level(SR_DATA, (data >> i) & 1);
        gpio_set_level(SR_CLOCK, 1);
    }

    gpio_set_level(SR_LATCH, 1);
}

static void update_leds(void)
{
    uint8_t led_data = 0;

    if (active_floor == 1)
        led_data |= (1 << 0);

    if (active_floor == 2)
        led_data |= (1 << 1);

    if (active_floor == 3)
        led_data |= (1 << 2);

    for (int i = 0; i < queue_count; i++) {
        if (request_queue[i] == 1)
            led_data |= (1 << 0);

        if (request_queue[i] == 2)
            led_data |= (1 << 1);

        if (request_queue[i] == 3)
            led_data |= (1 << 2);
    }

    if (current_state == MOVING && movement_led_state)
        led_data |= (1 << 3);

    shift_register_write(led_data);
}

static const gpio_num_t segment_pins[7] = {
    SEG_A,
    SEG_B,
    SEG_C,
    SEG_D,
    SEG_E,
    SEG_F,
    SEG_G
};

static const uint8_t digit_patterns[4] = {
    0b1111110,
    0b0110000,
    0b1101101,
    0b1111001
};

static void display_floor(int floor)
{
    if (floor < 0 || floor > 3)
        return;

    uint8_t pattern = digit_patterns[floor];

    for (int i = 0; i < 7; i++) {
        int bit = (pattern >> (6 - i)) & 1;
        gpio_set_level(segment_pins[i], bit);
    }
}

static void buzzer_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .gpio_num = BUZZER_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };

    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

static void buzzer_tone(int frequency)
{
    if (frequency <= 0) {
        ledc_set_duty(
            LEDC_LOW_SPEED_MODE,
            LEDC_CHANNEL_0,
            0
        );

        ledc_update_duty(
            LEDC_LOW_SPEED_MODE,
            LEDC_CHANNEL_0
        );

        return;
    }

    ledc_set_freq(
        LEDC_LOW_SPEED_MODE,
        LEDC_TIMER_0,
        frequency
    );

    ledc_set_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0,
        96
    );

    ledc_update_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0
    );
}

static void buzzer_beep(int frequency, int duration_ms)
{
    buzzer_tone(frequency);
    buzzer_stop_time = get_time_ms() + duration_ms;
}

static void buzzer_update(void)
{
    if (buzzer_stop_time > 0 && get_time_ms() >= buzzer_stop_time) {
        buzzer_tone(0);
        buzzer_stop_time = 0;
    }
}

typedef struct {
    gpio_num_t pin;
    int floor;
    int last_read;
    int stable_state;
    int64_t last_change_time;
} Button;

static Button buttons[3] = {
    {BTN_FLOOR_1, 1, 1, 1, 0},
    {BTN_FLOOR_2, 2, 1, 1, 0},
    {BTN_FLOOR_3, 3, 1, 1, 0}
};

static bool request_exists(int floor)
{
    if (active_floor == floor)
        return true;

    for (int i = 0; i < queue_count; i++) {
        if (request_queue[i] == floor)
            return true;
    }

    return false;
}

static void add_request(int floor)
{
    if (current_state == DOORS_OPEN &&
        active_floor == 0 &&
        current_floor == floor) {

        ESP_LOGI(TAG, "Already on floor %d", floor);
        return;
    }

    if (request_exists(floor)) {
        ESP_LOGI(TAG, "Floor %d already requested", floor);
        return;
    }

    if (queue_count >= 3) {
        ESP_LOGW(TAG, "Request queue is full");
        return;
    }

    request_queue[queue_count] = floor;
    queue_count++;

    ESP_LOGI(TAG, "Floor %d request added", floor);

    update_leds();
}

static int get_next_request(void)
{
    if (queue_count == 0)
        return 0;

    int next_floor = request_queue[0];

    for (int i = 0; i < queue_count - 1; i++) {
        request_queue[i] = request_queue[i + 1];
    }

    queue_count--;

    return next_floor;
}

static void check_buttons(void)
{
    int64_t now = get_time_ms();

    for (int i = 0; i < 3; i++) {
        int reading = gpio_get_level(buttons[i].pin);

        if (reading != buttons[i].last_read) {
            buttons[i].last_change_time = now;
            buttons[i].last_read = reading;
        }

        if ((now - buttons[i].last_change_time)
            >= DEBOUNCE_TIME) {

            if (reading != buttons[i].stable_state) {
                buttons[i].stable_state = reading;

                if (buttons[i].stable_state == 0) {
                    ESP_LOGI(
                        TAG,
                        "Button GPIO %d pressed",
                        buttons[i].pin
                    );

                    add_request(buttons[i].floor);
                }
            }
        }
    }
}

static void update_movement_led(void)
{
    int64_t now = get_time_ms();

    if (current_state == MOVING) {
        if (now - last_blink_time >= 300) {
            movement_led_state = !movement_led_state;
            last_blink_time = now;
            update_leds();
        }
    }
    else if (movement_led_state) {
        movement_led_state = false;
        update_leds();
    }
}

static void elevator_update(void)
{
    int64_t now = get_time_ms();

    switch (current_state) {

        case DOORS_OPEN:
            if (now - state_start_time >= DOOR_OPEN_TIME) {
                if (queue_count > 0) {
                    active_floor = get_next_request();
                    target_floor = active_floor;

                    ESP_LOGI(
                        TAG,
                        "Closing doors. Target floor: %d",
                        target_floor
                    );

                    current_state = DOORS_CLOSING;
                    state_start_time = now;
                    buzzer_beep(1600, 90);
                    update_leds();
                }
            }
            break;

        case DOORS_CLOSING:
            if (now - state_start_time >= DOOR_CLOSE_TIME) {
                current_state = MOVING;
                movement_start_time = now;
                last_blink_time = now;
                movement_led_state = true;
                buzzer_tone(0);
                buzzer_stop_time = 0;

                ESP_LOGI(
                    TAG,
                    "Elevator moving from floor %d to floor %d",
                    current_floor,
                    target_floor
                );

                update_leds();
            }
            break;

        case MOVING: {
            int distance = target_floor - current_floor;

            if (distance < 0)
                distance = -distance;

            int64_t travel_time =
                (int64_t)distance * TRAVEL_TIME_PER_FLOOR;

            int64_t elapsed = now - movement_start_time;

            int direction = (target_floor > current_floor) ? 1 : -1;
            int floors_passed = (int)(elapsed / TRAVEL_TIME_PER_FLOOR);

            if (floors_passed > distance)
                floors_passed = distance;

            int shown_floor = current_floor + (direction * floors_passed);
            static int last_shown_floor = -1;

            if (shown_floor != last_shown_floor) {
                display_floor(shown_floor);
                last_shown_floor = shown_floor;
            }

            if (elapsed >= travel_time) {
                current_floor = target_floor;
                display_floor(current_floor);
                last_shown_floor = current_floor;
                current_state = ARRIVING;
                state_start_time = now;
                movement_led_state = false;
                buzzer_beep(1800, 110);

                ESP_LOGI(
                    TAG,
                    "Arrived at floor %d",
                    current_floor
                );

                update_leds();
            }

            break;
        }

        case ARRIVING:
            if (now - state_start_time >= ARRIVAL_TIME) {
                active_floor = 0;
                current_state = DOORS_OPEN;
                state_start_time = now;

                ESP_LOGI(TAG, "Doors are open");

                update_leds();
            }
            break;
    }
}

static void gpio_init_all(void)
{
    gpio_config_t button_config = {
        .pin_bit_mask =
            (1ULL << BTN_FLOOR_1) |
            (1ULL << BTN_FLOOR_2) |
            (1ULL << BTN_FLOOR_3),

        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&button_config));

    gpio_config_t sr_config = {
        .pin_bit_mask =
            (1ULL << SR_DATA) |
            (1ULL << SR_CLOCK) |
            (1ULL << SR_LATCH),

        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&sr_config));

    uint64_t segment_mask = 0;

    for (int i = 0; i < 7; i++) {
        segment_mask |= (1ULL << segment_pins[i]);
    }

    gpio_config_t segment_config = {
        .pin_bit_mask = segment_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&segment_config));

    gpio_set_level(SR_DATA, 0);
    gpio_set_level(SR_CLOCK, 0);
    gpio_set_level(SR_LATCH, 1);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Three Floor Elevator Controller Started");

    gpio_init_all();
    buzzer_init();

    current_floor = 1;
    display_floor(current_floor);

    state_start_time = get_time_ms();

    shift_register_write(0);

    ESP_LOGI(TAG, "Elevator is on floor 1");

    while (1) {
        check_buttons();
        elevator_update();
        buzzer_update();
        update_movement_led();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
