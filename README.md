cat << 'EOF' > README.md
# 🅿️ Smart Parking - TinyML Benchmark no ESP32

Este repositório contém a implementação e o benchmark de um modelo de **Visão Computacional quantizado (TFLite INT8)** embarcado em um microcontrolador **ESP32** usando o ecossistema **ESP-IDF v6.0**. 

O objetivo do projeto é classificar o status de vagas de estacionamento (*LIVRE* / *OCUPADA*) diretamente na borda (Edge AI / TinyML), avaliando métricas cruciais de sistemas embarcados: **latência de inferência em ciclos de CPU** e **alocação de memória SRAM**.

---

## 🛠️ Tecnologias e Ferramentas

* **Microcontrolador:** ESP32 (Xtensa LX6 dual-core @ 160 MHz)
* **Framework:** ESP-IDF v6.0.2
* **Engine de Inferência:** TensorFlow Lite for Microcontrollers (`espressif__esp-tflite-micro`)
* **Linguagem:** C++ (C++23/C++26)

---

## 📊 Resultados do Benchmark

Abaixo estão as métricas reais obtidas diretamente no console do ESP32 a partir de imagens estáticas pré-processadas:

| Métrica | Valor Obtido |
| :--- | :--- |
| **Status Detectado** | `VAGA LIVRE` |
| **Latência de Inferência** | **751.94 ms** |
| **Ciclos de CPU** | 120.309.823 ciclos (@ 160 MHz) |
| **Consumo de SRAM** | **123.296 bytes** (~120.4 KiB) |
| **SRAM Livre (Antes da Alocação)** | ~388 KB |

### 📄 Exemplo de Saída Serial (Log em JSON)
```json
JSON_LOG:{"app":"smart_parking", "status":"LIVRE", "raw_int8":0, "latency_ms":751.94, "sram_bytes":123296}