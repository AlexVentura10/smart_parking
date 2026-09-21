#pragma once

#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa o sensor (SCCB) e o barramento DVP via componente esp32-camera.
// Framebuffer é alocado em PSRAM (CAMERA_FB_IN_PSRAM).
esp_err_t app_camera_init(void);

// Captura um frame. Retorna NULL em caso de falha.
// O buffer retornado DEVE ser liberado com app_camera_return().
camera_fb_t *app_camera_capture(void);

// Libera o framebuffer de volta para o driver.
void app_camera_return(camera_fb_t *fb);

#ifdef __cplusplus
}
#endif
