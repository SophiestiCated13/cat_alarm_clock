#include <stdio.h>
#include "driver/gpio.h"
#include <driver/i2c_master.h>
#include <esp_log.h>
#include "esp_attr.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "portmacro.h"
#include "soc/gpio_num.h"

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

static TaskHandle_t oled_on_off_handle = NULL;
static u8g2_t data;

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
    u8g2_SetFont(&data, u8g2_font_ncenB14_tr);
    u8g2_DrawStr(&data, 0, 20, "I love cats");
    u8g2_SendBuffer(&data);
    
    u8g2_SetPowerSave(&data, 1);

    button_init();
    xTaskCreate(oled_on_off_task, "oled_on_off_task", 2048, NULL, PRIOR_ON_OFF_OLED, &oled_on_off_handle);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    gpio_isr_handler_add(OLED_ON_OFF_GPIO, oled_onoff_but_isr_handler, NULL);

}
