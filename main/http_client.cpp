#include "http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char* TAG = "HttpClient";

std::vector<char> HttpClient::get_content() {
    int bufferSize = esp_http_client_get_content_length(client);
    std::vector<char> response_data(bufferSize+1);

    size_t item_size;
    int index = 0;

    ESP_LOGI(TAG,"Receiving item of size %d", bufferSize);

    while(1) {
        char *item = (char*)xRingbufferReceive(xRingbuffer,&item_size,pdMS_TO_TICKS(1000));
        if(item != nullptr) {
            for(int i = 0 ; i< item_size; i++) {
                response_data[index] = item[i];
                index++;
                response_data[index] = 0;
            }
            vRingbufferReturnItem(xRingbuffer,static_cast<void*>(item));
        }

        else {
            ESP_LOGI(TAG,"End of receive item");
            return response_data;
        }
    }
}

esp_err_t HttpClient::event_handler_dummy(esp_http_client_event_t *evt) {
    auto obj = static_cast<HttpClient*>(evt->user_data);
    return obj->event_handler(evt);
}

esp_err_t HttpClient::event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG,"HTTP_EVENT_ON_DATA");

        if(!esp_http_client_is_chunked_response(evt->client)) {
            ESP_LOGI(TAG,"Length of event data: %d",evt->data_len);

            char *buffer = static_cast<char*>(malloc(evt->data_len+1));

            memcpy(buffer,evt->data,evt->data_len);
            buffer[evt->data_len] = 0;

            UBaseType_t res = xRingbufferSend(xRingbuffer,buffer,evt->data_len,pdMS_TO_TICKS(1000));

            if(res != pdTRUE) {
                ESP_LOGE(TAG,"xRingbufferSendFromISR res=%d",res);
            }
            free(buffer);
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG,"HTTP_EVENT_ON_FINISH");
        break;
    default:
        break;
    }
    return ESP_OK;
}

HttpClient::HttpClient() {

    xRingbuffer = xRingbufferCreate(8192,RINGBUF_TYPE_NOSPLIT);
    
    if(xRingbuffer == NULL) {
        ESP_LOGE(TAG, "Couldn't create ring buffer");
    }

    //Create with some dummy data.
    esp_http_client_config_t config = {
        .url = "https://www.google.com/",
        .cert_pem = NULL,
        .method = HTTP_METHOD_GET,
        .event_handler = event_handler_dummy,
        .user_data = this
    };

    client = esp_http_client_init(&config);
}

HttpClient::~HttpClient() {
    esp_http_client_cleanup(client);
    vRingbufferDelete(xRingbuffer);
}

esp_err_t HttpClient::setHeader(std::string_view key, std::string_view value) {
    return esp_http_client_set_header(client,key.data(),value.data());
}

esp_err_t HttpClient::deleteHeader(std::string_view key) {
    return esp_http_client_delete_header(client,key.data());
}

std::string HttpClient::getHeader(std::string_view key) {
    char* value = nullptr;

    ESP_ERROR_CHECK(esp_http_client_get_header(client,key.data(),&value));

    if(value == nullptr) {
        return "";
    }

    else {
        return value;
    }
}

bool HttpClient::get(std::string_view url, std::vector<char>& dst) {

    esp_http_client_set_url(client,url.data());
    esp_http_client_set_method(client,HTTP_METHOD_GET);

    esp_err_t err = esp_http_client_perform(client);

    int status_code = esp_http_client_get_status_code(client);

    if(err != ESP_OK) {
        ESP_LOGE(TAG,"HTTP GET request failed: %s", esp_err_to_name(err));
        return false;
    }

    else if(status_code != HttpStatus_Ok) {
        ESP_LOGE(TAG,"HTTP status error, code: %d", status_code);
        return false;
    }

    else {
        dst = get_content();
        return true;
    }

}

bool HttpClient::post(std::string_view url, std::string_view data, std::vector<char>& dst) {
    esp_http_client_set_url(client,url.data());
    esp_http_client_set_method(client,HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, data.data(), data.size());

    esp_err_t err = esp_http_client_perform(client);

    int status_code = esp_http_client_get_status_code(client);

    if(err != ESP_OK) {
        ESP_LOGE(TAG,"HTTP POST request failed: %s", esp_err_to_name(err));
        return false;
    }

    else if(status_code != HttpStatus_Ok) {
        ESP_LOGE(TAG,"HTTP status error, code: %d", status_code);
        return false;
    }

    else {
        dst = get_content();
        return true;
    }
}

bool HttpClient::put(std::string_view url, std::vector<char>& dst) {
    esp_http_client_set_url(client,url.data());
    esp_http_client_set_method(client,HTTP_METHOD_PUT);

    esp_err_t err = esp_http_client_perform(client);

    int status_code = esp_http_client_get_status_code(client);

    if(err != ESP_OK) {
        ESP_LOGE(TAG,"HTTP PUT request failed: %s", esp_err_to_name(err));
        return false;
    }

    else if(status_code != HttpStatus_Ok) {
        ESP_LOGE(TAG,"HTTP status error, code: %d", status_code);
        return false;
    }

    else {
        dst = get_content();
        return true;
    }

}