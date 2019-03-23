#include "rtc_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <time.h>

static SemaphoreHandle_t rtc_mutex;
static TickType_t rtc_max_block_time;
static uint32_t rtc_time;
static uint32_t rtc_offset;




void rtc_config_init(){
    rtc_max_block_time = pdMS_TO_TICKS(500);
    rtc_offset = 0;
    rtc_time = 0;
    rtc_mutex = xSemaphoreCreateMutex();
    //printf("TIME: configTICK_RATE_HZ %d\n", configTICK_RATE_HZ);
    //printf("TIME: RTC_CONFIG_FACTOR %d\n", RTC_CONFIG_FACTOR);
    if(rtc_mutex != NULL){
        xSemaphoreGive(rtc_mutex);
    }
}

uint32_t rtc_config_get_time(){
    uint32_t t;
    if(xSemaphoreTake(rtc_mutex, rtc_max_block_time ) == pdPASS){
        t = (xTaskGetTickCount() - rtc_offset)/RTC_CONFIG_NORM_FACTOR + rtc_time;        
    }
    else{
        return 0;
    }
    xSemaphoreGive(rtc_mutex);
    return t;
}

void rtc_config_set_time(uint32_t in_time){
    if(xSemaphoreTake(rtc_mutex, rtc_max_block_time) == pdPASS){
        rtc_offset = xTaskGetTickCount();
        rtc_time = in_time;
    }
    else{
        return;
    }
    xSemaphoreGive(rtc_mutex);
}

void rtc_config_set_time_(char * timeString, int len){
    uint32_t time_ = 0;
    sscanf(timeString, "%u", &time_);
    printf("HORA CONFIGU**** %u\n", time_);
    rtc_config_set_time(time_);
    //rtc_time = time_;
}

