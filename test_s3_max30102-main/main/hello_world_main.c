/*
 * @Author: mojionghao
 * @Date: 2024-12-05 15:29:35
 * @LastEditors: mojionghao
 * @LastEditTime: 2025-06-13 20:17:01
 * @FilePath: \test_s3_max30102\main\hello_world_main.c
 * @Description:
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "myi2c.h"
#include "max30102.h"
#include "blood.h"
#include "esp_log.h"

max30102_handle_t max30102;
float temp, spo2, heart;
static const char *TAG = "main";
void max30102_task(void *p)
{
    while (1)
    {
        esp_err_t ret = max30102_read_temp(max30102, &temp);
        if (ret != ESP_OK)
        {
            printf("读取温度失败: %s\n", esp_err_to_name(ret));
        }
        else
        {
            printf("温度: %.2f°C\n", temp);
        }
        ESP_LOGI(TAG, "temp:%f", temp);
        temp = 0.0;
        blood_Loop(max30102, &heart, &spo2);
        ESP_LOGI(TAG, "SPO2:%.2f", spo2);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(i2c_master_init());

    max30102 = max30102_create(0, MAX30102_Device_address, GPIO_NUM_10);
    max30102_config(max30102);

    xTaskCreate(max30102_task, "max30102", 4096, NULL, 6, NULL);
}