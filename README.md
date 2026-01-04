![C++](https://img.shields.io/badge/C%2B%2B-Programming-blue)
![Raspberry Pi](https://img.shields.io/badge/Raspberry%20Pi-3%20Model%20B-red)
![Embedded Systems](https://img.shields.io/badge/Embedded-Systems-orange)
![GPIO](https://img.shields.io/badge/GPIO-Control-green)
![Linux](https://img.shields.io/badge/Linux-OS-black)

# Raspberry Pi 3 Model B – Traffic Light Controller 🚦

C++ project for controlling a traffic light system using **Raspberry Pi 3 Model B** and GPIO pins.

The project demonstrates the implementation of traffic light logic for an embedded system, developed as a university assignment.

---

## 🛠 Technologies
- **C++**
- **Raspberry Pi 3**
- **WiringPi**
- **GPIO**
- **I2C communication**
- **OLED SH1106 Display**
- **Multithreading (std::thread, mutex, condition_variable)**
- **Linux**

---

## ⚙️ Features
- Pedestrian button request system  
- Countdown timer displayed on OLED (large digits)  
- Sound signal (buzzer) for visually impaired pedestrians  
- Full traffic light logic for vehicles and pedestrians  
- Ethernet connection monitoring  
- Multithreaded architecture  
- Safe shutdown on Ctrl+C or network failure 

---

## 📂 Project Structure
- `firstTrafficLightController.cpp` – initial implementation  
- `seconTrafficLightController.cpp` – improved version  
- `third_FINAL_TrafficLightController.cpp` – final version  

---

## 📄 Documentation

📘 **Full project documentation (PDF):**  
➡️ [View documentation](Pedestrian_Traffic_Light_Documentation.pdf)

The documentation includes:
- System overview
- Hardware setup
- Algorithm description
- Flowcharts
- Source code explanation
- Future improvements

## ▶️ How to run
1. Clone the repository on a Raspberry Pi:
   ```bash
   git clone https://github.com/Miroslava2003/Raspberry-Pi-3-mobel-b.git


## 📽 Demo Video
🎥 **Project demonstration video:**  
➡️ [Watch the demo](https://drive.google.com/file/d/1tWKDmGcqH2fJHqTLqak5dqiveMtW-ieS/view?usp=sharing)

*(The video demonstrates the traffic light system running on Raspberry Pi 3 Model B.)*
