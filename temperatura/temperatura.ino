#include <Arduino.h>
#include <Adafruit_TinyUSB.h>   // <-- ESTA LÍNEA SOLUCIONA EL ERROR
#include <DHT.h>

#define DHTPIN 1
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando DHT22...");
  dht.begin();
}

void loop() {
  delay(2000);

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Error lectura DHT");
    return;
  }

  Serial.print("Temp: ");
  Serial.print(t);
  Serial.print(" C  |  Hum: ");
  Serial.print(h);
  Serial.println(" %");
}