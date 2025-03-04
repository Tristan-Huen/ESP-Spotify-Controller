#include "spotify_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "cJSON.h"
#include "base64.h"
#include <array>
#include <stdarg.h>

// TO-DO:
// 1) Add logic to handle updated access token for static objects.
// 2) Make sendCommand reentrant.
// 3) Get initial state of player upon construction.
// 4) Add more functions.

#define API_BODY "grant_type=refresh_token&refresh_token=" CONFIG_REFRESH_TOKEN
#define API_AUTH CONFIG_CLIENT_ID ":" CONFIG_CLIENT_SECRET

static const char* TAG = "SpotifyClient";

static cJSON* JSON_GetItemFromPath(cJSON *root, ...) {
    va_list args;
    va_start(args, root);

    cJSON *current_element = root;

    do {
        if (current_element == NULL) {
            va_end(args);
            return NULL;
        }

        if (current_element->type == cJSON_Array) {
            int index = va_arg(args, int);
            current_element = cJSON_GetArrayItem(current_element, index);
        } 
        
        else if (current_element->type == cJSON_Object) {
            const char *key = va_arg(args, const char *);
            current_element = cJSON_GetObjectItem(current_element, key);
        } 
        
        else {
            va_end(args);
            return NULL;
        }

    } while(current_element->child != NULL); 

    va_end(args);
    return current_element;
}

namespace spotify {
    Client::Client() {
        std::vector<char> buff;
        constexpr auto auth_enc = base64::encode(API_AUTH);
        constexpr std::string_view auth_part{"Basic "};

        mtx_token = xSemaphoreCreateMutex();
        
        //Concatenation
        constexpr auto auth = [&] {
            std::array<char, auth_enc.size()+auth_part.size()> out{};
            std::copy(auth_part.begin(), auth_part.end(), out.begin());
            std::copy(auth_enc.begin(),auth_enc.end(),out.begin() + auth_part.size());

            return out;
        }();

        token_http_client.setHeader("Content-Type", "application/x-www-form-urlencoded");
        token_http_client.setHeader("Authorization", auth.data());

        bool success = token_http_client.post("https://accounts.spotify.com/api/token",API_BODY, buff);
        
        if(!success || buff.empty()) {
            ESP_LOGE(TAG,"HTTP POST for access token failed");
        }

        else {
            cJSON *root = cJSON_Parse(buff.data());
            cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "access_token");
            access_token = std::string{item->valuestring};

            ESP_LOGI(TAG,"Access Token: %s",access_token.c_str());
            cJSON_Delete(root);

            xTaskCreatePinnedToCore(  
                get_access_token_task_dummy,    // Function to be called
                "Get Access Token",             // Name of task
                4096,                           // Stack size (bytes in ESP32, words in FreeRTOS)
                this,                           // Parameter to pass
                3,                              // Task priority
                nullptr,                        // Task handle
                0);                             // Core affinity
        }

    }

    Client::~Client() {

    }

    Track Client::getCurrentlyPlaying() {
        static bool first = true;
        static HttpClient http_client;
        std::vector<char> buff;
        Track track;

        xSemaphoreTake(mtx_token,portMAX_DELAY);
        std::string bearer = "Bearer " + access_token;
        xSemaphoreGive(mtx_token);
        
        http_client.setHeader("Authorization",bearer);

        bool success = http_client.get("https://api.spotify.com/v1/me/player/currently-playing",buff);

        if(!success) {
            ESP_LOGE(TAG,"HTTP GET for current play failed");
            return track;
        }

        else {
            ESP_LOGI(TAG, "Parsing data");

            //Should write a JSON class that moves the vector.
            cJSON *root = cJSON_Parse(buff.data());

            cJSON *item = cJSON_GetObjectItemCaseSensitive(root,"item");

            cJSON *track_name_obj = cJSON_GetObjectItemCaseSensitive(item,"name");

            cJSON *album_obj = cJSON_GetObjectItemCaseSensitive(item,"album");

            cJSON *album_name_obj = cJSON_GetObjectItemCaseSensitive(album_obj,"name");

            cJSON *album_pic_obj = JSON_GetItemFromPath(album_obj,"images",1,"url");

            cJSON *duration_ms_obj =  cJSON_GetObjectItemCaseSensitive(item,"duration_ms");

            cJSON *progress_ms_obj = cJSON_GetObjectItemCaseSensitive(root,"progress_ms");

            cJSON *artists_obj = cJSON_GetObjectItemCaseSensitive(item,"artists"); 

            cJSON *is_playing_obj = cJSON_GetObjectItemCaseSensitive(root,"is_playing");

            const cJSON *artist = nullptr;

            //Always an array even if there's only one object.
            cJSON_ArrayForEach(artist, artists_obj) {
                cJSON *artist_name_obj = cJSON_GetObjectItemCaseSensitive(artist, "name");

                track.artists.emplace_back(artist_name_obj->valuestring);
            }

            track.progress_ms = static_cast<int>(progress_ms_obj->valuedouble);
            track.duration_ms = static_cast<int>(duration_ms_obj->valuedouble);
            track.name = track_name_obj->valuestring;
            track.album_name = album_name_obj->valuestring;

            if(cJSON_IsNull(album_pic_obj)) {
                track.album_pic_url = "";
            }

            else {
                track.album_pic_url = album_pic_obj->valuestring;
            }

            bool is_playing = cJSON_IsTrue(is_playing_obj) == cJSON_True;
            
            play_state = is_playing ? PlayState::Playing : PlayState::Paused;

            cJSON_Delete(root);

            return track;
        }
    }

    bool Client::sendPlayerCommand(Command cmd) {
        static HttpClient http_client; 
        std::vector<char> buff;
        bool success = false;

        xSemaphoreTake(mtx_token,portMAX_DELAY);
        std::string bearer = "Bearer " + access_token;
        xSemaphoreGive(mtx_token);
        
        http_client.setHeader("Authorization", bearer);

        switch (cmd) {
            case Command::Play:
                success = http_client.post("https://api.spotify.com/v1/me/player/play","",buff);
                play_state = success ? PlayState::Playing : play_state;
                return success;
            case Command::Pause:
                success = http_client.post("https://api.spotify.com/v1/me/player/pause","",buff);
                play_state = success ? PlayState::Paused : play_state;
                return success;
            case Command::SkipNext:
                success = http_client.post("https://api.spotify.com/v1/me/player/next","",buff);
                return success;
            case Command::SkipPrev:
                success = http_client.post("https://api.spotify.com/v1/me/player/previous","",buff);
                return success;
            case Command::ShuffleOn:
                success = http_client.put("https://api.spotify.com/v1/me/player/shuffle?state=true",buff);
                shuffle_state = success ? ShuffleState::On : shuffle_state;
                return success;
            case Command::ShuffleOff:
                success = http_client.put("https://api.spotify.com/v1/me/player/shuffle?state=false",buff);
                shuffle_state = success ? ShuffleState::Off : shuffle_state;
                return success;
            case Command::RepeatContext:
                success = http_client.put("https://api.spotify.com/v1/me/player/repeat?state=context",buff);
                repeat_state = success ? RepeatState::Context : repeat_state;
                return success;
            case Command::RepeatTrack:
                success = http_client.put("https://api.spotify.com/v1/me/player/repeat?state=track",buff);
                repeat_state = success ? RepeatState::Track : repeat_state;
                return success;
            case Command::RepeatOff:
                success = http_client.put("https://api.spotify.com/v1/me/player/repeat?state=off",buff);
                repeat_state = success ? RepeatState::Off : repeat_state;
                return success;
            default:
                return success;
        }
    }

    bool Client::play() {
        return sendPlayerCommand(Command::Play);
    }

    bool Client::pause() {
        return sendPlayerCommand(Command::Pause);
    }

    bool Client::skipToNextSong() {
        return sendPlayerCommand(Command::SkipNext);
    }

    bool Client::skipToPrevSong() {
        return sendPlayerCommand(Command::SkipPrev);
    }

    bool Client::toggleShuffle() {
        if (shuffle_state == ShuffleState::On) {
            return sendPlayerCommand(Command::ShuffleOff);
        }

        else {
            return sendPlayerCommand(Command::ShuffleOn);
        }
    }

    bool Client::toggleRepeat() {
        switch (repeat_state) {
            case RepeatState::Off:
                return sendPlayerCommand(Command::RepeatContext);
            case RepeatState::Context:
                return sendPlayerCommand(Command::RepeatTrack);
            case RepeatState::Track:
                return sendPlayerCommand(Command::RepeatOff);
            default:
                return false;
        }
    }

    PlayState Client::getPlayState() {
        return play_state;
    }

    RepeatState Client::getRepeatState() {
        return repeat_state;
    }

    ShuffleState Client::getShuffleState() {
        return shuffle_state;
    }

    void Client::get_access_token_task_dummy(void* arg) {
        auto obj = static_cast<Client*>(arg);
        obj->get_access_token_task();
    }

    void Client::get_access_token_task() {
        std::vector<char> buff;
        constexpr int refresh_time_ms = 3500000;

        while(1) {
            vTaskDelay(pdMS_TO_TICKS(refresh_time_ms));

            bool success = token_http_client.post("https://accounts.spotify.com/api/token",API_BODY, buff);

            if(!success || buff.empty()) {
                ESP_LOGE(TAG,"HTTP POST for access token failed");
            }

            else {
                cJSON *root = cJSON_Parse(buff.data());
    
                cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "access_token");

                xSemaphoreTake(mtx_token,portMAX_DELAY);
                access_token = item->valuestring;
                xSemaphoreGive(mtx_token);

                ESP_LOGI(TAG, "Grabbed Access Token");

                cJSON_Delete(root);
            }

        }
    }
}