#pragma once
#include "esp_http_client.h"
#include "freertos/ringbuf.h"
#include <string_view>
#include <string>
#include <vector>

/**
* 
* @brief Provides the ability to send HTTP requests and receive responses
* 
* Provides HTTP request functionality (GET, POST, PUT) and response data from
* these requests. 
* 
*/
class HttpClient {
public:
    /**
     * @brief Constructor for HttpClient class. 
     *             
     */
    HttpClient();

    /**
     * @brief Destructor for HttpClient class. 
     *             
     */
    ~HttpClient();

    /**
     * @brief      Sets a HTTP header. This will create one if it's key does not exist. 
     *             
     *
     * @param[in]  key  The header key.
     * @param[in]  value The header value.
     *
     * @return
     *  - ESP_OK
     *  - ESP_FAIL
     */
    esp_err_t setHeader(std::string_view key, std::string_view value);

    /**
     * @brief      Deletes a HTTP header 
     *             
     *
     * @param[in]  key  The header key.
     * @param[in]  value The header value.
     *
     * @return
     *  - ESP_OK
     *  - ESP_FAIL
     */
    esp_err_t deleteHeader(std::string_view key);

    std::string getHeader(std::string_view key);

    bool get(std::string_view url, std::vector<char>& dst);
    
    bool post(std::string_view url, std::string_view data, std::vector<char>& dst);

    bool put(std::string_view url, std::vector<char>& dst);

    static esp_err_t event_handler_dummy(esp_http_client_event_t *evt);

    esp_err_t event_handler(esp_http_client_event_t *evt);

private:

    std::vector<char> get_content();

    esp_http_client_handle_t client;  ///< ESP client handle.

    RingbufHandle_t xRingbuffer;      ///< Ringbuffer to receive HTTP response data.

};