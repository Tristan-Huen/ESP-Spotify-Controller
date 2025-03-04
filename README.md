# ESP-Spotify-Controller
This is a Spotify controller running on an ESP32-S3, and written in C++20 using esp-idf, LVGL, and LovyanGFX. It uses a ILI9488 (with touch screen) to display the currently playing track and provides the standard controls of pause/resume, skip, and shuffle. Currently this project is a work in progress.

## Configuration
In the esp-idf menuconfig is a section called `Spotify Configuration` where the user must set the SSID, WIFI password, and Spotify API token info. As of the moment it is not automated so one will need to consult the Spotify Web API page for this.