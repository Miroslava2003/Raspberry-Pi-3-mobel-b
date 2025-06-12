
#include <pthread.h>
#include <signal.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <cstdio>
#include <unistd.h>  
#include <atomic>

constexpr int CAR_GREEN      = 17;
constexpr int CAR_YELLOW     = 27;
constexpr int CAR_RED        = 22;
constexpr int PED_RED        = 25;
constexpr int PED_GREEN      = 16;
constexpr int BUTTON_PIN     = 26;
constexpr int BUZZER_PIN     = 21;
constexpr int SH1106_I2C_ADDR = 0x3C;

int fd = -1;

volatile bool work = true;
volatile bool pedestrian_request = false;
volatile bool timer_running = false;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

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

void clearDisplay() {
    for (int page = 0; page < 8; ++page) {
        sendCommand(0xB0 + page);
        sendCommand(0x00);
        sendCommand(0x10);
        for (int col = 0; col < 132; ++col) {
            sendData(0x00);
        }
    }
}

void turnOffDisplay() {
    sendCommand(0xAE); 
}

void setCursor(int page, int col) {
    sendCommand(0xB0 + page);            
    sendCommand(0x00 + (col & 0x0F));   
    sendCommand(0x10 + ((col >> 4) & 0x0F)); 
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
        sendData(bigDigits16x8[digit][i * 2]);
    }
    setCursor(page + 1, col);
    for (int i = 0; i < 8; ++i) {
        sendData(bigDigits16x8[digit][i * 2 + 1]);
    }
}

struct TimerArgs {
    int seconds;
};

void* countdownTimerThread(void* arg) {
    TimerArgs* args = (TimerArgs*)arg;
    int seconds_count = args->seconds;

    digitalWrite(BUZZER_PIN, HIGH);  
    timer_running = true;

    for (int i = seconds_count; i >= 0 && work && timer_running; --i) {
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

        for (int ms = 0; ms < 1000 && work && timer_running; ms += 100) {
            usleep(100 * 1000);
        }
    }

    digitalWrite(BUZZER_PIN, LOW);   
    clearDisplay();

    timer_running = false;
    delete args;
    return nullptr;
}


void pedestrianSequence() {
    printf("Стартирана е пешеходна последователност\n");
    sleep(5); 

    digitalWrite(CAR_GREEN, LOW);
    digitalWrite(CAR_YELLOW, HIGH);
    sleep(2);

    digitalWrite(CAR_YELLOW, LOW);
    digitalWrite(CAR_RED, HIGH);
    sleep(2);

    digitalWrite(PED_RED, LOW);
    digitalWrite(PED_GREEN, HIGH);

    pthread_t timer_thread;
    TimerArgs* args = new TimerArgs{20};
    if (pthread_create(&timer_thread, nullptr, &countdownTimerThread, args) != 0) {
        fprintf(stderr, "Неуспешно стартиране на нишката на таймера\n");
        delete args;
        return;
    }
    pthread_join(timer_thread, nullptr);

    digitalWrite(PED_GREEN, LOW);
    digitalWrite(PED_RED, HIGH);
    sleep(5);

    digitalWrite(CAR_RED, LOW);
    digitalWrite(CAR_YELLOW, HIGH);
    sleep(2);

    digitalWrite(CAR_YELLOW, LOW);
    digitalWrite(CAR_GREEN, HIGH);

    printf("Пешеходната поредица приключи \n");
}

void* trafficLightController(void* arg) {
    while (work) {
        pthread_mutex_lock(&mutex);
        while (!pedestrian_request && work) {
            pthread_cond_wait(&cond, &mutex);
        }
        if (!work) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pedestrian_request = false;
        pthread_mutex_unlock(&mutex);

        pedestrianSequence();
    }
    return nullptr;
}

void buttonISR() {
    pthread_mutex_lock(&mutex);
    if (!pedestrian_request && !timer_running) {  
        pedestrian_request = true;
        pthread_cond_signal(&cond);
        printf("Бутон натиснат - заявката за пешеходци е зададена \n");
    } else {
        printf("Бутонът е натиснат, но таймерът работи или заявката е вече зададена - игнорира се \n");
    }
    pthread_mutex_unlock(&mutex);
}


void handle_sigint(int sig) {
    pthread_mutex_lock(&mutex);
    work = false;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    printf("\nSIGINT получено, спиране на програмата...\n");
}

int main() {
    signal(SIGINT, handle_sigint);

    if (wiringPiSetupGpio() == -1) {
        fprintf(stderr, " Грешка при инициализиране на wiringPi GPIO\n");
        return 1;
    }

    fd = wiringPiI2CSetup(SH1106_I2C_ADDR);
    if (fd == -1) {
        fprintf(stderr, "Грешка при инициализиране на I2C display\n");
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

    pthread_t thread_id;
    if (pthread_create(&thread_id, nullptr, &trafficLightController, nullptr) != 0) {
        fprintf(stderr, "Създаването на нишка не бе успешно\n");
        return 1;
    }

    while (work) {
        sleep(1);
    }

    pthread_join(thread_id, nullptr);

    clearDisplay();
    turnOffDisplay();

    printf("Програмата приключи\n");
    return 0;
}
