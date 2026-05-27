# HEMOGUARD AI ESP32 Biomedical Monitoring System

Hemoguard is an AI-powered multimodal wearable system for early detection of internal bleeding and hemodynamic instability before traditional vital signs change. Using ECG, PPG, Pulse Transit Time (PTT), bioimpedance, temperature gradient, HRV, and intelligent signal fusion, it enables real-time physiological risk assessment and monitoring.

A comprehensive, edge-based biomedical monitoring system running on the ESP32. It integrates real-time physiological data from multiple sensors (ECG, PPG, Bioimpedance, Temperature) and uses a Hybrid ML model (LSTM + Random Forest) to predict the patient's condition (Stable, Warning, Critical) continuously. The device features a local web dashboard accessible via WiFi for real-time visualization and monitoring.

## 🚀 Features

- **Edge ML Processing**: PyTorch-trained LSTM combined with Random Forest feature weighting.
- **Continuous Monitoring**: Processes data from ECG, PPG, Bioimpedance, and Temperature sensors.
- **Local Web Dashboard**: Fully featured, dark-themed HTML/JS dashboard hosted directly on the ESP32 (or running locally using the FastAPI backend).
- **Hemorrhage Inference Index (HII)**: A composite score derived from combined sensor inputs to determine the patient state.
- **RESTful API**: FastAPI backend to serve predictions and dashboard resources.

## 📁 Repository Structure

```
HEMOGUARD/
├── README.md
├── requirements.txt
├── .gitignore
├── firmware/                    # Main ESP32 Arduino sketches and PROGMEM HTML headers
├── sensor_tests/                # Code to test individual sensors (ECG, PPG, etc.)
├── ml/                          # ML training pipeline (PyTorch LSTM + Scikit-Learn RF)
├── models/                      # Saved trained models (.pt, .pkl) and metadata
├── web/                         # FastAPI backend and web dashboard (HTML/CSS/JS)
├── datasets/                    # Datasets (synthetic and recorded real-world data)
└── docs/                        # Plots and training logs
```

## 🛠️ Hardware Setup

The system relies on the following sensors connected to the ESP32:
- **ECG**: AD8232 module
- **PPG**: MAX30100 optical pulse oximeter
- **Temperature**: DS18B20 digital temperature probe
- **Bioimpedance**: Custom analog voltage divider setup

See `firmware/esp32_hii_monitor_wifi.ino` for exact pin mappings and wiring.

## 💻 Software Setup

### Prerequisites

- **ESP32 Firmware**: Arduino IDE with ESP32 board support, plus libraries (`MAX30100lib`, `DallasTemperature`, `OneWire`).
- **Python Backend / ML Pipeline**: Python 3.9+.

### Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/aayushpataskar/HEMOGUARD.git
   cd HEMOGUARD
   ```

2. **Install Python dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

3. **Deploy Firmware to ESP32:**
   - Update WiFi credentials in `firmware/esp32_hii_monitor_wifi.ino`.
   - Upload the sketch via Arduino IDE. The dashboard is hosted on the ESP32's assigned IP upon booting.

### Running the Python Backend (Local Dev)
To test predictions and data visualization without the ESP32 webserver:
```bash
python -m uvicorn web.backend:app --port 8000
```
Open `http://127.0.0.1:8000` in your browser.

## 🧠 ML Pipeline (Hybrid LSTM + Random Forest)

The model is trained using synthetic or real recorded ESP32 data. It applies Random Forest (for feature importance weighting) combined with an LSTM (for temporal clinical progression).

To retrain the model:
```bash
python ml/train_hybrid.py
```
This generates new models in the `models/` folder and saves training performance plots in `docs/`.

## 📜 License

This project is open-source. Please see the license file for details.
