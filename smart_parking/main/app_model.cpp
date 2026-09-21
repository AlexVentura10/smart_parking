#include "app_model.h"
#include "model_pklot.h"

#include <math.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "img_converters.h"   // fmt2rgb888() — fornecido pelo componente esp32-camera

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

static const char *TAG = "app_model";

// Dimensões confirmadas ao inspecionar o .tflite (tensor de entrada [1,50,50,3]).
#define MODEL_INPUT_WIDTH    50
#define MODEL_INPUT_HEIGHT   50
#define MODEL_INPUT_CHANNELS 3

static constexpr int kTensorArenaSize = 96 * 1024;
static uint8_t *tensor_arena = nullptr;

static const tflite::Model *model = tflite::GetModel(model_tflite);
static tflite::MicroInterpreter *interpreter = nullptr;
static TfLiteTensor *input_tensor = nullptr;
static TfLiteTensor *output_tensor = nullptr;

// Ops confirmadas na sequência real do grafo:
// QUANTIZE -> [CONV_2D -> MAX_POOL_2D] x5 -> CONV_2D -> MEAN ->
// FULLY_CONNECTED -> SOFTMAX -> QUANTIZE (dequant. de saída)
static tflite::MicroMutableOpResolver<6> resolver;

// Buffer intermediário RGB888 (alocado sob demanda, tamanho do frame da câmera).
static uint8_t *rgb888_buf = nullptr;
static size_t rgb888_buf_size = 0;

// Resize nearest-neighbor para dados de 3 canais intercalados (R,G,B).
static void resize_rgb888(const uint8_t *src, int src_w, int src_h,
                           uint8_t *dst, int dst_w, int dst_h)
{
    for (int y = 0; y < dst_h; y++) {
        int sy = (y * src_h) / dst_h;
        for (int x = 0; x < dst_w; x++) {
            int sx = (x * src_w) / dst_w;
            const uint8_t *sp = src + (sy * src_w + sx) * 3;
            uint8_t *dp = dst + (y * dst_w + x) * 3;
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
        }
    }
}

esp_err_t app_model_init(void)
{
    tensor_arena = static_cast<uint8_t *>(
        heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM));
    if (tensor_arena == nullptr) {
        ESP_LOGE(TAG, "Falha ao alocar tensor arena (%d bytes) na PSRAM", kTensorArenaSize);
        return ESP_ERR_NO_MEM;
    }

    model = tflite::GetModel(model_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Schema do modelo incompatível: %lu (esperado %d)",
                 (unsigned long)model->version(), TFLITE_SCHEMA_VERSION);
        return ESP_FAIL;
    }

    resolver.AddQuantize();
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddMean();
    resolver.AddFullyConnected();
    resolver.AddSoftmax();

    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors() falhou — considere aumentar kTensorArenaSize");
        return ESP_FAIL;
    }

    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);

    ESP_LOGI(TAG, "Modelo carregado. Entrada: %dx%dx%d (tipo=%d), saída: %d classes (tipo=%d)",
             input_tensor->dims->data[1],
             input_tensor->dims->data[2],
             input_tensor->dims->data[3],
             input_tensor->type,
             output_tensor->dims->data[1],
             output_tensor->type);

    return ESP_OK;
}

esp_err_t app_model_predict(camera_fb_t *fb, int *out_class, float *out_confidence)
{
    if (interpreter == nullptr || input_tensor == nullptr || output_tensor == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (fb == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    // (Re)aloca o buffer RGB888 do tamanho do frame, se necessário.
    size_t needed = (size_t)fb->width * fb->height * 3;
    if (rgb888_buf == nullptr || rgb888_buf_size < needed) {
        if (rgb888_buf) {
            heap_caps_free(rgb888_buf);
        }
        rgb888_buf = static_cast<uint8_t *>(heap_caps_malloc(needed, MALLOC_CAP_SPIRAM));
        if (rgb888_buf == nullptr) {
            ESP_LOGE(TAG, "Falha ao alocar buffer RGB888 (%u bytes)", (unsigned)needed);
            return ESP_ERR_NO_MEM;
        }
        rgb888_buf_size = needed;
    }

    if (!fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_buf)) {
        ESP_LOGE(TAG, "fmt2rgb888() falhou (formato do frame: %d)", fb->format);
        return ESP_FAIL;
    }

    static uint8_t resized[MODEL_INPUT_WIDTH * MODEL_INPUT_HEIGHT * MODEL_INPUT_CHANNELS];
    resize_rgb888(rgb888_buf, fb->width, fb->height, resized, MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT);

    const int num_values = MODEL_INPUT_WIDTH * MODEL_INPUT_HEIGHT * MODEL_INPUT_CHANNELS;

    // Tensor de entrada é UINT8 com scale=1.0/zero_point=0 (identidade),
    // ou seja: o modelo foi treinado com pixels brutos 0-255, sem
    // normalização. Ainda assim aplicamos a fórmula genérica de
    // quantização, lendo scale/zero_point do próprio tensor, para o
    // código continuar correto caso o modelo seja retreinado com outra
    // quantização no futuro.
  if (input_tensor->type == kTfLiteUInt8) 
  {
        for (int i = 0; i < num_values; i++) 
        {
            input_tensor->data.uint8[i] = resized[i];
        }


    } else if (input_tensor->type == kTfLiteInt8) {
        const float scale = input_tensor->params.scale;
        const int zero_point = input_tensor->params.zero_point;
        for (int i = 0; i < num_values; i++) {
            int32_t q = static_cast<int32_t>(lroundf(resized[i] / scale)) + zero_point;
            if (q < -128) q = -128;
            if (q > 127) q = 127;
            input_tensor->data.int8[i] = static_cast<int8_t>(q);
        }
    } else if (input_tensor->type == kTfLiteFloat32) {
        for (int i = 0; i < num_values; i++) {
            input_tensor->data.f[i] = resized[i] / 255.0f;
        }
    } else {
        ESP_LOGE(TAG, "Tipo de tensor de entrada não suportado: %d", input_tensor->type);
        return ESP_FAIL;
    }

    if (interpreter->Invoke() != kTfLiteOk) {
        ESP_LOGE(TAG, "Invoke() falhou");
        return ESP_FAIL;
    }

    // Saída: 2 classes (softmax). Pegamos o argmax e desquantizamos só
    // o valor vencedor.
    const int num_classes = output_tensor->dims->data[output_tensor->dims->size - 1];
    int best_idx = 0;
    float best_prob = -1.0f;

    for (int i = 0; i < num_classes; i++) {
        float prob;
        if (output_tensor->type == kTfLiteUInt8) {
            const float scale = output_tensor->params.scale;
            const int zero_point = output_tensor->params.zero_point;
            prob = (output_tensor->data.uint8[i] - zero_point) * scale;
        } else if (output_tensor->type == kTfLiteInt8) {
            const float scale = output_tensor->params.scale;
            const int zero_point = output_tensor->params.zero_point;
            prob = (output_tensor->data.int8[i] - zero_point) * scale;
        } else {
            prob = output_tensor->data.f[i];
        }
        if (prob > best_prob) {
            best_prob = prob;
            best_idx = i;
        }
    }

    *out_class = best_idx;
    *out_confidence = best_prob;

    return ESP_OK;
}
