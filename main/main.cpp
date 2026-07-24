/**
 * @file main.cpp
 * @brief Benchmark de Inferência TinyML Embarcado para Sistemas de Estacionamento Inteligente.
 * 
 * Realiza profiling estatístico de latência e rastreamento de memória para modelos
 * de Visão Computacional quantizados em INT8 executando nas arquiteturas ESP32 / ESP32-S3.
 * 
 * @author Alex Mateus da Silva Ventura
 * @framework ESP-IDF v6.0+ | TensorFlow Lite for Microcontrollers
 */

#include <cstdio>
#include <cmath>
#include <algorithm>

// APIs do FreeRTOS e Sistema ESP
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_cpu.h"
#include "esp_private/esp_clk.h"
#include "esp_heap_caps.h"

// Arquitetura TFLite Micro
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// Datasets do Modelo e Imagens
#include "model_data.h"
#include "sample_empty.h"
#include "sample_occupied.h"

namespace {
    constexpr size_t kTensorArenaSize = 120 * 1024; // 120 KB de memória de trabalho
    constexpr int kBenchmarkRuns = 50;
    
    struct BenchmarkMetrics {
        float mean_ms;
        float std_dev_ms;
        float min_ms;
        float max_ms;
        size_t sram_usage_bytes;
        int8_t raw_output;
    };
}

/**
 * @brief Pré-processa o buffer de imagem bruta e quantiza os pixels diretamente para o tensor INT8.
 */
static void QuantizeImageToInput(const uint8_t* raw_img, size_t img_len, TfLiteTensor* input_tensor) {
    if (input_tensor->type != kTfLiteInt8) return;

    const float scale = input_tensor->params.scale;
    const int32_t zero_point = input_tensor->params.zero_point;

    for (size_t i = 0; i < img_len; ++i) {
        if (scale != 0.0f) {
            float normalized = static_cast<float>(raw_img[i]) / 255.0f;
            int32_t q_val = static_cast<int32_t>(std::round(normalized / scale)) + zero_point;
            
            // Limites de saturação [-128, 127] com casts explícitos para evitar erro no std::clamp
            q_val = std::clamp(q_val, static_cast<int32_t>(-128), static_cast<int32_t>(127));
            input_tensor->data.int8[i] = static_cast<int8_t>(q_val);
        } else {
            input_tensor->data.int8[i] = static_cast<int8_t>(static_cast<int16_t>(raw_img[i]) - 128);
        }
    }
}

extern "C" void app_main(void) 
{
    // Atraso para estabilização do hardware
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("\n==================================================\n");
    printf("   BENCHMARK TINYML DE ESTACIONAMENTO - ESP-IDF   \n");
    printf("==================================================\n");

    const size_t sram_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    printf("[SIS] SRAM Interna Livre (Pré-alocação): %zu bytes\n", sram_before);

    // Alocação Dinâmica da Tensor Arena (Fallback de PSRAM para SRAM Interna)
    uint8_t* tensor_arena = static_cast<uint8_t*>(heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!tensor_arena) 
    {
        tensor_arena = static_cast<uint8_t*>(heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }

    if (!tensor_arena) 
    {
        printf("[ERRO] Falha ao alocar Tensor Arena (%zu bytes)\n", kTensorArenaSize);
        return;
    }

    // Carrega o Modelo TFLite
    const tflite::Model* model = tflite::GetModel(smart_parking_model_int8_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("[ERRO] Incompatibilidade de versão do schema do modelo!\n");
        heap_caps_free(tensor_arena);
        return;
    }

    // Registro de Operadores (Resolver)
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

    // Inicialização do Intérprete
    tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, kTensorArenaSize);
    if (interpreter.AllocateTensors() != kTfLiteOk) 
    {
        printf("[ERRO] Falha na alocação de tensores dentro da Arena!\n");
        heap_caps_free(tensor_arena);
        return;
    }

    TfLiteTensor* input = interpreter.input(0);
    TfLiteTensor* output = interpreter.output(0);

    // Quantiza a Imagem de Exemplo
    QuantizeImageToInput(reinterpret_cast<const uint8_t*>(sample_empty), sample_empty_len, input);
    printf("[DADOS] Imagem de teste quantizada no tensor de entrada (%zu bytes)\n", sample_empty_len);

    // Execução de Aquecimento (Warm-up)
    printf("\n[BENCHMARK] Executando iteração de aquecimento...\n");
    if (interpreter.Invoke() != kTfLiteOk) 
    {
        printf("[ERRO] Falha na inferência de aquecimento!\n");
        heap_caps_free(tensor_arena);
        return;
    }

    // Loop de Profiling Estatístico
    printf("[BENCHMARK] Coletando métricas ao longo de %d iterações...\n", kBenchmarkRuns);

    float latencies[kBenchmarkRuns];
    float total_latency_ms = 0.0f;
    const uint32_t cpu_freq = esp_clk_cpu_freq();

    for (int i = 0; i < kBenchmarkRuns; ++i) 
    {
        uint32_t start_cycles = esp_cpu_get_cycle_count();
        
        interpreter.Invoke();
        
        uint32_t end_cycles = esp_cpu_get_cycle_count();
        uint32_t cycles = end_cycles - start_cycles;

        float latency_ms = (static_cast<float>(cycles) / static_cast<float>(cpu_freq)) * 1000.0f;
        latencies[i] = latency_ms;
        total_latency_ms += latency_ms;

        // Alimenta o Watchdog (TWDT) do FreeRTOS para evitar estouro durante loops intensivos de CPU
        vTaskDelay(1);
    }

    // Cálculo das Métricas
    BenchmarkMetrics metrics;
    metrics.mean_ms = total_latency_ms / static_cast<float>(kBenchmarkRuns);
    metrics.min_ms = latencies[0];
    metrics.max_ms = latencies[0];

    float variance_sum = 0.0f;
    for (int i = 0; i < kBenchmarkRuns; ++i) 
    {
        variance_sum += std::pow(latencies[i] - metrics.mean_ms, 2);
        metrics.min_ms = std::min(metrics.min_ms, latencies[i]);
        metrics.max_ms = std::max(metrics.max_ms, latencies[i]);
    }

    metrics.std_dev_ms = std::sqrt(variance_sum / static_cast<float>(kBenchmarkRuns));
    
    const size_t sram_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    metrics.sram_usage_bytes = sram_before - sram_after;
    metrics.raw_output = output->data.int8[0];

    const char* status_str = (metrics.raw_output > 0) ? "OCUPADO" : "LIVRE";

   
    printf("\n--- RESUMO ESTATÍSTICO DO BENCHMARK (%d RODADAS) ---\n", kBenchmarkRuns);
    printf("Saída Bruta INT8   : %d\n", metrics.raw_output);
    printf("Status da Vaga     : %s\n", status_str);
    printf("Latência Média     : %.2f ms\n", metrics.mean_ms);
    printf("Desvio Padrão      : %.2f ms\n", metrics.std_dev_ms);
    printf("Latência Mínima    : %.2f ms\n", metrics.min_ms);
    printf("Latência Máxima    : %.2f ms\n", metrics.max_ms);
    printf("Consumo de SRAM    : %zu bytes\n", metrics.sram_usage_bytes);
    printf("--------------------------------------------------\n");

    printf("JSON_LOG:{\"app\":\"smart_parking\", \"status\":\"%s\", \"runs\":%d, \"mean_ms\":%.2f, \"std_ms\":%.2f, \"min_ms\":%.2f, \"max_ms\":%.2f, \"sram_bytes\":%zu}\n",
           status_str, kBenchmarkRuns, metrics.mean_ms, metrics.std_dev_ms, metrics.min_ms, metrics.max_ms, metrics.sram_usage_bytes);

    heap_caps_free(tensor_arena);
}