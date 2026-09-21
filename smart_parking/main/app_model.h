#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

// Carrega o modelo, aloca a tensor arena na PSRAM e prepara o interpretador.
esp_err_t app_model_init(void);

// Executa uma inferência a partir de um frame capturado (qualquer formato
// suportado pelo esp32-camera; é convertido internamente para RGB888 e
// redimensionado para a entrada do modelo).
//
// out_class recebe o índice da classe vencedora (0 = livre, 1 = ocupada,
// pela convenção assumida do dataset CNRPark-EXT — confirme na prática).
// out_confidence recebe a probabilidade (0.0–1.0) dessa classe.
esp_err_t app_model_predict(camera_fb_t *fb, int *out_class, float *out_confidence);

#ifdef __cplusplus
}
#endif
