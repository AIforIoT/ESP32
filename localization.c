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
    ESP_LOGD(CONFIG_TAG, "Event_handler starting.\n")
    switch(event->event_id) {
    case SYSTEM_EVENT_SCAN_DONE:
        /* 
            Upon receiving this event, the event task does nothing. 
            The application event callback needs to call esp_wifi_scan_get_ap_num()
            and esp_wifi_scan_get_ap_records() to fetch the scanned AP list and trigger
            the Wi-Fi driver to free the internal memory which is allocated during the 
            scan (do not forget to do this)! Refer to ‘ESP32 Wi-Fi Scan’ for a more detailed 
            description.        
        */
        ESP_LOGD(CONFIG_LOCALIZATION_TAG, "SYSTEM_EVENT_SCAN_DONE popped.");
        break;
    case SYSTEM_EVENT_STA_START:
        /*
            SYSTEM_EVENT_STA_START:
            If esp_wifi_start() returns ESP_OK and the current Wi-Fi mode is Station or 
            SoftAP+Station, then this event will arise. Upon receiving this event, the 
            event task will initialize the LwIP network interface (netif). Generally, 
            the application event callback needs to call esp_wifi_connect() to connect 
            to the configured AP.
        */
        ESP_LOGD(CONFIG_LOCALIZATION_TAG, "SYSTEM_EVENT_STA_START popped.");
        /*
            esp_wifi_connect():
            Connect the ESP32 WiFi station to the AP.

            @attention1 This API only impact WIFI_MODE_STA or WIFI_MODE_APSTA mode.
            @attention2 The scanning triggered by esp_wifi_start_scan() will not be effective until connection between ESP32 and the AP is established.
                If ESP32 is scanning and connecting at the same time, ESP32 will abort scanning and return a warning message and error
                number ESP_ERR_WIFI_STATE.
                If you want to do reconnection after ESP32 received disconnect event, remember to add the maximum retry time, otherwise the called    
                scan will not work. This is especially true when the AP doesn't exist, and you still try reconnection after ESP32 received disconnect
                event with the reason code WIFI_REASON_NO_AP_FOUND.
        */
        // Check if esp connected to AP, if so launch disconnect!
        ESP_ERROR_CHECK(esp_wifi_connect());        
        break;
    case SYSTEM_EVENT_STA_STOP:
        /*
            If esp_wifi_stop() returns ESP_OK and the current Wi-Fi mode is Station or 
            SoftAP+Station, then this event will arise. Upon receiving this event, the 
            event task will release the station’s IP address, stop the DHCP client, remove 
            TCP/UDP-related connections and clear the LwIP station netif, etc. 

            The application event callback generally does not need to do anything.
        */
        ESP_LOGD(CONFIG_LOCALIZATION_TAG, "SYSTEM_EVENT_STA_STOP popped.");
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        /* 
            If esp_wifi_connect() returns ESP_OK and the station successfully connects 
            to the target AP, the connection event will arise. Upon receiving this event, 
            the event task starts the DHCP client and begins the DHCP process of getting 
            the IP address. Then, the Wi-Fi driver is ready for sending and receiving data. 
            This moment is good for beginning the application work, provided that the 
            application does not depend on LwIP, namely the IP address. However, if the 
            application is LwIP-based, then you need to wait until the got ip event comes in.
        */
        ESP_LOGD(CONFIG_LOCALIZATION_TAG, "SYSTEM_EVENT_STA_CONNECTED popped.");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /*
            Upon receiving this event, the event task will shut down the station’s LwIP 
            netif and notify the LwIP task to clear the UDP/TCP connections which cause 
            the wrong status to all sockets. For socket-based applications, the application 
            callback needs to close all sockets and re-create them, if necessary, upon 
            receiving this event.
        */
        ESP_LOGD(CONFIG_TAG, "Connection with AP lost...\n");

        int s_retry_num = CONFIG_ESP_MAXIMUM_RETRY;
        while (s_retry_num > 0 ) {
            esp_wifi_connect();
            s_retry_num--;
            ESP_LOGD(CONFIG_TAG, "Retry to connect to the AP\n");
        }
        ESP_LOGD(CONFIG_TAG, "Connect to the AP fail\n");
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        /*
            This event arises when the DHCP client successfully gets the IP address from 
            the DHCP server. The event means that everything is ready and the application 
            can begin its tasks (e.g., creating sockets).
        */
        ESP_LOGD(CONFIG_TAG, "Got ip:%s !! \n", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        break;
    
    default:
        break;
    }
    return ESP_OK;
}

void app_main()
{   
    ESP_LOGD(CONFIG_TAG, "Application started... \n")
    /* 
        ESP_ERROR_CHECK():
        Serves similar purpose as assert, except that it checks esp_err_t value rather
        than a bool condition. If the argument of ESP_ERROR_CHECK() is not equal ESP_OK,
        then an error message is printed on the console, and abort() is called.
    */
    nvs_start();
    /* 
        esp_event_loop_init:
        Initialize event loop and create the event handler and task.
        esp_event_loop_init initializes the event loop used to dispatch event callbacks for 
        Wi-Fi, IP, and Ethernet related events.
    */
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) ); 
    config_wifi();
    /* 
        esp_wifi_start():
        Start WiFi according to current configuration.
    */
    esp_wifi_start();
}

/***
 * Non-volatile storage (NVS) library is designed to store key-value pairs in flash. 
 * This function initializes NVS (nonvolatile storage) library, which is used by some
 * components to store parameters in flash (e.g. WiFi SSID and password).
 * ***/
void nvs_start()
{
    esp_err_t ret = nvs_flash_init();
    /*
        Check for uncompatibilities and try to solve by resetting nvs.
    */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

/***
 * Initialize LwIP-realted actions.
 * Init WiFi driver on ESP32.
 * Set operation mode (station, soft-AP or station+soft-AP).
 * Set SSID and PASS args taken from menuconfig actions.
 * ***/
void config_wifi()
{
    ESP_LOGI(CONFIG_TAG, "WiFi Config Started...");
    /* 
        tcpip_adapter_init():
        Create an LwIP core task and initialize LwIP-related work.
        tcpip_adapter_init should be called in the start of app_main for only once.

        @lwIP: lightweight IP is a widely used open source TCP/IP stack designed
                    for embedded systems. The focus of the lwIP TCP/IP implementation is 
                    to reduce resource usage while still having a full-scale TCP stack.
                    This makes lwIP suitable for use in embedded systems with tens of 
                    kilobytes of free RAM and room for around 40 kilobytes of code ROM.
    */
    tcpip_adapter_init();
    /* 
        WIFI_INIT_CONFIG_DEFAULT:
        #define WIFI_INIT_CONFIG_DEFAULT() { \
            .event_handler = &esp_event_send, \
            .osi_funcs = &g_wifi_osi_funcs, \
            .wpa_crypto_funcs = g_wifi_default_wpa_crypto_funcs, \
            .static_rx_buf_num = CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM,\
            .dynamic_rx_buf_num = CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM,\
            .tx_buf_type = CONFIG_ESP32_WIFI_TX_BUFFER_TYPE,\
            .static_tx_buf_num = WIFI_STATIC_TX_BUFFER_NUM,\
            .dynamic_tx_buf_num = WIFI_DYNAMIC_TX_BUFFER_NUM,\
            .csi_enable = WIFI_CSI_ENABLED,\
            .ampdu_rx_enable = WIFI_AMPDU_RX_ENABLED,\
            .ampdu_tx_enable = WIFI_AMPDU_TX_ENABLED,\
            .nvs_enable = WIFI_NVS_ENABLED,\
            .nano_enable = WIFI_NANO_FORMAT_ENABLED,\
            .tx_ba_win = WIFI_DEFAULT_TX_BA_WIN,\
            .rx_ba_win = WIFI_DEFAULT_RX_BA_WIN,\
            .wifi_task_core_id = WIFI_TASK_CORE_ID,\
            .beacon_max_len = WIFI_SOFTAP_BEACON_MAX_LEN, \
            .magic = WIFI_INIT_CONFIG_MAGIC\
        };
    */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    /* 
        esp_wifi_init():
        Init WiFi Alloc resource for WiFi driver, such as WiFi control structure, RX/TX buffer,
        WiFi NVS structure etc, this WiFi also start WiFi task.
        @attention 1. This API must be called before all other WiFi API can be called
        @attention 2. Always use WIFI_INIT_CONFIG_DEFAULT macro to init the config to default
                        values, this can guarantee all the fields got correct value when more 
                        fields are added into wifi_init_config_t in future release. If you want 
                        to set your owner initial values, overwrite the default values which are 
                        set by WIFI_INIT_CONFIG_DEFAULT, please be notified that the field 'magic' 
                        of wifi_init_config_t should always be WIFI_INIT_CONFIG_MAGIC!
    */ 
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    /* 
        esp_wifi_set_mode():
        Set the WiFi operating mode.
        Options: station, soft-AP or station+soft-AP. The default mode is soft-AP mode.
    */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    /* 
        wifi_config_t:
        Constants declared as CONFIG_x are defined while make menuconfig.
    */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD
        }
    };
    /* 
        esp_wifi_set_config():
        Set the configuration of the ESP32 STA or AP.

        @param1 wifi_interface_t interface mode. Options:  
        @param2 conf station or soft-AP configuration

        @attention1 This API can be called only when specified interface is enabled, 
                        otherwise, API fail.
        @attention2 For station configuration, bssid_set needs to be 0; and it needs to be 1
                        only when users need to check the MAC address of the AP.
        @attention3 ESP32 is limited to only one channel, so when in the soft-AP+station mode, 
                        the soft-AP will adjust its channel automatically to be the same as the 
                        channel of the ESP32 station.
    */
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    ESP_LOGI(CONFIG_TAG, "WiFi Config Done.\n");
}

