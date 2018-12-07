/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
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

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;
const wifi_promiscuous_filter_t filt={
    .filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT
};
static const char *TAG = "wifi station";

static int s_retry_num = 0;
uint32_t last_clock_registered = 0;

typedef struct {
    unsigned frame_ctrl:16;
    unsigned duration_id:16;
    uint8_t addr1[6]; /* receiver address */
    uint8_t addr2[6]; /* sender address */
    uint8_t addr3[6]; /* filtering address */
    unsigned sequence_ctrl:16;
    uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct {
    wifi_ieee80211_mac_hdr_t hdr;
    uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

/***
 * 
 * FUNCTIONS AVAILABLE 
 * 
 * ***/
static esp_err_t event_handler(void *ctx, system_event_t *event);
static void wifi_init_sniffer_sta(void);
static const char *wifi_sniffer_packet_type2str(wifi_vendor_ie_type_t type);
static void wifi_sniffer_packet_handler(void *buff, wifi_vendor_ie_type_t type);
static uint64_t xos_cycles_to_msecs(uint64_t cycles);




static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    printf( "event_handler starting.\n");
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        printf( "Got ip:%s !! \n", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            s_retry_num++;
            printf("Retry to connect to the AP\n");
        }
        //last_clock_registered=0;
        printf("connect to the AP fail\n");
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_sniffer_sta()
{
    printf( "About to establish connection with AP. ");
    printf( "SSID:%s, password:%s.\n",
             CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    printf( "Wifi_init_sta finished. ");
    printf( "Connect to ap SSID:%s password:%s.\n",
             CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filt));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler));
}

void app_main()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    printf( "Enabling ESP_WIFI_MODE_STA... \n");
    wifi_init_sniffer_sta();
}

const char *wifi_sniffer_packet_type2str(wifi_vendor_ie_type_t type)
{
    switch(type) {
    case WIFI_VND_IE_TYPE_BEACON: return "BEACON";
    default:    
    case WIFI_PKT_MISC: return "NO BEACON";
    }
}

void wifi_sniffer_packet_handler(void* buff, wifi_vendor_ie_type_t type)
{
    /*
        typedef struct {
            wifi_pkt_rx_ctrl_t rx_ctrl; < metadata header 
            uint8_t payload[0]; < Data or management payload. Length of payload is described by
                                    rx_ctrl.sig_len. Type of content determined by packet type 
                                    argument of callback.
        } wifi_promiscuous_pkt_t;
    */
    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);
    for(int i=0; i<sizeof(ap_info.bssid); i++){
        if(ap_info.bssid[i]!=hdr->addr2[i]){
            return;
        }
    }
    last_clock_registered =  (uint32_t *) xTaskGetTickCountFromISR();  
}

uint32_t get_ticks_from_last_beacon(uint32_t curr_clock)
{
    printf("Diff = %u. ",curr_clock-last_clock_registered);
    printf("Freq = %u\n",esp_clk_cpu_freq());   
    return (curr_clock-last_clock_registered);
}
