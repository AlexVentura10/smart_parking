idf_component_register(
    SRCS
        "main.c"
        "app_camera.c"
        "app_model.cpp"
    INCLUDE_DIRS
        "."
)

# TFLite Micro é C++; garanta C++17 no arquivo que inclui os headers do tflite-micro.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=maybe-uninitialized)
set_property(SOURCE app_model.cpp APPEND PROPERTY COMPILE_OPTIONS -std=gnu++17)
