#pragma once
#include "esp_wifi.h"

class Wifi {
public:
    Wifi();

    void init();
    esp_err_t connect();
};