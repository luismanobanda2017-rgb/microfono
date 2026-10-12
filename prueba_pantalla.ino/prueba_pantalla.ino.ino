#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#define USE_TFT_ESPI_LIBRARY
#include <lv_xiao_round_screen.h>

// ❌ QUITA ESTA LÍNEA:
// TFT_eSPI tft = TFT_eSPI();

// La librería ya tiene su propio "tft"

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("PRUEBA PANTALLA REDONDA");

  lv_xiao_disp_init();

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString("HOLA", 120, 90);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("XIAO nRF52840", 120, 150);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Pantalla OK", 120, 190);

  Serial.println("Pantalla inicializada");
}

void loop() {
  delay(2000);
  tft.fillCircle(120, 120, 20, TFT_RED);
  delay(1000);
  tft.fillCircle(120, 120, 20, TFT_BLACK);
  delay(1000);
}
