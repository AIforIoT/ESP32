#include <string.h>
#include "micro_handler.h"
#include "synch_beacon.h"

int _id_esp = 0;
uint64_t _last_clock_registered = 0;
unsigned _last_beacon_frame_sequence = 0;
 esp_err_t _wifi_event_handler(void *ctx, system_event_t *event)
{
    //ESP_LOGI(CONFIG_TAG, "Event_handler starting.\n");
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
        ESP_LOGI(CONFIG_LOCALIZATION_TAG, "SYSTEM_EVENT_SCAN_DONE popped.");
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
        ESP_LOGI(CONFIG_LOCALIZATION_TAG, "SYSTEM_EVENT_STA_START popped.");
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
        _id_esp=1;     
        break;
    case SYSTEM_EVENT_STA_STOP:
        /*
            If esp_wifi_stop() returns ESP_OK and the current Wi-Fi mode is Station or 
            SoftAP+Station, then this event will arise. Upon receiving this event, the 
            event task will release the station’s IP address, stop the DHCP client, remove 
            TCP/UDP-related connections and clear the LwIP station netif, etc. 

            The application event callback generally does not need to do anything.
        */
        //ESP_LOGI(CONFIG_LOCALIZATION_TAG, "SYSTEM_EVENT_STA_STOP popped.");
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
        //ESP_LOGI(CONFIG_LOCALIZATION_TAG, "SYSTEM_EVENT_STA_CONNECTED popped.");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /*
            Upon receiving this event, the event task will shut down the station’s LwIP 
            netif and notify the LwIP task to clear the UDP/TCP connections which cause 
            the wrong status to all sockets. For socket-based applications, the application 
            callback needs to close all sockets and re-create them, if necessary, upon 
            receiving this event.
        */
        ESP_LOGI(CONFIG_TAG, "Connection with AP lost...\n");

        uint8_t s_retry_num = CONFIG_ESP_MAXIMUM_RETRY;
        while (s_retry_num > 0 ) {
            esp_wifi_connect();
            s_retry_num--;
            ESP_LOGI(CONFIG_TAG, "Retry to connect to the AP\n");
        }
        ESP_LOGE(CONFIG_TAG, "Connect to the AP fail\n");
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        /*
            This event arises when the DHCP client successfully gets the IP address from 
            the DHCP server. The event means that everything is ready and the application 
            can begin its tasks (e.g., creating sockets).
        */
        ESP_LOGI(CONFIG_TAG, "Got ip:%s !!", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        sniffer_config_wifi();
         /*
            Start listening to microphone and post when info retrieved.
        */
        config_gpio_handler();
        break;
    
    default:
        break;
    }
    return ESP_OK;
}

void app_main()
{   
    //ESP_LOGI(CONFIG_TAG, "Application started... \n");
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
    ESP_ERROR_CHECK(esp_event_loop_init(_wifi_event_handler, NULL) ); 
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
    //ESP_LOGI(CONFIG_TAG, "WiFi Config Started...");
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

/***
 * Config Wifi to work on promiscuous mode and use sniffer function as callback.
 * ***/
void sniffer_config_wifi()
{
    uint32_t start_tick = XTHAL_GET_CCOUNT();
    /*
        Enable the promiscuous mode packet type filter. The default filter is to filter all 
        packets except WIFI_PKT_MISC.
        @param filter the packet type filtered in promiscuous mode.
    */
    wifi_promiscuous_filter_t filt={
        .filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT
    };
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filt));
    /*
        Enable the promiscuous mode.
    */
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    /*
        Register the RX callback function in the promiscuous mode.
        Each time a packet is received, the registered callback function will be called.
    */
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler));
    uint32_t final_tick = XTHAL_GET_CCOUNT();
    //ESP_LOGI("Sniffer Config", "Final_t %u; Start_t %u; Diff %u.",final_tick, start_tick, final_tick-start_tick);

}

//int debug_sniff=0;
//int total_packets=0;
/***
 * For each packet received through promiscuous mode:
 *      Check if it's a beacon corresponding to own AP. 
 *          If so, synch lastClockRegistered.
 *          Otherwise continue.
 * ***/
void wifi_sniffer_packet_handler(void* buff, wifi_vendor_ie_type_t type)
{
    //total_packets++;
    //debug_sniff++;
    if(type != WIFI_VND_IE_TYPE_BEACON)
        return;
    /*
        Promiscuous packets deserialization:
        typedef struct {
            typedef struct {
                signed rssi:8;                =>< Received Signal Strength Indicator(RSSI) of packet. unit: dBm >
                unsigned rate:5;              =>< PHY rate encoding of the packet. Only valid for non HT(11bg) packet >
                unsigned :1;                  =>< reserve >
                unsigned sig_mode:2;          =>< 0: non HT(11bg) packet; 1: HT(11n) packet; 3: VHT(11ac) packet >
                unsigned :16;                 =>< reserve >
                unsigned mcs:7;               =>< Modulation Coding Scheme. If is HT(11n) packet, shows the modulation, range from 0 to 76(MSC0 ~ MCS76) >
                unsigned cwb:1;               =>< Channel Bandwidth of the packet. 0: 20MHz; 1: 40MHz >
                unsigned :16;                 =>< reserve >
                unsigned smoothing:1;         =>< reserve >
                unsigned not_sounding:1;      =>< reserve >
                unsigned :1;                  =>< reserve >
                unsigned aggregation:1;       =>< Aggregation. 0: MPDU packet; 1: AMPDU packet >
                unsigned stbc:2;              =>< Space Time Block Code(STBC). 0: non STBC packet; 1: STBC packet >
                unsigned fec_coding:1;        =>< Flag is set for 11n packets which are LDPC >
                unsigned sgi:1;               =>< Short Guide Interval(SGI). 0: Long GI; 1: Short GI >
                signed noise_floor:8;         =>< noise floor of Radio Frequency Module(RF). unit: 0.25dBm>
                unsigned ampdu_cnt:8;         =>< ampdu cnt >
                unsigned channel:4;           =>< primary channel on which this packet is received >
                unsigned secondary_channel:4; =>< secondary channel on which this packet is received. 0: none; 1: above; 2: below >
                unsigned :8;                  =>< reserve >
                unsigned timestamp:32;        =>< timestamp. The local time when this packet is received. It is precise only if modem sleep or light sleep is not enabled. unit: microsecond >
                unsigned :32;                 =>< reserve >
                unsigned :31;                 =>< reserve >
                unsigned ant:1;               =>< antenna number from which this packet is received. 0: WiFi antenna 0; 1: WiFi antenna 1 >
                unsigned sig_len:12;          =>< length of packet including Frame Check Sequence(FCS) >
                unsigned :12;                 =>< reserve >
                unsigned rx_state:8; =>< state of the packet. 0: no error; others: error numbers which are not public >
            } wifi_pkt_rx_ctrl_t,
            typedef struct {
                typedef struct {
                    unsigned frame_ctrl:16;
                    unsigned duration_id:16;
                    uint8_t addr1[6]; =>< receiver address >
                    uint8_t addr2[6]; =>< sender address >
                    uint8_t addr3[6]; =>< filtering address >
                    unsigned sequence_ctrl:16;
                    uint8_t addr4[6]; =>< optional >
                } wifi_ieee80211_mac_hdr_t,
                typedef struct {
                    typedef struct{

                    } wifi_ieee80211_data_network_t,
                    typedef struct{

                    } wifi_ieee80211_data_csum_t;
                } wifi_ieee80211_payload_data_t;
            } wifi_ieee80211_packet_t;
        } wifi_promiscuous_pkt_t;
    */
    /*
        Promiscuous packets deserialization with names defined in esp_wifi.h:
        wifi_promiscuous_pkt_t {
            wifi_pkt_rx_ctrl_t,
            (wifi_ieee80211_packet_t) payload {
                wifi_ieee80211_mac_hdr_t,
                (wifi_ieee80211_payload_data_t) payload {
                    wifi_ieee80211_data_network_t,
                    wifi_ieee80211_data_csum_t
                }
            }
        }
    */
    /*
        Define structs that will be used to parse promiscuous packets. Note that they are not defined
        on esp_wifi.h lib.
    */
    /*
        uint8_t beacon_raw[] = {
            0x80, 0x00,							// 0-1: Frame Control
            0x00, 0x00,							// 2-3: Duration
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff,				// 4-9: Destination address (broadcast)
            0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,				// 10-15: Source address
            0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,				// 16-21: BSSID
            0x00, 0x00,							// 22-23: Sequence / fragment number
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,			// 24-31: Timestamp (GETS OVERWRITTEN TO 0 BY HARDWARE)
            0x64, 0x00,							// 32-33: Beacon interval
            0x31, 0x04,							// 34-35: Capability info
            0x00, 0x00,  FILL CONTENT HERE 				// 36-38: SSID parameter set, 0x00:length:content
            0x01, 0x08, 0x82, 0x84,	0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,	// 39-48: Supported rates
            0x03, 0x01, 0x01,						// 49-51: DS Parameter set, current channel 1 (= 0x01),
            0x05, 0x04, 0x01, 0x02, 0x00, 0x00,				// 52-57: Traffic Indication Map
        };
    */
    typedef struct {
        uint16_t interval;
        uint16_t capability;
        uint8_t tag_number;
        uint8_t tag_length;
        char ssid[0];
        uint8_t payload[0];
    } wifi_mgmt_beacon_t;
    typedef struct {
        unsigned frame_ctrl:16;
        unsigned duration_id:16;
        uint8_t addr1[6]; 
        uint8_t addr2[6]; 
        uint8_t addr3[6]; 
        unsigned sequence_ctrl:12;
        unsigned segmentation_ctrl:4;
        uint8_t addr4[6];
    } wifi_ieee80211_mac_hdr_t;
    typedef struct {
        wifi_ieee80211_mac_hdr_t hdr;
        wifi_mgmt_beacon_t beacon_info;
        uint8_t fcfs[4];
    } wifi_ieee80211_packet_t;
    
    /*
        Get buffer stored and cast it as wifi_promiscuous_pkt_t
    */
    wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
    /*
        Get payload from wifi_promiscuous_pkt_t stored and cast it as wifi_ieee80211_packet_t
    */
    wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    /*
        Get wifi_ieee80211_mac_hdr_t from wifi_ieee80211_packet_t
    */
    wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
    /*
        Get wifi_mgmt_beacon_t from wifi_ieee80211_packet_t
    */
    wifi_mgmt_beacon_t *beacon_info = &ipkt->beacon_info;

    /*
        typedef struct {
            uint8_t bssid[6];                     =><< MAC address of AP >
            uint8_t ssid[33];                     =><< SSID of AP >
            uint8_t primary;                      =><< channel of AP >
            wifi_second_chan_t second;            =><< secondary channel of AP >
            int8_t  rssi;                         =><< signal strength of AP >
            wifi_auth_mode_t authmode;            =><< authmode of AP >
            wifi_cipher_type_t pairwise_cipher;   =><< pairwise cipher of AP >
            wifi_cipher_type_t group_cipher;      =><< group cipher of AP >
            wifi_ant_t ant;                       =><< antenna used to receive beacon from AP >
            uint32_t phy_11b:1;                   =><< bit: 0 flag to identify if 11b mode is enabled or not >
            uint32_t phy_11g:1;                   =><< bit: 1 flag to identify if 11g mode is enabled or not >
            uint32_t phy_11n:1;                   =><< bit: 2 flag to identify if 11n mode is enabled or not >
            uint32_t phy_lr:1;                    =><< bit: 3 flag to identify if low rate is enabled or not >
            uint32_t wps:1;                       =><< bit: 4 flag to identify if WPS is supported or not >
            uint32_t reserved:27;                 =><< bit: 5..31 reserved >
            wifi_country_t country;               =><< country information of AP >
        } wifi_ap_record_t;
    */
    wifi_ap_record_t ap_info;
    /*
        Get information of AP which the ESP32 station is associated with.
        Check if MAC of ESP32's provider AP equals to MAC of received packet. If they don't match,
        drop papcket. Otherwise continue.
    */
    esp_wifi_sta_get_ap_info(&ap_info);
    for(int i=0; i<sizeof(ap_info.bssid); i++){
        /*
            Filter ieee802.11 promiscuous packets by:
                - @MAC_AccessPoint=@MAC_Sender_Address (addr2[i]) 
                - @MAC_Receiver_Address=Broadcast (addr1[i])
        */
        if(ap_info.bssid[i]!=hdr->addr2[i] || hdr->addr1[i]!=0xFF){
            /*
                If filters not matched Drop packet.
            */
            //ESP_LOGD(CONFIG_LOCALIZATION_TAG, "Got promiscouous packet with not matching AP. Drop.");
            return;
        }
    }
    /*
    ESP_LOGI(CONFIG_LOCALIZATION_TAG,"PACKET TYPE=%u, CHAN=%02d, RSSI=%02d,"
		" ADDR1=%02x:%02x:%02x:%02x:%02x:%02x,"
		" ADDR2=%02x:%02x:%02x:%02x:%02x:%02x,"
		" ADDR3=%02x:%02x:%02x:%02x:%02x:%02x"
        " AP=%02x:%02x:%02x:%02x:%02x:%02x",
		type,
		ppkt->rx_ctrl.channel,
		ppkt->rx_ctrl.rssi,
		// ADDR1 
		hdr->addr1[0],hdr->addr1[1],hdr->addr1[2],
		hdr->addr1[3],hdr->addr1[4],hdr->addr1[5],
		// ADDR2 
		hdr->addr2[0],hdr->addr2[1],hdr->addr2[2],
		hdr->addr2[3],hdr->addr2[4],hdr->addr2[5],
		// ADDR3
		hdr->addr3[0],hdr->addr3[1],hdr->addr3[2],
		hdr->addr3[3],hdr->addr3[4],hdr->addr3[5],
        // AP
		ap_info.bssid[0],ap_info.bssid[1],ap_info.bssid[2],
		ap_info.bssid[3],ap_info.bssid[4],ap_info.bssid[5]
	);*/
    /*
        Get present tick_count from system to define arrival time of present beacon.
    */
    uint64_t last_beacon = (uint64_t *) XTHAL_GET_CCOUNT();
    /*
        Get time until next beacon provided by present beacon frame.
    */
    int interval_beacon_i = beacon_info->interval;
    register_last_beacon(last_beacon, hdr->sequence_ctrl);
    //ESP_LOGI(CONFIG_LOCALIZATION_TAG, "%llu: Got promiscouous packet with matching AP. Synch.", last_beacon);
    //ESP_LOGI(CONFIG_CLOCK_TAG, "Interval: %d, Packets before match: %d", interval_beacon_i, debug_sniff);
    //ESP_LOGI(CONFIG_LOCALIZATION_TAG, "sequence_ctrl: %u", hdr->sequence_ctrl);
    TickType_t xDelay = interval_beacon_i / portTICK_PERIOD_MS;
    //sleep_not_promiscuous_mode(xDelay);
    //debug_sniff=0;
}

void register_last_beacon(uint64_t curr_clock, unsigned _beacon_frame)
{
    ESP_LOGI(CONFIG_LOCALIZATION_TAG, "Beacon frame: %u, Received clock: %llu, _last_clock: %llu, Diff last to previous beacon: %llu", _last_beacon_frame_sequence, curr_clock, _last_clock_registered, curr_clock-_last_clock_registered);
    _last_clock_registered =  curr_clock;  
    _last_beacon_frame_sequence = _beacon_frame;
}

void sleep_not_promiscuous_mode(TickType_t xDelay)
{
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
    uint32_t sleep_tick = XTHAL_GET_CCOUNT();
    vTaskDelay(xDelay);
    uint32_t awake_tick = XTHAL_GET_CCOUNT();
    //ESP_LOGI(CONFIG_CLOCK_TAG, "Sleep %u; Awake %u; Diff %u.",sleep_tick, awake_tick, awake_tick-sleep_tick);
    sniffer_config_wifi();
    //ESP_LOGI(CONFIG_CLOCK_TAG, "Total: %d", total_packets);
}

uint64_t get_delay()
{
    uint64_t curr_clock = (uint64_t *) XTHAL_GET_CCOUNT();
    uint64_t delay = curr_clock-_last_clock_registered;
    //ESP_LOGI("<DELAY>", "Received clock %llu; _last_clock %llu, Read Delay %llu",curr_clock, _last_clock_registered, delay);
    return delay;
}

unsigned get_seq_last_beacon_frame()
{
    return _last_beacon_frame_sequence;
}

int get_id_esp()
{
    return _id_esp;
}