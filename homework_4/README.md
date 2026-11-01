In this project, I used an ESP32 to build a simple fan control and status panel. An external LED simulates the fan, while a push button is used to switch between four operating modes: OFF, DIM, FULL and BLINK.

The project also includes a potentiometer for analog input, an RGB LED for status indication and a passive buzzer for sound feedback.

I used PWM to control the LED brightness and interrupts to detect button presses. A debouncing mechanism prevents unwanted multiple button registrations. For blinking and heartbeat functions, I used millis() instead of delay() so that the system can perform different operations without blocking the main loop.

The project was developed using PlatformIO and the Arduino framework.
