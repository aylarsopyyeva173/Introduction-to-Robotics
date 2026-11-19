# Homework 6 – Three-Floor Elevator Controller

## 1. Project Description

For this homework, I developed a three-floor elevator controller using an ESP32 development board.

The system allows users to request one of three floors by pressing the corresponding buttons. The elevator processes the requests in the order they are received and moves to the selected floor.

I implemented the control logic using a state machine and a FIFO (First In, First Out) queue. I also used LEDs to indicate floor requests and elevator movement, a seven-segment display to show the current floor, and a passive buzzer to provide sound signals.

## 2. Hardware Components

The following components were used in this project:

- ESP32 development board
- 74HC595 shift register
- 3 push buttons
- 4 LEDs (3 floor indicator LEDs and 1 movement LED)
- Single-digit seven-segment display
- Passive buzzer
- NPN transistor
- Resistors
- Breadboard and jumper wires

## 3. Pin Connections

### Push Buttons

| Button | ESP32 GPIO |
|---|---|
| Floor 1 | GPIO 25 |
| Floor 2 | GPIO 26 |
| Floor 3 | GPIO 27 |

The buttons are connected to GND, and the internal pull-up resistors of the ESP32 are enabled.

### 74HC595 Shift Register

| 74HC595 Signal | ESP32 GPIO |
|---|---|
| DS (Data) | GPIO 22 |
| SHCP (Clock) | GPIO 18 |
| STCP (Latch) | GPIO 5 |

The shift register controls the four LEDs.

| LED | 74HC595 Output |
|---|---|
| Floor 1 LED | Q0 |
| Floor 2 LED | Q1 |
| Floor 3 LED | Q2 |
| Movement LED | Q3 |

### Seven-Segment Display

| Segment | ESP32 GPIO |
|---|---|
| A | GPIO 4 |
| B | GPIO 16 |
| C | GPIO 17 |
| D | GPIO 19 |
| E | GPIO 21 |
| F | GPIO 23 |
| G | GPIO 32 |

### Buzzer

| Component | ESP32 GPIO |
|---|---|
| Passive buzzer control | GPIO 33 |

The passive buzzer is controlled through an NPN transistor.

## 4. How the Elevator Works

When the system starts, the elevator is positioned on the first floor, and the seven-segment display shows the number 1.

When a user presses one of the floor buttons, the system detects the button press and adds the requested floor to the queue.

The elevator does not process all requests immediately. Instead, it stores them and handles them one by one in FIFO order. This means that the first floor request received is the first one processed.

The system also checks whether a floor has already been requested. Duplicate requests are not added to the queue.

The floor indicator LEDs show the requested floors. While the elevator is moving, the movement LED blinks. When the elevator reaches its destination, the display is updated and the buzzer produces an arrival sound.

## 5. State Machine

I used four states to control the elevator:

### 1. DOORS_OPEN
The elevator doors are open. The system waits for the next request.

### 2. DOORS_CLOSING
The elevator closes its doors before starting to move.

### 3. MOVING
The elevator moves from its current floor to the requested floor. During this state, the movement LED blinks.

### 4. ARRIVING
The elevator has reached the requested floor. The buzzer produces an arrival signal, and after a short time, the system returns to the DOORS_OPEN state.

The elevator changes between these states according to the elapsed time and its current condition.

## 6. FIFO Request Queue

I implemented a FIFO queue to store floor requests.

When a button is pressed, the requested floor is added to the end of the queue. The elevator takes the first request from the queue and processes it before moving to the next one.

The system also prevents duplicate requests, so the same floor cannot be added multiple times while it is already waiting or being served.

This allows the elevator to handle multiple requests without losing them while it is busy.

## 7. Timing and Button Debouncing

I used `esp_timer_get_time()` to measure elapsed time in milliseconds.

Instead of using long blocking delays, I implemented the elevator timing with time checks inside the main loop. This allows the system to continue checking button inputs while the elevator is operating.

I also implemented button debouncing with a 50 ms interval to avoid detecting one physical button press multiple times.

## 8. Buzzer Control

The passive buzzer is controlled using the ESP32 LEDC peripheral.

I configured an LEDC timer and channel to generate different sound frequencies. The buzzer produces sound signals during the door-closing and arrival stages.

## 9. Software and Development Environment

I developed this project using:

- Visual Studio Code
- PlatformIO
- ESP32
- Native ESP-IDF framework
- C programming language

The project uses the native ESP-IDF framework instead of the Arduino framework.


The `main.c` file contains the complete elevator control program, including GPIO configuration, button handling, FIFO queue management, shift register communication, display control, buzzer control, and the state machine.

## 10. Screenshots and Hardware Demonstration

### Hardware Setup

This picture shows my completed elevator controller circuit with the ESP32 and the connected components.

![Hardware Setup](media/elevator_photo.jpg)

### Serial Monitor

I used the ESP-IDF logging system to follow the elevator's operation. The Serial Monitor displays button presses, received floor requests, elevator movement, and arrival messages.

![Serial Monitor](media/serial_monitor.png)

## 11. Demonstration Video

The demonstration video shows how the elevator responds to floor requests, processes multiple requests, changes floors, and uses the LEDs, seven-segment display, and buzzer.

[Watch the demonstration video](ADD_VIDEO_LINK_HERE)

## 12. Conclusion

During this homework, I gained practical experience with the ESP32 and the native ESP-IDF framework.

I learned how to configure GPIO pins, read button inputs, control LEDs using a 74HC595 shift register, display numbers on a seven-segment display, and generate different buzzer tones using LEDC.

I also practiced implementing a state machine, managing requests with a FIFO queue, and using non-blocking timing in an embedded system.

Overall, this project helped me understand how different hardware components and software functions can work together to control a simple elevator system.
