
#include <Arduino.h>

// 74HC595
const uint8_t dataPin = 23;
const uint8_t clockPin = 18;
const uint8_t latchPin = 22;

// Four-digit display
const uint8_t displayDigits[4] = {25, 26, 27, 33};

// Joystick
const uint8_t JOY_X = 34;
const uint8_t JOY_Y = 35;
const uint8_t JOY_SW = 32;

const int TARGET = 5000;
const int START_VALUE = 9999;

const unsigned long COUNTDOWN_INTERVAL = 2;
const unsigned long DEBOUNCE_TIME = 30;
const unsigned long DISPLAY_INTERVAL = 2;
const unsigned long SERIAL_INTERVAL = 200;

const bool COMMON_ANODE = false;

const uint8_t DIGIT_ON = COMMON_ANODE ? HIGH : LOW;
const uint8_t DIGIT_OFF = COMMON_ANODE ? LOW : HIGH;

const uint8_t numbers[10] = {
    0b11111100, // 0
    0b01100000, // 1
    0b11011010, // 2
    0b11110010, // 3
    0b01100110, // 4
    0b10110110, // 5
    0b10111110, // 6
    0b11100000, // 7
    0b11111110, // 8
    0b11110110  // 9
};

const uint8_t BLANK = 0b00000000;

enum GameState {
    READY,
    RUNNING,
    STOPPED
};

GameState gameState = READY;

int currentValue = TARGET;
int stoppedValue = 0;
int errorValue = 0;

unsigned long gameStartTime = 0;
unsigned long lastCountdownTime = 0;
unsigned long lastDisplayTime = 0;
unsigned long lastSerialTime = 0;

uint8_t currentDigit = 0;
uint8_t digitPatterns[4];

int lastJoystickAction = 0;

int lastButtonReading = HIGH;
int stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;

void writeRegister(uint8_t value) {

    digitalWrite(latchPin, LOW);

    for (uint8_t mask = 0x80; mask > 0; mask >>= 1) {

        digitalWrite(clockPin, LOW);

        digitalWrite(dataPin,
            (value & mask) ? HIGH : LOW);

        digitalWrite(clockPin, HIGH);
    }

    digitalWrite(latchPin, HIGH);
}

void prepareNumber(int value) {

    uint8_t digits[4];

    digits[0] = value / 1000;
    digits[1] = (value / 100) % 10;
    digits[2] = (value / 10) % 10;
    digits[3] = value % 10;

    bool leadingZero = true;

    for (uint8_t i = 0; i < 4; i++) {

        if (leadingZero && digits[i] == 0 && i < 3) {
            digitPatterns[i] = BLANK;
        }
        else {
            leadingZero = false;
            digitPatterns[i] = numbers[digits[i]];
        }

        if (COMMON_ANODE) {
            digitPatterns[i] = ~digitPatterns[i];
        }
    }
}

void updateDisplay() {

    unsigned long now = millis();

    if (now - lastDisplayTime < DISPLAY_INTERVAL) {
        return;
    }

    lastDisplayTime = now;

    for (uint8_t i = 0; i < 4; i++) {
        digitalWrite(displayDigits[i], DIGIT_OFF);
    }

    writeRegister(digitPatterns[currentDigit]);

    digitalWrite(displayDigits[currentDigit], DIGIT_ON);

    currentDigit = (currentDigit + 1) % 4;
}

void startGame() {

    if (gameState != READY) {
        return;
    }

    currentValue = START_VALUE;

    gameStartTime = millis();
    lastCountdownTime = gameStartTime;
    lastSerialTime = gameStartTime;

    gameState = RUNNING;

    Serial.println();
    Serial.println("GAME STARTED!");
    Serial.print("Target: ");
    Serial.println(TARGET);
}

void calculateScore() {

    errorValue = abs(stoppedValue - TARGET);

    Serial.println();
    Serial.println("===== GAME RESULT =====");

    Serial.print("Target: ");
    Serial.println(TARGET);

    Serial.print("Stopped value: ");
    Serial.println(stoppedValue);

    Serial.print("Error: ");
    Serial.println(errorValue);

    if (errorValue == 0) {
        Serial.println("PERFECT SCORE!");
    }

    Serial.println("=======================");
}

void stopGame() {

    if (gameState != RUNNING) {
        return;
    }

    stoppedValue = currentValue;

    gameState = STOPPED;

    calculateScore();
}

void resetGame() {

    gameState = READY;

    currentValue = TARGET;

    stoppedValue = 0;
    errorValue = 0;

    Serial.println();
    Serial.println("GAME RESET!");
    Serial.print("Target: ");
    Serial.println(TARGET);
}

void updateCountdown() {

    if (gameState != RUNNING) {
        return;
    }

    unsigned long now = millis();

    if (now - lastCountdownTime >= COUNTDOWN_INTERVAL) {

        unsigned long elapsed = now - gameStartTime;

        currentValue = START_VALUE - elapsed / COUNTDOWN_INTERVAL;

        if (currentValue <= 0) {

            currentValue = 0;
            stoppedValue = 0;

            gameState = STOPPED;

            Serial.println("TIME EXPIRED!");

            calculateScore();
        }

        lastCountdownTime = now;
    }

    // Print current value every 200 ms
    if (gameState == RUNNING &&
        now - lastSerialTime >= SERIAL_INTERVAL) {

        Serial.print("Counter: ");
        Serial.println(currentValue);

        lastSerialTime = now;
    }
}

void readJoystick() {

    int yValue = analogRead(JOY_Y);

    int action = 0;

    if (yValue < 1200) {
        action = 1;
    }
    else if (yValue > 2900) {
        action = -1;
    }

    if (action != 0 && lastJoystickAction == 0) {

        if (action == 1) {
            startGame();
        }
        else if (action == -1) {
            resetGame();
        }
    }

    if (yValue > 1400 && yValue < 2700) {
        lastJoystickAction = 0;
    }
    else if (action != 0) {
        lastJoystickAction = action;
    }
}

void readButton() {

    int reading = digitalRead(JOY_SW);

    if (reading != lastButtonReading) {
        lastDebounceTime = millis();
    }

    if (millis() - lastDebounceTime > DEBOUNCE_TIME) {

        if (reading != stableButtonState) {

            stableButtonState = reading;

            if (stableButtonState == LOW) {
                stopGame();
            }
        }
    }

    lastButtonReading = reading;
}

void setup() {

    Serial.begin(115200);

    pinMode(dataPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(latchPin, OUTPUT);

    for (uint8_t i = 0; i < 4; i++) {

        pinMode(displayDigits[i], OUTPUT);

        digitalWrite(displayDigits[i], DIGIT_OFF);
    }

    pinMode(JOY_SW, INPUT_PULLUP);

    analogReadResolution(12);

    currentValue = TARGET;

    prepareNumber(currentValue);

    Serial.println("STOP THE COUNTER!");
    Serial.print("Target: ");
    Serial.println(TARGET);

    Serial.println("UP = START");
    Serial.println("BUTTON = STOP");
    Serial.println("DOWN = RESET");
}

void loop() {

    readJoystick();

    readButton();

    updateCountdown();

    prepareNumber(currentValue);

    updateDisplay();
}
