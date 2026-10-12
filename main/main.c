#include <stdio.h>
#include "driver/gpio.h"
#include <driver/i2c_master.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_num.h"

#include <u8g2.h>
#include <u8g2_esp32_hal.h>

#define LED_GPIO GPIO_NUM_2
#define OLED_CLK GPIO_NUM_18
#define OLED_MOSI GPIO_NUM_23
#define OLED_CS GPIO_NUM_5
#define OLED_DC GPIO_NUM_17
#define OLED_RESET GPIO_NUM_16

static void led_init(void){
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

void app_main(void)
{
    u8g2_esp32_hal_t oled_hal = U8G2_ESP32_HAL_DEFAULT;
    oled_hal.bus.spi.clk   = OLED_CLK;
    oled_hal.bus.spi.mosi  = OLED_MOSI;
    oled_hal.bus.spi.cs    = OLED_CS;
    oled_hal.dc    = OLED_DC;
    oled_hal.reset = OLED_RESET;
    u8g2_esp32_hal_init(oled_hal);

    u8g2_t data;

    u8g2_Setup_ssd1322_nhd_256x64_f(&data, U8G2_R2, u8g2_esp32_spi_byte_cb, u8g2_esp32_gpio_and_delay_cb);

    u8g2_InitDisplay(&data);

    u8g2_SetPowerSave(&data, 0);

    u8g2_ClearBuffer(&data);
    u8g2_SetFont(&data, u8g2_font_ncenB14_tr);
    u8g2_DrawStr(&data, 0, 20, "I love cats");
    u8g2_SendBuffer(&data);
    
}
