#pragma once

// ---------------------------------------------------------------------------
// Pinout para o clone "ESP32-S3-CAM" com header no layout do
// ESP32-S3-DevKitC-1 (dual USB-C, módulo N16R8, câmera OV3660 via
// conector FPC) — identificado pela foto da sua placa.
//
// Fonte: mapeamento de pinos padrão usado pela comunidade para esse
// clone específico (macro CAMERA_MODEL_ESP32_S3_CAM, presente em
// diversos firmwares open-source para essa mesma placa). Todos os
// pinos abaixo estão fora da faixa GPIO 26–37, reservada internamente
// pelo PSRAM octal — então não deve haver conflito com o PSRAM.
//
// Se ainda assim o firmware travar após "PSRAM enabled", isso descarta
// a hipótese de conflito de pino e aponta para outra causa (ver
// README.md, seção de diagnóstico).
// ---------------------------------------------------------------------------

#define CAM_PIN_PWDN    -1   // não usado nesta placa
#define CAM_PIN_RESET   -1   // não usado nesta placa

#define CAM_PIN_XCLK    15

#define CAM_PIN_SIOD    4    // SDA (I2C/SCCB)
#define CAM_PIN_SIOC    5    // SCL (I2C/SCCB)

#define CAM_PIN_D0      11   // Y2
#define CAM_PIN_D1      9    // Y3
#define CAM_PIN_D2      8    // Y4
#define CAM_PIN_D3      10   // Y5
#define CAM_PIN_D4      12   // Y6
#define CAM_PIN_D5      18   // Y7
#define CAM_PIN_D6      17   // Y8
#define CAM_PIN_D7      16   // Y9

#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13

// LED RGB (WS2812) onboard nesse clone, se presente na sua unidade.
// Não usado neste projeto, mas deixado aqui de referência.
#define BOARD_WS2812_GPIO_NUM 48
