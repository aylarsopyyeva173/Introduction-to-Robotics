
#include <Arduino.h>

const int BUTTON_PIN = 21;
const int LED_PIN = 18;
const int HEARTBEAT_PIN = 2;

const unsigned long DEBOUNCE_TIME = 30;
const unsigned long LONG_PRESS_TIME = 1000;
const unsigned long BLINK_INTERVAL = 500;
const unsigned long HEARTBEAT_INTERVAL = 500;

enum OperatingMode {
    OFF,
    DIM,
    FULL,
    BLINK
};

OperatingMode currentMode = OFF;

volatile bool buttonInterruptFlag = false;

void IRAM_ATTR buttonISR() {
    buttonInterruptFlag = true;
}

bool debouncePending = false;
bool buttonPressed = false;
bool releaseTiming = false;
bool longPressHandled = false;

int lastRawButtonState = HIGH;

unsigned long debounceStart = 0;
unsigned long pressStart = 0;
unsigned long releaseStart = 0;

unsigned long lastBlinkTime = 0;
unsigned long lastHeartbeatTime = 0;

bool blinkState = false;
bool heartbeatState = false;

void setMode(OperatingMode newMode) {

    currentMode = newMode;

    if (currentMode == BLINK) {
        blinkState = false;
        lastBlinkTime = millis();
        digitalWrite(LED_PIN, LOW);
    }

    Serial.print("Operating Mode: ");

    switch (currentMode) {

        case OFF:
            Serial.println("OFF");
            analogWrite(LED_PIN, 0);
            break;

        case DIM:
            Serial.println("DIM");
            analogWrite(LED_PIN, 64);
            break;

        case FULL:
            Serial.println("FULL");
            analogWrite(LED_PIN, 255);
            break;

        case BLINK:
            Serial.println("BLINK");
            break;
    }
}

void handleShortPress() {

    switch (currentMode) {

        case OFF:
            setMode(DIM);
            break;

        case DIM:
            setMode(FULL);
            break;

        case FULL:
            setMode(BLINK);
            break;

        case BLINK:
            setMode(OFF);
            break;
    }
}

void updateButton() {

    unsigned long now = millis();

    noInterrupts();
    bool interruptDetected = buttonInterruptFlag;
    buttonInterruptFlag = false;
    interrupts();

    if (interruptDetected &&
        !buttonPressed &&
        !debouncePending) {

        debouncePending = true;
        debounceStart = now;
        lastRawButtonState = digitalRead(BUTTON_PIN);
    }

    if (debouncePending) {

        int reading = digitalRead(BUTTON_PIN);

        if (reading != lastRawButtonState) {
            lastRawButtonState = reading;
            debounceStart = now;
        }

        if (now - debounceStart >= DEBOUNCE_TIME) {

            debouncePending = false;

            if (reading == LOW) {
                buttonPressed = true;
                pressStart = now;
                longPressHandled = false;
            }
        }
    }

    if (buttonPressed && !longPressHandled) {

        if (now - pressStart >= LONG_PRESS_TIME) {

            setMode(OFF);
            longPressHandled = true;

            Serial.println("Long Press Detected!");
        }
    }

    if (buttonPressed) {

        int reading = digitalRead(BUTTON_PIN);

        if (reading == HIGH) {

            if (!releaseTiming) {
                releaseTiming = true;
                releaseStart = now;
            }

            if (now - releaseStart >= DEBOUNCE_TIME) {

                buttonPressed = false;
                releaseTiming = false;

                if (!longPressHandled) {
                    handleShortPress();
                    Serial.println("Short Press Detected!");
                }
            }

        } else {
            releaseTiming = false;
        }
    }
}

void updateOperatingLED() {

    unsigned long now = millis();

    switch (currentMode) {

        case OFF:
            analogWrite(LED_PIN, 0);
            break;

        case DIM:
            analogWrite(LED_PIN, 64);
            break;

        case FULL:
            analogWrite(LED_PIN, 255);
            break;

        case BLINK:

            if (now - lastBlinkTime >= BLINK_INTERVAL) {
                lastBlinkTime = now;
                blinkState = !blinkState;
                digitalWrite(LED_PIN, blinkState);
            }

            break;
    }
}

void updateHeartbeat() {

    unsigned long now = millis();

    if (now - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {

        lastHeartbeatTime = now;
        heartbeatState = !heartbeatState;

        digitalWrite(HEARTBEAT_PIN, heartbeatState);
    }
}

void setup() {

    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    pinMode(HEARTBEAT_PIN, OUTPUT);

    digitalWrite(LED_PIN, LOW);
    digitalWrite(HEARTBEAT_PIN, LOW);

    attachInterrupt(
        digitalPinToInterrupt(BUTTON_PIN),
        buttonISR,
        FALLING
    );

    setMode(OFF);

    Serial.println("ESP32 Button Control System Started!");
}

void loop() {

    updateButton();
    updateOperatingLED();
    updateHeartbeat();
}
