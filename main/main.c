#include <stdio.h>
#include "driver/gpio.h"
#include <driver/i2c_master.h>
#include "driver/ledc.h"
#include <esp_log.h>
#include "esp_attr.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "portmacro.h"
#include "soc/gpio_num.h"
#include "cat20.h"


#include <u8g2.h>
#include <u8g2_esp32_hal.h>

#define OLED_ON_OFF_GPIO GPIO_NUM_15
#define OLED_CLK GPIO_NUM_18
#define OLED_MOSI GPIO_NUM_23
#define OLED_CS GPIO_NUM_5
#define OLED_DC GPIO_NUM_17
#define OLED_RESET GPIO_NUM_16

#define DEBOUNCE_MS 50

#define PRIOR_ON_OFF_OLED 4

#define BUZZER_GPIO GPIO_NUM_25

static TaskHandle_t oled_on_off_handle = NULL;
static u8g2_t data;

static void buzzer_init(void){
    ledc_timer_config_t ledc_timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 2700,
        .clk_cfg         = LEDC_AUTO_CLK,
    };

    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .gpio_num   = BUZZER_GPIO,
        .duty       = 0,
        .hpoint     = 0,
    };

    ledc_channel_config(&ledc_channel);

}
void buzzer_tone(uint32_t freq) {
    if (freq) {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
    } else {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    }
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void play_sound(void) {
    const uint32_t notes[][2] = {
    {2093, 90}, {0, 20},   // ti  (C7)
    {2637, 90}, {0, 20},   // ru  (E7)
    {3136, 160},           // ri  (G7)
};
    for (int i = 0; i < sizeof(notes) / sizeof(notes[0]); i++) {
        buzzer_tone(notes[i][0]);
        vTaskDelay(pdMS_TO_TICKS(notes[i][1]));
    }
    buzzer_tone(0);
}

static void IRAM_ATTR oled_onoff_but_isr_handler(void *arg){
    gpio_intr_disable(OLED_ON_OFF_GPIO);
    BaseType_t higher_prior_woken = pdFALSE;
    vTaskNotifyGiveFromISR(oled_on_off_handle, &higher_prior_woken);
    portYIELD_FROM_ISR(higher_prior_woken);
    
}

static void oled_on_off_task(void *args){
    bool on = false;

    while(1){

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));

        if (gpio_get_level(OLED_ON_OFF_GPIO) == 0){
            on = !on;
            play_sound();
            u8g2_SetPowerSave(&data, on ? 0 : 1);
            while (gpio_get_level(OLED_ON_OFF_GPIO) == 0)
                vTaskDelay(pdMS_TO_TICKS(10));
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS)); 
        }

        gpio_intr_enable(OLED_ON_OFF_GPIO);
    }
}

static void button_init(void){
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << OLED_ON_OFF_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
}

void app_main(void)
{
    buzzer_init();
    u8g2_esp32_hal_t oled_hal = U8G2_ESP32_HAL_DEFAULT;
    oled_hal.bus.spi.clk   = OLED_CLK;
    oled_hal.bus.spi.mosi  = OLED_MOSI;
    oled_hal.bus.spi.cs    = OLED_CS;
    oled_hal.dc    = OLED_DC;
    oled_hal.reset = OLED_RESET;
    u8g2_esp32_hal_init(oled_hal);

    u8g2_Setup_ssd1322_nhd_256x64_f(&data, U8G2_R2, u8g2_esp32_spi_byte_cb, u8g2_esp32_gpio_and_delay_cb);

    u8g2_InitDisplay(&data);

    u8g2_SetPowerSave(&data, 0);

    u8g2_ClearBuffer(&data);
    u8g2_DrawXBM(&data, 96, 0, cat20_width, cat20_height, cat20_bits); // wyśrodkowany: (256-64)/2
    u8g2_SendBuffer(&data); 
    
    u8g2_SetPowerSave(&data, 1);

    button_init();
    xTaskCreate(oled_on_off_task, "oled_on_off_task", 2048, NULL, PRIOR_ON_OFF_OLED, &oled_on_off_handle);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    gpio_isr_handler_add(OLED_ON_OFF_GPIO, oled_onoff_but_isr_handler, NULL);

}
