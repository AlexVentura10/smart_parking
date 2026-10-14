# TinyML Smart Parking System

Master's research project focused on Neural Architecture Search (NanoNAS) and Edge AI deployment on microcontroller hardware.

## About the Project

This project implements an automated, end-to-end pipeline designed for resource-constrained edge devices:
1. **Search:** Discovery of an optimized convolutional neural network architecture using **NanoNAS**.
2. **Training & Validation:** Training and evaluation of the discovered topology on the **PKLot dataset**.
3. **Quantization:** Post-training quantization of the model to 8-bit integers (**INT8**) using TensorFlow Lite.
4. **Deployment:** Real-time embedded inference on an **ESP32-S3** microcontroller for parking space detection.

## Repository Structure

```text
projeto-pklot-esp32/
├── nanonas/                 # Python scripts for NAS, training, and quantization
└── smart_parking/           # C/C++ firmware for ESP32-S3 (ESP-IDF)
```

## Hardware Target

* **Microcontroller:** ESP32-S3 (WROOM / DevKitC-1 with PSRAM support)
* **Target Constraints:** Minimized RAM and Flash memory footprint optimized for edge computing devices.

## How to Run

### 1. NanoNAS Pipeline (Python)
Navigate to the `nanonas` folder, configure your environment, and execute the search script:
```bash
cd nanonas
python search.py
```

### 2. ESP32 Firmware Deployment (C / ESP-IDF)
Open the `smart_parking` folder in VS Code or your preferred IDE with the ESP-IDF extension, build, and flash the project to your ESP32-S3 board:
```bash
cd smart_parking
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```