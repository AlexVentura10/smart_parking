#include "app_camera.h"
#include "camera_pins.h"
#include "esp_log.h"

static const char *TAG = "app_camera";

esp_err_t app_camera_init(void)
{
    camera_config_t config = {
        .pin_pwdn     = CAM_PIN_PWDN,
        .pin_reset    = CAM_PIN_RESET,
        .pin_xclk     = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,

        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href  = CAM_PIN_HREF,
        .pin_pclk  = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,
        .ledc_timer   = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        // O modelo espera entrada RGB de 3 canais (confirmado ao inspecionar
        // o .tflite), então capturamos em RGB565 — depois convertida para
        // RGB888 e redimensionada em app_model.cpp antes da inferência.
        .pixel_format = PIXFORMAT_RGB565,
        .frame_size   = FRAMESIZE_QQVGA,   // 160x120

        .jpeg_quality = 12,                // ignorado fora de PIXFORMAT_JPEG
        .fb_count     = 2,
        .fb_location  = CAMERA_FB_IN_PSRAM,
        .grab_mode    = CAMERA_GRAB_LATEST,
    };

    // O sensor da sua placa é o OV3660 (visível na etiqueta do módulo FPC).
    // O componente esp32-camera detecta o modelo automaticamente via SCCB
    // (lê o registro de ID do sensor), então não é preciso indicar qual
    // sensor é — só falha aqui se a fiação/pinout estiver errada.
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init falhou: 0x%x", err);
        return err;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor != NULL) {
        ESP_LOGI(TAG, "Sensor detectado, PID=0x%x", sensor->id.PID);
    }

    ESP_LOGI(TAG, "Câmera inicializada: QQVGA 160x120, RGB565, framebuffer em PSRAM");
    return ESP_OK;
}

camera_fb_t *app_camera_capture(void)
{
    return esp_camera_fb_get();
}

void app_camera_return(camera_fb_t *fb)
{
    if (fb) {
        esp_camera_fb_return(fb);
    }
}
