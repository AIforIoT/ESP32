#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "http_client.h"
#include "esp_system.h"

/*
    HTTP GET to *url.
*/
void http_get(char *url);
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
void http_post(char *url, uint64_t delay, unsigned beacon_frame, int id_esp);
/*
    Convertion uint64_t -> String
*/
void uint64ToChar(char a[], uint64_t n);
#endif