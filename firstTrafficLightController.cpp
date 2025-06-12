#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <mutex>
#include <condition_variable>

using namespace std;
using namespace std::chrono;
using namespace std::this_thread;

constexpr int CAR_GREEN      = 17;
constexpr int CAR_YELLOW     = 27;
constexpr int CAR_RED        = 22;
constexpr int PED_RED        = 25;
constexpr int PED_GREEN      = 16;
constexpr int BUTTON_PIN     = 26;
constexpr int BUZZER_PIN     = 21;
constexpr int SH1106_I2C_ADDR = 0x3C;

int fd = -1;
atomic<bool> work(true);
atomic<bool> pedestrian_request(false);
mutex mtx;
condition_variable cv;
atomic<bool> countdown_running(false);
atomic<steady_clock::time_point> last_button_press_time(steady_clock::now());

void sendCommand(uint8_t cmd) {
    wiringPiI2CWriteReg8(fd, 0x00, cmd);
}

void sendData(uint8_t data) {
    wiringPiI2CWriteReg8(fd, 0x40, data);
}

void initDisplay() {
    sendCommand(0xAE);
    sendCommand(0xD5); sendCommand(0x80);
    sendCommand(0xA8); sendCommand(0x3F);
    sendCommand(0xD3); sendCommand(0x00);
    sendCommand(0x40);
    sendCommand(0xAD); sendCommand(0x8B);
    sendCommand(0xA1);
    sendCommand(0xC8);
    sendCommand(0xDA); sendCommand(0x12);
    sendCommand(0x81); sendCommand(0xCF);
    sendCommand(0xD9); sendCommand(0xF1);
    sendCommand(0xDB); sendCommand(0x40);
    sendCommand(0xA4);
    sendCommand(0xA6);
    sendCommand(0xAF);
}

void turnOffDisplay() {
    sendCommand(0xAE); 
}

void setCursor(int page, int col) {
    sendCommand(0xB0 + page);
    sendCommand(0x00 + (col & 0x0F));
    sendCommand(0x10 + ((col >> 4) & 0x0F));
}

void clearDisplay() {
    for (int page = 0; page < 8; ++page) {
        setCursor(page, 0);
        for (int col = 0; col < 132; ++col) {
            sendData(0x00);
        }
    }
}

const uint8_t bigDigits16x8[10][16] = {
    {0x3C,0x00,0x42,0x00,0x81,0x00,0x81,0x00,0x81,0x00,0x81,0x00,0x42,0x00,0x3C,0x00}, 
    {0x00,0x00,0x84,0x00,0xFE,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 
    {0xC2,0x00,0xA1,0x00,0x91,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x86,0x00,0x00,0x00}, 
    {0x42,0x00,0x81,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x76,0x00,0x00,0x00}, 
    {0x30,0x00,0x28,0x00,0x24,0x00,0x22,0x00,0xFE,0x00,0x20,0x00,0x20,0x00,0x00,0x00}, 
    {0x4F,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x71,0x00,0x00,0x00}, 
    {0x7E,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x72,0x00,0x00,0x00}, 
    {0x01,0x00,0xE1,0x00,0x11,0x00,0x09,0x00,0x05,0x00,0x03,0x00,0x01,0x00,0x00,0x00}, 
    {0x76,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x76,0x00,0x00,0x00}, 
    {0x06,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x89,0x00,0x49,0x00,0x3E,0x00,0x00,0x00}  
};

void drawBigDigit16x8(int page, int col, int digit) {
    if (digit < 0 || digit > 9) return;
    setCursor(page, col);
    for (int i = 0; i < 8; ++i) {
        sendData(bigDigits16x8[digit][i*2]);
    }
    setCursor(page + 1, col);
    for (int i = 0; i < 8; ++i) {
        sendData(bigDigits16x8[digit][i*2 + 1]);
    }
}

void countdownDisplayWithBuzzer(int seconds_count = 20) {
    countdown_running = true;
    digitalWrite(BUZZER_PIN, HIGH);

    for (int i = seconds_count; i >= 0 && work; --i) {
        for (int page = 3; page <= 4; ++page) {
            setCursor(page, 44);
            for (int col = 44; col < 60; ++col) {
                sendData(0x00);
            }
        }

        int tens = i / 10;
        int ones = i % 10;
        drawBigDigit16x8(3, 44, tens);
        drawBigDigit16x8(3, 52, ones);

        sleep_for(seconds(1));
    }
	digitalWrite(BUZZER_PIN, LOW);
    clearDisplay(); 
    countdown_running = false;;
}

void pedestrianSequence() {
    sleep_for(seconds(5)); 
    digitalWrite(CAR_GREEN, LOW);
    digitalWrite(CAR_YELLOW, HIGH);
    sleep_for(seconds(2));

    digitalWrite(CAR_YELLOW, LOW);
    digitalWrite(CAR_RED, HIGH);
    sleep_for(seconds(2));

    digitalWrite(PED_RED, LOW);
    digitalWrite(PED_GREEN, HIGH);

    countdownDisplayWithBuzzer(20);

    digitalWrite(PED_GREEN, LOW);
    digitalWrite(PED_RED, HIGH);
    sleep_for(seconds(5)); 

    digitalWrite(CAR_RED, LOW);
    digitalWrite(CAR_YELLOW, HIGH);
    sleep_for(seconds(2));

    digitalWrite(CAR_YELLOW, LOW);
    digitalWrite(CAR_GREEN, HIGH);
}


void trafficLightController() {
    while (work) {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [] { return pedestrian_request.load() || !work; });
        if (!work) break;

        pedestrian_request = false;
        lock.unlock();

        pedestrianSequence();
    }
}

void handle_sigint(int sig) {
    work = false;
    cv.notify_all();
}

void buttonISR() {
    auto now = steady_clock::now();
    auto last = last_button_press_time.load();

    if (duration_cast<milliseconds>(now - last).count() > 200) {
        last_button_press_time = now;
        if (!countdown_running.load()) {
            pedestrian_request = true;
            cv.notify_one();
        }
    }
}

int main() {
    signal(SIGINT, handle_sigint);

    if (wiringPiSetupGpio() == -1) {
        cerr << "Грешка при инициализация на wiringPi GPIO" << endl;
        return 1;
    }

    fd = wiringPiI2CSetup(SH1106_I2C_ADDR);
    if (fd == -1) {
        cerr << "Грешка при инициализация на I2C дисплей" << endl;
        return 1;
    }

    pinMode(CAR_GREEN, OUTPUT);
    pinMode(CAR_YELLOW, OUTPUT);
    pinMode(CAR_RED, OUTPUT);
    pinMode(PED_RED, OUTPUT);
    pinMode(PED_GREEN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    pinMode(BUTTON_PIN, INPUT);
    pullUpDnControl(BUTTON_PIN, PUD_UP);
    wiringPiISR(BUTTON_PIN, INT_EDGE_FALLING, &buttonISR);

    digitalWrite(CAR_GREEN, HIGH);
    digitalWrite(CAR_YELLOW, LOW);
    digitalWrite(CAR_RED, LOW);
    digitalWrite(PED_RED, HIGH);
    digitalWrite(PED_GREEN, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    initDisplay();
    clearDisplay();

    cout << "Стартиране на светофар с пешеходен бутон..." << endl;

    thread controller_thread(trafficLightController);

    while (work) {
        sleep_for(milliseconds(100));
    }

    cv.notify_all();
    controller_thread.join();

    digitalWrite(CAR_GREEN, LOW);
    digitalWrite(CAR_YELLOW, LOW);
    digitalWrite(CAR_RED, LOW);
    digitalWrite(PED_GREEN, LOW);
    digitalWrite(PED_RED, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    clearDisplay();
    turnOffDisplay(); 
    cout << "Програмата приключи." << endl;
    return 0;
}
