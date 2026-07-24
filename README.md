# Smart Parking - TinyML Benchmark no ESP32 / ESP32-S3

Esse repositório contém a implementação, o pipeline de dados e o benchmark de uma **Rede Neural Convolucional (CNN) quantizada em 8 bits (int8)** embarcado em microcontroladores **ESP32 / ESP32-S3** usando o ecossistema **ESP-IDF v6.0**.

O objetivo do projeto é classificar o status de vagas de estacionamento (*LIVRE* / *OCUPADA*) diretamente na borda (Edge AI / TinyML), avaliando métricas cruciais de sistemas embarcados: **latência de inferência estatística (média, desvio padrão e mín/máx)** e **alocação de memória SRAM**.

---

## Workflow & Arquitetura do Modelo

O ciclo de desenvolvimento da CNN foi dividido entre o ambiente de nuvem (**Google Colab**) e o ambiente embarcado:

1. **Treinamento & Quantização (Google Colab / TensorFlow):**
   * Treinamento do modelo de classificação de imagem.
   * Quantização pós-treinamento para **INT8** (*Full Integer Quantization*) com representação dinâmica/fixa de tensores para otimizar tamanho na Flash e velocidade de execução.
   * Exportação do modelo para o formato `.tflite`.

2. **Vetorização das Imagens de Teste (`convert_parking_images.py`):**
   * Leitura e pré-processamento das imagens da pasta `test_samples/`.
   * Redimensionamento, normalização e conversão da matriz de pixels em **C Byte Arrays** (headers `.h`) para serem gravados na memória Flash do ESP32 (`RODATA`).

3. **Inferência no ESP32 (ESP-IDF + TensorFlow Lite for Microcontrollers):**
   * Carregamento do modelo a partir do array C++ em memória.
   * Alocação da Tensor Arena com suporte a fallback de PSRAM para SRAM interna.
   * Execução de 50 iterações de inferência após warm-up e medição de desempenho em ciclos de CPU e heap interno.

---

## Tecnologias e Ferramentas

* **Microcontroladores:** ESP32 / ESP32-S3
* **Framework Embarcado:** ESP-IDF v6.0.2 (C++26)
* **Engine de Inferência:** TensorFlow Lite for Microcontrollers (`espressif__esp-tflite-micro`)
* **Treinamento & Modelação:** Google Colab, TensorFlow / Keras, Python 3
* **Linguagem:** C++

---

## Resultados do Benchmark (ESP32-S3)

Métricas estatísticas reais coletadas diretamente no console do ESP32 a partir das imagens estáticas pré-processadas ao longo de 50 execuções:

| Métrica | Valor Obtido |
| :--- | :--- |
| **Status Detectado** | `VAGA LIVRE` |
| **Latência Média** | **127.33 ms** |
| **Desvio Padrão** | **0.04 ms** (Mín: 127.32 ms \| Máx: 127.63 ms) |
| **Consumo de SRAM** | **123.060 bytes** (~120.1 KiB) |

---

## Nota: 

Esse projeto foi desenvolvido para fins de aprendizagem e introdução ao TinyML, não se aprofundando em técncias avançadas de otimização, porém possui uma boa margem de evolução. Sinta-se à vontade para adaptar e evoluir esse projeto da maneira que melhor te atenda.  
