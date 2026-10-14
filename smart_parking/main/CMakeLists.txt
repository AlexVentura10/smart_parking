idf_component_register(
    SRCS
        "main.c"
        "app_camera.c"
        "app_model.cpp"
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        esp_wifi
        nvs_flash
        esp_http_server
        esp_psram
        esp_hw_support
        espressif__esp32-camera
        espressif__mqtt
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=maybe-uninitialized)
set_property(SOURCE app_model.cpp APPEND PROPERTY COMPILE_OPTIONS -std=gnu++17)