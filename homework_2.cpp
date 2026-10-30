#include <Arduino.h>

// Pin Tanımlamaları
const int fanLED = 18;    
const int ledBlue = 25;  
const int ledGreen = 26;
const int ledRed = 27;    
const int potPin = 34;    
const int buzzerPin = 23; 

// Dahili LED (Heartbeat için millis)
const int builtInLED = 2; //[cite: 2]
unsigned long previousMillis = 0;
const long interval = 500;
bool ledState = LOW;

bool buzzerTriggered = false;

void setup() {
  Serial.begin(115200);

  pinMode(fanLED, OUTPUT);
  pinMode(ledBlue, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  pinMode(ledRed, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(builtInLED, OUTPUT);


  ledcAttach(fanLED, 5000, 8); 

  digitalWrite(buzzerPin, LOW);
}

void loop() {
 
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(builtInLED, ledState);
  }

 
  int potValue = analogRead(potPin);
  

  int pwmValue = map(potValue, 0, 4095, 0, 255);
  ledcWrite(fanLED, pwmValue);

  Serial.print("Pot Değeri: ");
  Serial.println(potValue);

 
  if (potValue < 1360) {
    digitalWrite(ledBlue, HIGH);
    digitalWrite(ledGreen, LOW);
    digitalWrite(ledRed, LOW);
    
    buzzerTriggered = false; 
  } 
  else if (potValue >= 1360 && potValue < 2720) {
    digitalWrite(ledBlue, LOW);
    digitalWrite(ledGreen , HIGH);
    digitalWrite(ledRed, LOW);
    
    buzzerTriggered = false; 
  } 
  else {
    // En Yüksek Seviye: Sadece Kırmızı LED (GPIO 27) yanar ve buzzer bir kere öter
    digitalWrite(ledBlue, LOW);
    digitalWrite(ledGreen, LOW);
    digitalWrite(ledRed, HIGH);

    if (!buzzerTriggered) {
      digitalWrite(buzzerPin, HIGH);
      delay(100);                    // Kısa bir bip sesi
      digitalWrite(buzzerPin, LOW);
      buzzerTriggered = true;        // Sürekli ötmesini engelle
    }
  }

  delay(20); // Kararlılık gecikmesi
}
