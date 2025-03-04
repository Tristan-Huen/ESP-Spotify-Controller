idf_component_register(SRCS "main.cpp" "wifi.cpp" "http_client.cpp" "spotify_client.cpp"
                       INCLUDE_DIRS "../include")

idf_build_set_property(COMPILE_OPTIONS "-Wno-missing-field-initializers" APPEND)
