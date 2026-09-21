# TinyML Smart Parking System

Master's research project focused on **Neural Architecture Search (NanoNAS)** and **Edge AI** deployment on microcontroller hardware.

---

## About the Project
This project implements an automated pipeline to:
1. **Search** for an optimized convolutional neural network using **NanoNAS**.
2. **Train** and validate the model on the **PKLot** dataset.
3. **Quantize** the model to 8-bit integers (INT8) using **TensorFlow Lite**.
4. **Deploy** the model on an **ESP32-S3** microcontroller for real-time parking space detection.

---

## Repository Structure

```text
projeto-pklot-esp32/
├── nanonas/                 # Python scripts for NAS, training, and quantization
└── smart_parking/           # C/C++ firmware for ESP32-S3 (ESP-IDF)

```

## Hardware Target
Microcontroller: ESP32-S3

Target Constraints: Low RAM and Flash usage optimized for edge devices.

## How to Run

1. NanoNAS (Python)
Navigate to the nanonas folder and run the search script:

Bash

```text
python search.py

```
2. ESP32 Firmware (C / ESP-IDF)
Open the smart_parking folder in VS Code, build, and flash the project to your ESP32-S3 board.



