#ifndef _GPIO_HANDLER_H_
#define _GPIO_HANDLER_H_
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

void config_gpio_handler();
void gpio_task_example(void* arg);
void IRAM_ATTR gpio_isr_handler(void* arg);

#endif