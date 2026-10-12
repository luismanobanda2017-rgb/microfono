#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {

  Serial.begin(115200);

  delay(1500);

  Serial.println();
  Serial.println("================================");
  Serial.println("PRUEBA PANTALLA REDONDA XIAO");
  Serial.println("================================");

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // Título
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString("HOLA", 120, 90);

  // Subtítulo
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("XIAO nRF52840", 120, 150);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Pantalla OK", 120, 190);

  Serial.println("Pantalla inicializada");
}

void loop() {

  // Parpadeo simple cada 2 segundos
  delay(2000);

  tft.fillCircle(120, 120, 20, TFT_RED);
  delay(1000);

  tft.fillCircle(120, 120, 20, TFT_BLACK);
  delay(1000);
}