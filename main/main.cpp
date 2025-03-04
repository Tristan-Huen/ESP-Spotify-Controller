#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"

#define LGFX_USE_V1
#include "../include/panel.h"
#include "lvgl.h"
#include "../include/spotify_client.h"
#include "../include/wifi.h"
#include "../include/http_client.h"

static const char *TAG = "main";
LGFX tft;

static lv_display_t *disp = nullptr;
static lv_indev_t *indev = nullptr;
static lv_color16_t *lv_buf_1 = nullptr;
static lv_color16_t *lv_buf_2 = nullptr;

static TaskHandle_t player_task_handle;

static void lv_tick_task(void *arg) {
    (void) arg;

    lv_tick_inc(1);
}

/* Display flushing */
void my_disp_flush( lv_display_t *disp, const lv_area_t *area, uint8_t* data) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    // lv_draw_sw_rgb565_swap(data, w*h);

    // if (tft.getStartCount() == 0)
    // {   // Processing if not yet started
    //      tft.startWrite();
    // }

    // tft.pushImageDMA( area->x1
    //     , area->y1
    //     , area->x2 - area->x1 + 1
    //     , area->y2 - area->y1 + 1
    //     ,(uint16_t*) data); 

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixels((lgfx::rgb565_t *)data, w * h);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

void touch_driver_read(lv_indev_t *indev, lv_indev_data_t *data) {
    uint16_t touchX, touchY;
    bool touched = tft.getTouch( &touchX, &touchY);

    if( !touched ) {
        data->state = LV_INDEV_STATE_REL;
    }

    else {
        data->state = LV_INDEV_STATE_PR;

        data->point.x = touchX;
        data->point.y = touchY;
    }

    data->continue_reading = false;
}

static void btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        xTaskNotifyGive(player_task_handle);
    }
}

static void lvgl_timer_task(void* arg) {
    while(1) {
        uint32_t time_till_next;
        time_till_next = lv_timer_handler(); /* lv_lock/lv_unlock is called internally */
        vTaskDelay(pdMS_TO_TICKS(time_till_next));
    }
}

static void player_task(void* arg) {

    auto client = static_cast<spotify::Client*>(arg);

    while(1) {
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);

        client->skipToNextSong();

        vTaskDelay(pdTICKS_TO_MS(10));
    }
}

extern "C" void app_main() {

    constexpr int screen_width = 480;
    constexpr int screen_height = 320;
    constexpr int lv_buffer_size = screen_width * screen_height/10;
    
    Wifi wifi_sta;

    wifi_sta.init();

    ESP_ERROR_CHECK(wifi_sta.connect());

    spotify::Client client;

    tft.begin();
    tft.setRotation(1);
    tft.setBrightness(255);
    uint16_t calData[] = {191, 3913, 207, 289, 3748, 3901, 3799, 256};
    tft.setTouchCalibrate(calData);

    lv_buf_1 = (lv_color16_t *)heap_caps_malloc(lv_buffer_size * sizeof(lv_color16_t), MALLOC_CAP_DMA);
    lv_buf_2 = (lv_color16_t *)heap_caps_malloc(lv_buffer_size * sizeof(lv_color16_t), MALLOC_CAP_DMA);

    lv_init();
    disp = lv_display_create(screen_width,screen_height);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, lv_buf_1, lv_buf_2, lv_buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL );

    indev = lv_indev_create();
    lv_indev_set_type(indev,LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev,touch_driver_read);

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000));
    
    xTaskCreatePinnedToCore(player_task,
                            "Player Control",
                            4096,
                            &client,
                            2,
                            &player_task_handle,
                            0);

    xTaskCreatePinnedToCore(lvgl_timer_task,
        "LVGL Timer",
        4096,
        nullptr,
        2,
        nullptr,
        0);

    lv_lock();
    lv_obj_t * btn = lv_button_create(lv_screen_active());     /*Add a button the current screen*/
    lv_obj_align(btn, LV_ALIGN_CENTER,0,0);                         /*Set its position*/
    lv_obj_set_size(btn, 120, 50);                          /*Set its size*/
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, &client);           /*Assign a callback to the button*/

    lv_obj_t * label = lv_label_create(btn);          /*Add a label to the button*/
    lv_label_set_text(label, "Button");                     /*Set the labels text*/
    lv_obj_center(label);
    lv_unlock();

    TickType_t api_request_time;

    while(1) {

        // api_request_time = xTaskGetTickCount();

        // ESP_LOGI(TAG, "Free Heap Space %u", (unsigned int)esp_get_free_heap_size());

        // spotify::Track track = client.getCurrentlyPlaying();

        // printf("Track Name: %s\n", track.name.c_str());
        // printf("Album Name: %s\n", track.album_name.c_str());
        // printf("Artists: ");
    
        // for(const auto &artist: track.artists) {
        //     printf("%s, ", artist.c_str());
        // }
    
        // printf("\nProgress %dms\n",track.progress_ms);
        // printf("Duration %dms\n",track.duration_ms);
        // printf("Album Pic: %s\n", track.album_pic_url.c_str());

        // vTaskDelayUntil(&api_request_time,pdMS_TO_TICKS(1000));
        // uint32_t time_till_next;
        // time_till_next = lv_timer_handler(); /* lv_lock/lv_unlock is called internally */
        // vTaskDelay(pdMS_TO_TICKS(time_till_next));
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
