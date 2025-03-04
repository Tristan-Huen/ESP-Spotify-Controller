#pragma once
#include <string>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http_client.h"

#define API_BODY "grant_type=refresh_token&refresh_token=" CONFIG_REFRESH_TOKEN
#define API_AUTH CONFIG_CLIENT_ID ":" CONFIG_CLIENT_SECRET
#define AUTH_ENC_LEN 89 // 4 * ceil(N/3) + 1
#define AUTH_FULL_BODY AUTH_ENC_LEN + 5

namespace spotify {

    //See the following link for explanations of each code: https://developer.spotify.com/documentation/web-api/concepts/api-calls
    enum class StatusCode {
        Ok =                  200,
        Created =             201,
        Accepted =            202,
        NoContent =           204,
        NotModified =         304,
        BadRequest =          400,
        Unauthorized =        401,
        Forbidden =           403,
        NotFound =            404,
        TooManyRequests =     429,
        InternalServerError = 500,
        BadGateway =          502,
        ServiceUnavailable =  503,
        GatewayTimeout =      504
    };

    enum class PlayState {
        Playing,
        Paused
    };

    enum class ShuffleState {
        On,
        Off
    };

    enum class RepeatState {
        Off,
        Track,
        Context
    };

    enum class Command {
        Play,
        Pause,
        SkipNext,
        SkipPrev,
        ShuffleOn,
        ShuffleOff,
        RepeatContext,
        RepeatTrack,
        RepeatOff
    };

    struct Track {
        std::string name;                 ///< The track name.
        std::string album_name;           ///< The name of the track's album.
        std::string album_pic_url;        ///< A URL to a JPG of the track's album.
        std::vector<std::string> artists; ///< A list of artists on the track.
        int duration_ms;                  ///< The duration of the track.
        int progress_ms;                  ///< The current progress into the track.
        std::string uri;                  ///< The track's url.
        int response_code;                ///< The HTTP response code.
    };

    class Client {
    public:
        Client();
        ~Client();

        Track getCurrentlyPlaying();

        /**
         * @brief           Sends a command to the player.
         *                  
         * 
         * @param[in]  cmd  The command to send. 
         *
         * @return
         *  - True if successful
         *  - False otherwise
         */
        bool sendPlayerCommand(Command cmd);


        /**
         * @brief Resumes playing of paused song. 
         *             
         * @return
         *  - True if successful
         *  - False otherwise
         */
        bool play();

        /**
         * @brief Pauses song.   
         *             
         * @return
         *  - True if successful
         *  - False otherwise
         */
        bool pause();

        /**
         * @brief Skips to the next song.   
         *
         * @return
         *  - True if successful
         *  - False otherwise
         */
        bool skipToNextSong();

        /**
         * @brief Skips to the previous song.   
         *
         * @return
         *  - True if successful
         *  - False otherwise
         */
        bool skipToPrevSong();

        /**
         * @brief Toggles the shuffle state.   
         *
         * @return
         *  - True if successful
         *  - False otherwise
         */
        bool toggleShuffle();

        /**
         * @brief Toggles the shuffle state.
         *          
         *        The order is Off, Context, and Track. Toggling at state Track wraps back to state Off.
         * 
         * @return
         *  - True if successful
         *  - False otherwise
         */
        bool toggleRepeat();

        /**
         * @brief  Gets the current play state.
         *          
         * @return The current play state.
         */
        PlayState getPlayState();

        /**
         * @brief Gets the current repeat state.
         *          
         * @return The current repeat state.
         */
        RepeatState getRepeatState();

        /**
         * @brief Gets the current shuffle state.
         *          
         * @return The current shuffle state.
         */ 
        ShuffleState getShuffleState();

        static void get_access_token_task_dummy(void *arg);
        void get_access_token_task();

    private:
        
        PlayState play_state;         ///< The player's state.
        RepeatState repeat_state;     ///< The repeat state.
        ShuffleState shuffle_state;   ///< The shuffle state.
        std::string access_token;     ///< The access token,
        HttpClient token_http_client; ///< HttpClient for access token.
        HttpClient api_http_client;
        SemaphoreHandle_t mtx_token;  ///< Mutex for the access token.
    };

}

