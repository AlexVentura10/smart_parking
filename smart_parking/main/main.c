#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include "esp_psram.h"
#include "img_converters.h" 

#include "app_camera.h"
#include "app_model.h"

// Defina aqui as credenciais da sua rede Wi-Fi
#define WIFI_SSID      "Mary"
#define WIFI_PASS      "mary0912"

static const char *TAG = "main";
static httpd_handle_t camera_httpd = NULL;

// Callback de eventos do Wi-Fi
static void event_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "A tentar religar ao Wi-Fi...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Ligado com sucesso! Endereço IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// Inicialização padrão do Wi-Fi Station no ESP-IDF
void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta concluído.");
}

// Handler para a página HTML principal que carrega a imagem corretamente no navegador
static esp_err_t index_handler(httpd_req_t *req) {
    const char* resp = "<html><head><title>ESP32 Cam Stream - Smart Parking</title></head>"
                       "<body style='background:#111; text-align:center; color:white; font-family:sans-serif;'>"
                       "<h2>ESP32-S3 Smart Parking - Camera Stream</h2>"
                       "<div style='margin-top:20px;'>"
                       "<img src='/stream' style='max-width:100%%; border:2px solid #444; border-radius:8px;'/>"
                       "</div>"
                       "</body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, resp, strlen(resp));
}

// Handler responsável por converter o frame RGB565 para JPEG e enviar para o navegador
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[64];
    static const char* _stream_content_type = "multipart/x-mixed-replace;boundary=123456789000000000000987654321";
    static const char* _stream_boundary = "\r\n--123456789000000000000987654321\r\n";
    static const char* _stream_type = "image/jpeg\r\nContent-Length: %u\r\n\r\n";

    res = httpd_resp_set_type(req, _stream_content_type);
    if (res != ESP_OK) return res;

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGW(TAG, "Falha ao capturar frame para o stream");
            res = ESP_FAIL;
            break;
        }

        uint8_t *jpg_buf = NULL;
        size_t jpg_len = 0;

        // Converte o buffer RGB565 para JPEG para o navegador conseguir renderizar
        bool jpeg_converted = fmt2jpg(fb->buf, fb->len, fb->width, fb->height, PIXFORMAT_RGB565, 80, &jpg_buf, &jpg_len);
        
        // Devolvemos o framebuffer imediatamente para liberar para o modelo de inferência
        esp_camera_fb_return(fb);

        if (!jpeg_converted) {
            ESP_LOGE(TAG, "Falha ao converter RGB565 para JPEG");
            res = ESP_FAIL;
            break;
        }

        size_t hlen = snprintf(part_buf, 64, _stream_type, jpg_len);
        res = httpd_resp_send_chunk(req, _stream_boundary, strlen(_stream_boundary));
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_len);
        }

        free(jpg_buf); // Liberta o buffer JPEG temporário

        if (res != ESP_OK) break;
    }
    return res;
}

// Inicia o servidor HTTP do ESP-IDF registrando as rotas '/' e '/stream'
void start_camera_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &stream_uri);
        ESP_LOGI(TAG, "Servidor web de streaming iniciado na porta 80.");
    }
}

void app_main(void)
{
    // 1. Inicializar memória NVS (necessária para o controlador Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND || ret == ESP_ERR_NVS_NO_FREE_PAGES) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "PSRAM disponivel: %u bytes", (unsigned int)esp_psram_get_size());

    for (int i = 3; i > 0; i--) {
        ESP_LOGI(TAG, "A iniciar em %d...", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 2. Ligar ao Wi-Fi configurado acima
    wifi_init_sta();

    // 3. Inicializar câmara e modelo
    ESP_ERROR_CHECK(app_camera_init());

    if (app_model_init() != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar o modelo. A abortar.");
        return;
    }

    // 4. Iniciar servidor web para ver o fluxo de vídeo
    start_camera_server();

    ESP_LOGI(TAG, "Sistema pronto. A iniciar ciclo de inferencia.");

    while (1) {
        camera_fb_t *fb = app_camera_capture();
        if (fb == NULL) {
            ESP_LOGW(TAG, "Falha ao capturar frame");
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        int predicted_class = -1;
        float confidence = 0.0f;
        esp_err_t res = app_model_predict(fb, &predicted_class, &confidence);

        if (res == ESP_OK) {
            const char *status = (predicted_class == 0) ? "OCUPADA" : "LIVRE";
            ESP_LOGI(TAG, "Vaga: %s (classe=%d, confianca=%.3f)",
                     status, predicted_class, confidence);
        } else {
            ESP_LOGW(TAG, "Inferencia falhou (0x%x)", res);
        }

        app_camera_return(fb);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}