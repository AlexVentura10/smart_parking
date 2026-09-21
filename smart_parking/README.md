# Monitoramento de vagas — ESP32-S3-N16R8 + TinyML (ESP-IDF)

Porte do projeto de Arduino/PlatformIO para ESP-IDF puro, usando os componentes
gerenciados oficiais da Espressif para câmera e TFLite Micro.

## Por que isso resolve o hang anterior

O firmware Arduino travava silenciosamente logo após `psramInit(): PSRAM
enabled`. As causas mais prováveis eram conflito de pinos entre a câmera e o
barramento octal do PSRAM, ou inconsistência nas flags de `flash_mode`/
`memory_type`. Aqui:

- O driver de câmera é o componente `espressif/esp32-camera`, testado
  extensivamente em placas S3 com PSRAM octal. Os pinos em
  `main/camera_pins.h` já foram identificados a partir da foto da sua placa
  (ver seção "Hardware identificado" abaixo), todos fora da faixa reservada
  pelo PSRAM octal.
- `sdkconfig.defaults` fixa explicitamente `qio` (flash) + `oct` (PSRAM) +
  16MB, sem depender de um board manifest de terceiros que possa estar
  desalinhado.

## Hardware identificado

A partir da foto da placa: módulo ESP32-S3-WROOM-1-N16R8, sensor **OV3660**
(etiqueta "TY-OV3660-21MM-V3.0" no FPC), conectado via conector FPC (não
fiação manual), header no layout padrão do ESP32-S3-DevKitC-1 (dual USB-C).
Esse é um clone bem conhecido, e `main/camera_pins.h` já está preenchido com
o mapeamento de pinos padrão da comunidade para esse modelo específico
(macro `CAMERA_MODEL_ESP32_S3_CAM`), todos fora da faixa GPIO 26–37 (reservada
pelo PSRAM octal). Se o hardware tiver alguma variação de revisão, veja a
seção de diagnóstico abaixo.

## Modelo

O `main/model_cnrpark.h` já contém o array real do seu
`results_cnrpark_resulting_architecture.tflite` (16.832 bytes), extraído e
analisado diretamente do flatbuffer. Arquitetura confirmada:

- **Entrada:** `uint8 [1, 50, 50, 3]` — imagem RGB 50×50, pixels brutos
  (scale=1.0, zero_point=0, ou seja, sem normalização adicional).
- **Corpo:** 6 camadas `Conv2D` (canais 3→3→6→9→12→14→15), as 5 primeiras
  seguidas de `MaxPool2D` (50→25→12→6→3→1), BatchNorm+ReLU fundidos nas
  convoluções.
- **Cabeça:** `Mean` (global average pooling) → `FullyConnected` → `Softmax`.
- **Saída:** `uint8 [1, 2]` — probabilidades das 2 classes (scale=1/256).

Por isso o pipeline de câmera foi ajustado para capturar em **RGB565** (não
mais escala de cinza) e convertê-lo para RGB888 antes de redimensionar para
50×50 em `app_model.cpp`.

**Atenção — convenção de classes assumida:** o código assume índice `0 =
livre` e `1 = ocupada`, que é o padrão do dataset CNRPark-EXT usado para
treinar esse tipo de classificador. Isso **não está codificado no `.tflite`**
— confirme apontando a câmera para uma vaga livre e depois ocupada na
primeira execução; se o log sair invertido, troque a condição em `main.c`
(`predicted_class == 1`).

## O que ainda vale a pena verificar antes de compilar

Todos os valores críticos (pinos, sensor, dimensões de entrada, ops usadas)
já foram extraídos de fontes reais (foto da placa + inspeção do `.tflite`),
mas vale conferir:

1. **Convenção de classes** (0=livre / 1=ocupada) — ver seção "Modelo" acima.
2. **`kTensorArenaSize` (96 KB)** em `app_model.cpp` — herdado do projeto
   Arduino original. Com 6 camadas Conv2D isso deve sobrar folga (as
   camadas são bem pequenas — no máximo 15 canais, 50×50 no início), mas se
   `AllocateTensors()` falhar no log, é só aumentar esse valor.
3. Se a etiqueta do FPC da câmera não for exatamente "TY-OV3660", ou se o
   header da sua placa tiver alguma revisão diferente da foto analisada,
   revalide `main/camera_pins.h`.

## Estrutura

```
parking_tinyml/
├── CMakeLists.txt          # projeto top-level
├── sdkconfig.defaults      # PSRAM octal, flash 16MB, partição custom
├── partitions.csv          # partição factory de 4MB (modelo + libs são grandes)
└── main/
    ├── idf_component.yml   # dependências: esp32-camera, esp-tflite-micro
    ├── CMakeLists.txt
    ├── camera_pins.h       # pinout confirmado (clone DevKitC-1 + FPC OV3660)
    ├── app_camera.h/.c     # wrapper sobre esp32-camera (captura RGB565)
    ├── model_cnrpark.h     # array real do modelo (50x50x3 -> 2 classes)
    ├── app_model.h/.cpp    # wrapper TFLite Micro (RGB888 + resize + inferência)
    └── main.c              # countdown + loop de captura/inferência
```

## Build

Pré-requisito: ESP-IDF v5.1 ou superior instalado e "exportado" no shell
(`. $IDF_PATH/export.sh` ou `export.ps1` no Windows).

```bash
cd parking_tinyml
idf.py set-target esp32s3

# primeira vez: baixa esp32-camera e esp-tflite-micro do component registry
# (precisa de internet)
idf.py build

idf.py -p /dev/ttyUSB0 flash monitor
```

Se `idf.py build` reclamar de dependências não resolvidas, rode
`idf.py update-dependencies` e tente de novo.

## Diagnóstico se ainda travar após o PSRAM

Mesmo com o driver oficial, se o hang persistir:

- Rode `idf.py monitor` sem filtros — no ESP-IDF puro (fora do PlatformIO), o
  monitor já decodifica backtraces de panic automaticamente, então qualquer
  Guru Meditation Error vai aparecer com endereço de PC.
- Comente `app_camera_init()` em `main.c` e veja se o `app_main` chega até o
  fim do countdown normalmente — isso isola se o problema é a câmera ou algo
  na alocação da tensor arena.
- Confira no log de boot (`idf.py monitor`, seção antes do `app_main`) se o
  PSRAM realmente reporta 8MB — se aparecer só metade, é sinal de que o modo
  octal não foi realmente aplicado no build.
