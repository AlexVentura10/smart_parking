#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_cpu.h"
#include "esp_private/esp_clk.h"
#include "esp_heap_caps.h"

// TensorFlow Lite Micro Headers (Ajustado para o esp-tflite-micro)
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// Modelo e amostras de imagens estáticas
#include "model_data.h"
#include "sample_empty.h"
#include "sample_occupied.h"

// 💡 120 KB para acomodar com folga a alocação dos tensores
constexpr size_t kTensorArenaSize = 120 * 1024;

extern "C" void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda estabilização da Serial
    
    printf("\n==================================================\n");
    printf("   SMART PARKING TINYML BENCHMARK - ESP32        \n");
    printf("==================================================\n");

    // 1. Verificação de Memória SRAM Antes da Alocação
    size_t free_sram_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    printf("[MEMORIA] SRAM Livre Antes da Alocação: %u bytes\n", (unsigned int)free_sram_before);

    // 2. Alocação Dinâmica do Tensor Arena via Heap
    uint8_t* tensor_arena = (uint8_t*) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (tensor_arena == nullptr) {
        tensor_arena = (uint8_t*) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (tensor_arena == nullptr) {
        printf("[ERRO] Falha crítica ao alocar %u bytes para o Tensor Arena no Heap!\n", (unsigned int)kTensorArenaSize);
        return;
    }

    // 3. Carregar o Modelo TFLite da Flash
    const tflite::Model* model = tflite::GetModel(smart_parking_model_int8_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("[ERRO] Versão do Schema TFLite incompatível!\n");
        heap_caps_free(tensor_arena);
        return;
    }


    // 4. Registrar explicitamente as ops utilizadas pelo modelo
    static tflite::MicroMutableOpResolver<20> resolver;
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddFullyConnected();
    resolver.AddReshape();
    resolver.AddSoftmax();
    resolver.AddShape();
    resolver.AddMaxPool2D();
    resolver.AddStridedSlice();
    resolver.AddPack();
    resolver.AddUnpack();
    resolver.AddLogistic(); 
    resolver.AddPad();
    resolver.AddMul();
    resolver.AddAdd();
    resolver.AddQuantize();
    resolver.AddDequantize();

    // 5. Instanciar o Intérprete Dinamicamente
    auto* interpreter = new tflite::MicroInterpreter(model, resolver, tensor_arena, kTensorArenaSize);

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        printf("[ERRO] Falha ao alocar tensores! Verifique o tamanho do kTensorArenaSize.\n");
        heap_caps_free(tensor_arena);
        return;
    }

    TfLiteTensor* input = interpreter->input(0);
    TfLiteTensor* output = interpreter->output(0);

    // 6. Copiar e quantizar os dados de imagem para o tensor de entrada
    const uint8_t* raw_image = (const uint8_t*)sample_empty; 
    size_t image_size = sample_empty_len;

    if (input->type == kTfLiteInt8) {
        float scale = input->params.scale;
        int32_t zero_point = input->params.zero_point;

        for (size_t i = 0; i < image_size; i++) {
            if (scale != 0.0f) {
                float normalized_val = (float)raw_image[i] / 255.0f;
                int32_t q_val = (int32_t)(normalized_val / scale) + zero_point;
                
                if (q_val < -128) q_val = -128;
                if (q_val > 127) q_val = 127;
                
                input->data.int8[i] = (int8_t)q_val;
            } else {
                input->data.int8[i] = (int8_t)((int16_t)raw_image[i] - 128);
            }
        }
    } else if (input->type == kTfLiteUInt8) {
        for (size_t i = 0; i < image_size; i++) {
            input->data.uint8[i] = raw_image[i];
        }
    }

    printf("[DADOS] Imagem de Teste Carregada e Quantizada no Tensor (%u bytes)\n", (unsigned int)image_size);

    // 7. Medição de Latência com Contador de Ciclos da CPU
    uint32_t start_cycles = esp_cpu_get_cycle_count();

    TfLiteStatus invoke_status = interpreter->Invoke();

    uint32_t end_cycles = esp_cpu_get_cycle_count();

    if (invoke_status != kTfLiteOk) {
        printf("[ERRO] Falha ao executar a inferência!\n");
        heap_caps_free(tensor_arena);
        return;
    }

    // 8. Métricas de Desempenho
    uint32_t total_cycles = end_cycles - start_cycles;
    float latency_ms = ((float)total_cycles / (float)esp_clk_cpu_freq()) * 1000.0f;
    size_t free_sram_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    // 9. Interpretação dos Resultados
    int8_t raw_output = output->data.int8[0];
    const char* status = (raw_output > 0) ? "OCUPADA" : "LIVRE";

    printf("\n--- RESULTADOS DA INFERÊNCIA ---\n");
    printf("Saída Bruta INT8 : %d\n", raw_output);
    printf("Status Detectado : VAGA %s\n", status);
    printf("Latência Exata   : %.2f ms (%lu ciclos de CPU)\n", latency_ms, (unsigned long)total_cycles);
    printf("Consumo de SRAM  : %u bytes\n", (unsigned int)(free_sram_before - free_sram_after));
    printf("--------------------------------------------------\n");

    // Saída em JSON
    printf("JSON_LOG:{\"app\":\"smart_parking\", \"status\":\"%s\", \"raw_int8\":%d, \"latency_ms\":%.2f, \"sram_bytes\":%u}\n",
           status, raw_output, latency_ms, (unsigned int)(free_sram_before - free_sram_after));

    heap_caps_free(tensor_arena);
}