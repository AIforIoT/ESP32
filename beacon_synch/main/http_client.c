#include <string.h>
#include <stdlib.h>
#include "http_client.h"

/*
    Handler defined to manage http_states. Appended in order to control responses.
    Note that ESP32 might not be able to handle so many LOGS on screen so decided to comment them.
*/
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            //ESP_LOGD(CONFIG_HTTP_TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            //ESP_LOGD(CONFIG_HTTP_TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            //ESP_LOGD(CONFIG_HTTP_TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            //ESP_LOGD(CONFIG_HTTP_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            //ESP_LOGD(CONFIG_HTTP_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
                //printf("%.*s", evt->data_len, (char*)evt->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            //ESP_LOGD(CONFIG_HTTP_TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            //ESP_LOGD(CONFIG_HTTP_TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

/*
    HTTP json POST to *url.
    Sample data to post:
    {
        "delay": "100",
        "beacon_frame": "1024",
        "id_esp":"1"
    }  
    Note that every field on json is a string not it's logical primitive value.
*/
void http_post(char *url, uint64_t delay, unsigned beacon_frame, int id_esp)
{
    char data[20];
    uint64ToChar(data, delay);
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char post_data[100] = "";
    sprintf(post_data, "{\"delay\":\"%s\", \"beacon_frame\":\"%u\", \"id_esp\":\"%i\"}", data, beacon_frame, id_esp);
    // POST
    esp_http_client_set_url(client, url);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(CONFIG_HTTP_TAG, "HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(CONFIG_HTTP_TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
}

/*
    Convertion uint64_t -> String
*/
void uint64ToChar(char a[], uint64_t n)
{
    sprintf(a, "%llu", n);
}

/*
    HTTP GET to *url.
*/
void http_get(char *url)
{
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // GET
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(CONFIG_HTTP_TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(CONFIG_HTTP_TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
}