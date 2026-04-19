# 🌍 eLSI Hackathon Weather Station

![Hackathon Deploy](https://img.shields.io/badge/Status-Deployed-00d2ff?style=for-the-badge) ![Hardware](https://img.shields.io/badge/Hardware-ESP32%20%7C%20SIM800L-32d74b?style=for-the-badge) ![Cloud](https://img.shields.io/badge/Cloud-Google%20Sheets-ffd60a?style=for-the-badge)

A robust, cloud-connected environmental monitoring system developed as part of the **eLSI Hackathon (February 2026 - Wada Visit)**. This repository houses the entire tech stack for the project, spanning from low-level C++ firmware on embedded hardware to a sleek, real-time frontend visualization dashboard.

---

## 🛠️ Hardware Architecture

Our weather station goes beyond standard prototyping. To ensure high reliability across varying environmental conditions, the entire circuit is **permanently soldered onto a perf board**. By eliminating loose jumper wires, we guarantee structural integrity and consistent communication protocols.

### Core Microcontroller
* **LILYGO T-Call V1.4 ESP32 (AM-036)**: The brain of our station. This board provides dual-core processing power and features an onboard **SIM800L GPRS module**, allowing us to choose between Wi-Fi and Cellular network up-links.

### Sensor Integrated Suite
1. **Plantower PMS7003**: A highly precise laser scattering sensor continuously monitoring ambient Air Quality (PM2.5 particulate matter).
2. **MH-Z19C / MH-Z19B (NDIR)**: An infrared gas sensor providing highly accurate CO₂ concentration readings in Parts Per Million (PPM).
3. **MobileSense Core Module**: A consolidated environmental interface that aggregates:
   - **Temperature** (°C)
   - **Humidity** (%)
   - **Ambient Light** (LUX)
   - **Atmospheric Pressure** (hPa)

---

## 💻 Firmware & Codebase Integration

The repository includes two distinct `.ino` operational modes, allowing deployment flexibility depending on network availability. Both scripts parse data over multiple UART interfaces from our sensor suite.

### 1. `weather-station-connected-with-wifi.ino`
Ideal for deployment in areas with stable local networking. 
* Uses the native ESP32 `WiFi.h` library for reliable WLAN connectivity.
* Implements NTP (Network Time Protocol) for precise timestamping.

### 2. `weather-station-connected-with-simcard.ino`
Designed for remote, off-grid deployments (e.g., the Wada visit).
* Relies on the ESP32's onboard SIM800L module.
* Uses the `TinyGsmClient` library to establish a cellular GPRS connection (configured for Airtel GPRS).
* Employs internal uptime calculation when cell-tower time synchronization is slow.

---

## ☁️ Cloud Pipeline & Frontend Dashboard

The data collected by the ESP32 doesn't just sit on the hardware:

1. **Google Sheets Endpoint**: Both firmware variants fire HTTP GET requests to actuate a Google Apps Script endpoint. This completely serverless architecture appends live readings straight into a Google SpreadSheet.
2. **Glassmorphic Web Dashboard**: The included web application (`index.html`, `style.css`, `script.js`) polls the live, published Google Sheet CSV every 10 seconds.
3. **Data Visualization**: Employs `Chart.js` for dynamic historical rendering, allowing users to trace anomalies in weather patterns from 1 hour back, up to the maximum logged span. 

---

## 🚀 Setup & Local Execution

**For the Hardware:**
1. Open the desired `.ino` file via the Arduino IDE.
2. Install dependencies: `TinyGSM`, `Plantower_PMS7003`, `ErriezMHZ19B`.
3. Modify the `ssid`/`password` or Cellular `apn` parameters at the top of the file.
4. Flash to the LILYGO T-Call board.

**For the Frontend:**
1. Clone the repository: `git clone https://github.com/CrystalXitio/weather-station.git`
2. Simply open `index.html` in your favorite web browser—no `npm install` or local backend server required.

---
*Built with precision and sustainability during the eLSI Hackathon 2026. Data-driven engineering from breadboard, to perf board, to the cloud.*
