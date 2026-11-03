#include "app_camera.h"
#include "camera_pins.h"
#include "esp_log.h"

static const char *TAG = "app_camera"; // Tag para mensagens de log relacionadas à câmera.

esp_err_t app_camera_init(void)  // Inicializa a câmera com configuração padrão.
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

        // O modelo espera entrada em escala de cinza (1 canal).
        .pixel_format = PIXFORMAT_GRAYSCALE, // Mantém 1 canal (escala de cinza) para reduzir o uso de memória e largura de banda.
        .frame_size   = FRAMESIZE_QVGA,   // Mantém 320x240 (ou ajusta conforme o recorte de ROI)

        .jpeg_quality = 12,                // Qualidade JPEG (não usado em GRAYSCALE, mas necessário para inicialização)
        .fb_count     = 2,
        .fb_location  = CAMERA_FB_IN_PSRAM,
        .grab_mode    = CAMERA_GRAB_LATEST,
    };

    // Inicializa a câmera com a configuração especificada do sensor (OV3660).
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init falhou: 0x%x", err);
        return err;
    }
    
    // Obtém o sensor e imprime o PID (identificador único) para verificação. 
    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor != NULL) {
        ESP_LOGI(TAG, "Sensor detectado, PID=0x%x", sensor->id.PID);
    }

    ESP_LOGI(TAG, "Câmera inicializada: QVGA 320x240, GRAYSCALE (1 canal), framebuffer em PSRAM");
    return ESP_OK;
}

// Captura uma imagem da câmera e retorna um ponteiro para o framebuffer (espaço de memória onde a imagem é armazenada).
camera_fb_t *app_camera_capture(void)
{
    return esp_camera_fb_get();
}

// Retorna o framebuffer para a câmera, liberando a memória associada.
void app_camera_return(camera_fb_t *fb)
{
    if (fb) {
        esp_camera_fb_return(fb);
    }
}