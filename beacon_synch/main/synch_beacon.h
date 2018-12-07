#ifndef _SYNCH_BEACON_H_
#define _SYNCH_BEACON_H_
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_clk.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "xtensa/core-macros.h"

void wifi_init_sniffer_sta(void);
void nvs_start();
void config_wifi();
void sniffer_config_wifi();
void wifi_sniffer_packet_handler(void *buff, wifi_vendor_ie_type_t type);
void register_last_beacon(uint64_t curr_clock, unsigned _beacon_frame);
void sleep_not_promiscuous_mode(TickType_t xDelay);
uint64_t get_delay();
unsigned get_seq_last_beacon_frame();
int get_id_esp();
#endif