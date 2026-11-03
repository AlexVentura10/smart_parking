#pragma once

#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif


// Retorna ESP_OK em caso de sucesso, ou código de erro do driver em caso de falha.
esp_err_t app_camera_init(void);


// O ponteiro retornado é válido até a próxima chamada de app_camera_capture() ou app_camera_return().
camera_fb_t *app_camera_capture(void);

// Libera o framebuffer de volta para o driver.
void app_camera_return(camera_fb_t *fb);


#ifdef __cplusplus
}
#endif
