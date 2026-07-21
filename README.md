# Smart Parking - TinyML Benchmark no ESP32

Esse repositório contém a implementação, o pipeline de dados e o benchmark de um modelo de **Visão Computacional quantizado (TFLite INT8)** embarcado em um microcontrolador **ESP32** usando o ecossistema **ESP-IDF v6.0**.

O objetivo do projeto é classificar o status de vagas de estacionamento (*LIVRE* / *OCUPADA*) diretamente na borda (Edge AI / TinyML), avaliando métricas cruciais de sistemas embarcados: **latência de inferência em ciclos de CPU** e **alocação de memória SRAM**.

---

## Workflow & Arquitetura do Modelo

O ciclo de vida do modelo foi dividido entre o ambiente de nuvem (**Google Colab**) e o ambiente embarcado:

1. **Treinamento & Quantização (Google Colab / TensorFlow):**
   * Treinamento do modelo de classificação de imagem.
   * Quantização pós-treinamento para **INT8** (*Full Integer Quantization*) com representação dinâmica/fixa de tensores para otimizar tamanho na Flash e velocidade de execução.
   * Exportação do modelo para o formato `.tflite`.

2. **Vetorização das Imagens de Teste (`convert_parking_images.py`):**
   * Leitura e pré-processamento das imagens da pasta `test_samples/`.
   * Redimensionamento, normalização e conversão da matriz de pixels em **C Byte Arrays** (headers `.h`) para serem gravados na memória Flash do ESP32 (`RODATA`).

3. **Inferência no ESP32 (ESP-IDF + TensorFlow Lite for Microcontrollers):**
   * Carregamento do modelo a partir do array C++ em memória.
   * Execução da inferência e medição de desempenho via ciclos de CPU e heap interno/SPIRAM.

---

## 🛠Tecnologias e Ferramentas

* **Microcontrolador:** ESP32 (Xtensa LX6 dual-core @ 160 MHz)
* **Framework Embarcado:** ESP-IDF v6.0.2
* **Engine de Inferência:** TensorFlow Lite for Microcontrollers (`espressif__esp-tflite-micro`)
* **Treinamento & Modelação:** Google Colab, TensorFlow / Keras, Python 3
* **Linguagem:** C++ (C++23/C++26)

---

## 📊 Resultados do Benchmark

Métricas reais obtidas diretamente no console do ESP32 a partir das imagens estáticas pré-processadas:

| Métrica | Valor Obtido |
| :--- | :--- |
| **Status Detectado** | `VAGA LIVRE` |
| **Latência de Inferência** | **751.94 ms** |
| **Ciclos de CPU** | 120.309.823 ciclos (@ 160 MHz) |
| **Consumo de SRAM** | **123.296 bytes** (~120.4 KiB) |
| **SRAM Livre (Antes da Alocação)** | ~388 KB |

### Exemplo de Saída Serial (Log em JSON)
```json
JSON_LOG:{"app":"smart_parking", "status":"LIVRE", "raw_int8":0, "latency_ms":751.94, "sram_bytes":123296}