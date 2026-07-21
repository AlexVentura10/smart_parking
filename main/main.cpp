/**
 * @file main.cpp
 * @brief Benchmark de Inferência TinyML em ESP32 para Detecção de Vagas de Estacionamento.
 * 
 * Este arquivo realiza a inicialização do TensorFlow Lite for Microcontrollers (TFLM),
 * alocação dinâmica da memória de tensores (Tensor Arena), pré-processamento/quantização
 * dos dados de entrada (C Byte Array), medição precisa de ciclos de CPU para latência 
 * e monitoramento de consumo de memória SRAM.
 *
 * @author Alex Mateus da Silva Ventura
 * @framework ESP-IDF v6.0+ | TensorFlow Lite for Microcontrollers
 */

#include <stdio.h>

// Includes nativos do FreeRTOS e Arquitetura do ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_cpu.h"             // Acesso aos contadores de ciclos do hardware (Xtensa LX6/LX7)
#include "esp_private/esp_clk.h" // Leitura da frequência atual do clock da CPU
#include "esp_heap_caps.h"       // Gestão avançada de memória RAM (SRAM e PSRAM/SPIRAM)

// TensorFlow Lite for Microcontrollers (TFLM) - Dependências do componente esp-tflite-micro
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h" // Registro manual de operadores (reduz binário)
#include "tensorflow/lite/micro/micro_interpreter.h"        // Engine de execução e inferência
#include "tensorflow/lite/schema/schema_generated.h"        // Validação da versão do Schema FlatBuffers

// Arrays C/C++ contendo o modelo quantizado (INT8) e amostras de imagens estáticas pré-processadas
#include "model_data.h"
#include "sample_empty.h"
#include "sample_occupied.h"

/**
 * @brief Tamanho da Tensor Arena (Buffer de trabalho interno do TFLite).
 * 
 * Alocado dinamicamente no Heap para armazenar tensores de entrada, saída, 
 * mapas de características intermediários e estados de camadas.
 * Definido com margem de segurança (~120 KB).
 */
constexpr size_t kTensorArenaSize = 120 * 1024;

extern "C" void app_main(void) {
    // Atraso inicial de 1s para permitir a estabilização do hardware do UART/Serial Monitor
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    printf("\n==================================================\n");
    printf("   SMART PARKING TINYML BENCHMARK - ESP32        \n");
    printf("==================================================\n");

    // -------------------------------------------------------------------------
    // 1. MONITORAMENTO DE MEMÓRIA INICIAL
    // -------------------------------------------------------------------------
    // Mede a quantidade de memória SRAM interna disponível antes de qualquer alocação.
    size_t free_sram_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    printf("[MEMORIA] SRAM Livre Antes da Alocação: %u bytes\n", (unsigned int)free_sram_before);

    // -------------------------------------------------------------------------
    // 2. ALOCAÇÃO DINÂMICA DA TENSOR ARENA
    // -------------------------------------------------------------------------
    // Tenta alocar na PSRAM externa (SPIRAM) se disponível; caso contrário, usa a SRAM interna.
    uint8_t* tensor_arena = (uint8_t*) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (tensor_arena == nullptr) {
        tensor_arena = (uint8_t*) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (tensor_arena == nullptr) {
        printf("[ERRO] Falha crítica ao alocar %u bytes para o Tensor Arena no Heap!\n", (unsigned int)kTensorArenaSize);
        return;
    }

    // -------------------------------------------------------------------------
    // 3. CARREGAMENTO E VALIDAÇÃO DO MODELO
    // -------------------------------------------------------------------------
    // Mapeia o vetor 'smart_parking_model_int8_tflite' armazenado na Flash (RODATA)
    const tflite::Model* model = tflite::GetModel(smart_parking_model_int8_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("[ERRO] Versão do Schema TFLite incompatível!\n");
        heap_caps_free(tensor_arena);
        return;
    }

    // -------------------------------------------------------------------------
    // 4. REGISTRO SELETIVO DE OPERADORES (OpResolver)
    // -------------------------------------------------------------------------
    // Registra explicitamente apenas as camadas necessárias ao modelo para economizar Flash.
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

    // -------------------------------------------------------------------------
    // 5. INSTANCIAÇÃO DO INTÉRPRETE E Mapeamento DOS TENSORES
    // -------------------------------------------------------------------------
    auto* interpreter = new tflite::MicroInterpreter(model, resolver, tensor_arena, kTensorArenaSize);

    // Aloca a memória da Tensor Arena para os tensores de cada camada do grafo
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        printf("[ERRO] Falha ao alocar tensores! Verifique o tamanho do kTensorArenaSize.\n");
        heap_caps_free(tensor_arena);
        return;
    }

    // Ponteiros para os buffers de Entrada (Input) e Saída (Output)
    TfLiteTensor* input = interpreter->input(0);
    TfLiteTensor* output = interpreter->output(0);

    // -------------------------------------------------------------------------
    // 6. PRÉ-PROCESSAMENTO E QUANTIZAÇÃO INT8 DA IMAGEM
    // -------------------------------------------------------------------------
    // Carrega a imagem estática pré-convertida em array C (`sample_empty`)
    const uint8_t* raw_image = (const uint8_t*)sample_empty; 
    size_t image_size = sample_empty_len;

    // Se a entrada do modelo for quantizada em INT8 (faixa -128 a 127)
    if (input->type == kTfLiteInt8) {
        float scale = input->params.scale;
        int32_t zero_point = input->params.zero_point;

        // Converte e quantiza pixel por pixel: Q = (Real / Scale) + ZeroPoint
        for (size_t i = 0; i < image_size; i++) {
            if (scale != 0.0f) {
                float normalized_val = (float)raw_image[i] / 255.0f; // Normalização [0.0, 1.0]
                int32_t q_val = (int32_t)(normalized_val / scale) + zero_point;
                
                // Truncamento dentro dos limites do inteiro de 8 bits assinado
                if (q_val < -128) q_val = -128;
                if (q_val > 127) q_val = 127;
                
                input->data.int8[i] = (int8_t)q_val;
            } else {
                // Algoritmo de fallback para alinhamento simples de offset
                input->data.int8[i] = (int8_t)((int16_t)raw_image[i] - 128);
            }
        }
    } else if (input->type == kTfLiteUInt8) {
        // Cópia direta para modelos não assinados [0, 255]
        for (size_t i = 0; i < image_size; i++) {
            input->data.uint8[i] = raw_image[i];
        }
    }

    printf("[DADOS] Imagem de Teste Carregada e Quantizada no Tensor (%u bytes)\n", (unsigned int)image_size);

    // -------------------------------------------------------------------------
    // 7. EXECUÇÃO DA INFERÊNCIA E MEDIÇÃO DE TEMPO DE CPU
    // -------------------------------------------------------------------------
    // Captura o ciclo exato da CPU antes da inferência via registrador CCOUNT
    uint32_t start_cycles = esp_cpu_get_cycle_count();

    // Executa o grafo computacional da rede neural no ESP32
    TfLiteStatus invoke_status = interpreter->Invoke();

    // Captura o ciclo final da CPU
    uint32_t end_cycles = esp_cpu_get_cycle_count();

    if (invoke_status != kTfLiteOk) {
        printf("[ERRO] Falha ao executar a inferência!\n");
        heap_caps_free(tensor_arena);
        return;
    }

    // -------------------------------------------------------------------------
    // 8. CÁLCULO DE MÉTRICAS E DESEMPENHO
    // -------------------------------------------------------------------------
    uint32_t total_cycles = end_cycles - start_cycles;
    
    // Converte ciclos de CPU em milissegundos utilizando a frequência do clock (ex: 160MHz)
    float latency_ms = ((float)total_cycles / (float)esp_clk_cpu_freq()) * 1000.0f;
    
    // Mede a SRAM restante pós-execução para calcular o consumo líquido
    size_t free_sram_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    // -------------------------------------------------------------------------
    // 9. INTERPRETAÇÃO DOS RESULTADOS E LOGS
    // -------------------------------------------------------------------------
    // Saída escalar INT8: Valores maiores que 0 indicam probabilidade de ocupação (> 50%)
    int8_t raw_output = output->data.int8[0];
    const char* status = (raw_output > 0) ? "OCUPADA" : "LIVRE";

    printf("\n--- RESULTADOS DA INFERÊNCIA ---\n");
    printf("Saída Bruta INT8 : %d\n", raw_output);
    printf("Status Detectado : VAGA %s\n", status);
    printf("Latência Exata   : %.2f ms (%lu ciclos de CPU)\n", latency_ms, (unsigned long)total_cycles);
    printf("Consumo de SRAM  : %u bytes\n", (unsigned int)(free_sram_before - free_sram_after));
    printf("--------------------------------------------------\n");

    // Saída estruturada em JSON para facilidade de parse por scripts remotos/Iot
    printf("JSON_LOG:{\"app\":\"smart_parking\", \"status\":\"%s\", \"raw_int8\":%d, \"latency_ms\":%.2f, \"sram_bytes\":%u}\n",
           status, raw_output, latency_ms, (unsigned int)(free_sram_before - free_sram_after));

    // Liberação de memória alocada dinamicamente
    heap_caps_free(tensor_arena);
}