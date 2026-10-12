#include <Arduino.h>
#include <SPI.h>
#include <PDM.h>
#include <RadioLib.h>
//Registro2 pantalla prueba 1
#include <TFT_eSPI.h>

#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;

// ============================================================
// PANTALLA REDONDA
// ============================================================

TFT_eSPI tft = TFT_eSPI();

// ============================================================
// WIO-SX1262
// ============================================================
//
// Wio-SX1262:
// NSS   = D4
// DIO1  = D1
// RESET = D2
// BUSY  = D3
// RXEN  = D5
//
// SPI:
// SCK  = D8
// MISO = D9
// MOSI = D10
//

SX1262 radio = new Module(D4, D1, D2, D3);

// ============================================================
// LORAWAN
// ============================================================

const LoRaWANBand_t Region = AU915;

// Gateway TTN:
// AU915-928
// FSB2
// canales 8-15 + 65
const uint8_t subBand = 2;

LoRaWANNode node(&radio, &Region, subBand);

// ============================================================
// CREDENCIALES OTAA (THE THINGS NETWORK)
// ============================================================

uint64_t joinEUI = 0x0000000000000000ULL;

// DevEUI del XIAO registrado en TTN
uint64_t devEUI = 0x2CF7F1BF8410000CULL;

// AppKey generada por TTN
uint8_t appKey[] = {
  0xE3, 0x93, 0x14, 0xC5,
  0xBA, 0xD5, 0x89, 0x12,
  0x1B, 0xDD, 0x02, 0x61,
  0x01, 0xDE, 0x89, 0x47
};

// ============================================================
// ARCHIVOS DE PERSISTENCIA LORAWAN
// ============================================================

const char* ARCHIVO_NONCES  = "/lorawan_nonces.bin";
const char* ARCHIVO_SESSION = "/lorawan_session.bin";

bool sistemaArchivosOK = false;

// ============================================================
// MICROFONO PDM
// ============================================================

int16_t bufferAudio[256];

volatile uint32_t sumaAbsoluta = 0;
volatile uint32_t cantidadMuestras = 0;
volatile uint16_t valorPico = 0;

// ============================================================
// INTERVALO DE TRANSMISION
// ============================================================

const unsigned long INTERVALO_TX = 30000;

unsigned long ultimaTransmision = 0;

// ============================================================
// PDM CALLBACK
// ============================================================

void recibirPDM() {

  int bytesDisponibles = PDM.available();

  if (bytesDisponibles <= 0) {
    return;
  }

  int bytesLeidos = PDM.read(
    bufferAudio,
    min(bytesDisponibles, (int)sizeof(bufferAudio))
  );

  int muestras =
      bytesLeidos / sizeof(int16_t);

  for (int i = 0; i < muestras; i++) {

    int32_t valor = bufferAudio[i];

    if (valor < 0) {
      valor = -valor;
    }

    sumaAbsoluta += valor;
    cantidadMuestras++;

    if (valor > valorPico) {
      valorPico = valor;
    }
  }
}

// ============================================================
// ERROR FATAL
// ============================================================

void detener(
  const char* etapa,
  int16_t codigo
) {

  Serial.println();

  Serial.print("ERROR ");
  Serial.print(etapa);
  Serial.print(": ");
  Serial.println(codigo);

  while (true) {
    delay(1000);
  }
}

// ============================================================
// PANTALLA - DIBUJAR MENSAJE CENTRADO
// ============================================================

void mostrarEnPantalla(
  const char* linea1,
  const char* linea2,
  const char* linea3,
  const char* linea4
) {

  tft.fillScreen(TFT_BLACK);

  // Titulo
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);

  tft.drawString(
    "MONITOREO",
    120,
    60
  );

  tft.drawString(
    "LABORATORIO",
    120,
    90
  );

  // Separador
  tft.drawFastHLine(
    40,
    115,
    160,
    TFT_DARKGREY
  );

  // Datos
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  tft.drawString(linea1, 120, 140);
  tft.drawString(linea2, 120, 170);
  tft.drawString(linea3, 120, 200);
  tft.drawString(linea4, 120, 230);
}

// ============================================================
// INICIALIZAR MEMORIA FLASH
// ============================================================

bool iniciarMemoria() {

  Serial.println();
  Serial.println(
    "Inicializando almacenamiento persistente..."
  );

  if (!InternalFS.begin()) {

    Serial.println(
      "ERROR: no se pudo montar InternalFS."
    );

    return false;
  }

  Serial.println("InternalFS OK");

  return true;
}

// ============================================================
// GUARDAR BUFFER EN FLASH
// ============================================================

bool guardarBuffer(
  const char* nombre,
  const uint8_t* datos,
  size_t longitud
) {

  if (!sistemaArchivosOK) {
    return false;
  }

  InternalFS.remove(nombre);

  // ⚠️ FIX: usar namespace completo para evitar conflicto
  // entre Adafruit_LittleFS_Namespace::File y fs::File
  Adafruit_LittleFS_Namespace::File archivo(InternalFS);

  if (!archivo.open(nombre, FILE_O_WRITE)) {

    Serial.print(
      "ERROR abriendo para escritura: "
    );

    Serial.println(nombre);

    return false;
  }

  size_t escritos =
      archivo.write(datos, longitud);

  archivo.close();

  if (escritos != longitud) {

    Serial.print(
      "ERROR escritura incompleta: "
    );

    Serial.println(nombre);

    return false;
  }

  return true;
}

// ============================================================
// LEER BUFFER DESDE FLASH
// ============================================================

bool leerBuffer(
  const char* nombre,
  uint8_t* datos,
  size_t longitud
) {

  if (!sistemaArchivosOK) {
    return false;
  }

  // ⚠️ FIX: usar namespace completo para evitar conflicto
  // entre Adafruit_LittleFS_Namespace::File y fs::File
  Adafruit_LittleFS_Namespace::File archivo(InternalFS);

  if (!archivo.open(nombre, FILE_O_READ)) {
    return false;
  }

  if ((size_t)archivo.size() != longitud) {

    archivo.close();

    Serial.print(
      "Archivo incompatible: "
    );

    Serial.println(nombre);

    return false;
  }

  size_t leidos =
      archivo.read(datos, longitud);

  archivo.close();

  return (leidos == longitud);
}

// ============================================================
// GUARDAR NONCES
// ============================================================

bool guardarNonces() {

  uint8_t* buffer =
      node.getBufferNonces();

  bool resultado = guardarBuffer(
    ARCHIVO_NONCES,
    buffer,
    RADIOLIB_LORAWAN_NONCES_BUF_SIZE
  );

  if (resultado) {

    Serial.println(
      "LoRaWAN NONCES guardados en Flash."
    );
  }

  return resultado;
}

// ============================================================
// GUARDAR SESION
// ============================================================

bool guardarSesion() {

  uint8_t* buffer =
      node.getBufferSession();

  bool resultado = guardarBuffer(
    ARCHIVO_SESSION,
    buffer,
    RADIOLIB_LORAWAN_SESSION_BUF_SIZE
  );

  if (resultado) {

    Serial.println(
      "Sesion LoRaWAN guardada en Flash."
    );
  }

  return resultado;
}

// ============================================================
// RESTAURAR NONCES
// ============================================================

bool restaurarNonces() {

  uint8_t buffer[
    RADIOLIB_LORAWAN_NONCES_BUF_SIZE
  ];

  if (!leerBuffer(
        ARCHIVO_NONCES,
        buffer,
        sizeof(buffer)
      )) {

    Serial.println(
      "No existen NONCES LoRaWAN guardados."
    );

    return false;
  }

  int16_t estado =
      node.setBufferNonces(buffer);

  if (estado != RADIOLIB_ERR_NONE) {

    Serial.print(
      "NONCES guardados no validos: "
    );

    Serial.println(estado);

    return false;
  }

  Serial.println(
    "NONCES LoRaWAN restaurados."
  );

  return true;
}

// ============================================================
// RESTAURAR SESION
// ============================================================

bool restaurarSesion() {

  uint8_t buffer[
    RADIOLIB_LORAWAN_SESSION_BUF_SIZE
  ];

  if (!leerBuffer(
        ARCHIVO_SESSION,
        buffer,
        sizeof(buffer)
      )) {

    Serial.println(
      "No existe sesion LoRaWAN guardada."
    );

    return false;
  }

  int16_t estado =
      node.setBufferSession(buffer);

  if (estado != RADIOLIB_ERR_NONE) {

    Serial.print(
      "Sesion guardada no valida: "
    );

    Serial.println(estado);

    return false;
  }

  Serial.println(
    "Sesion LoRaWAN cargada desde Flash."
  );

  return true;
}

// ============================================================
// INICIALIZAR SX1262 CON REINTENTOS
// ============================================================

void configurarRadio() {

  Serial.println();
  Serial.println("Inicializando SX1262...");

  delay(1500);

  const int MAX_INTENTOS = 5;

  for (
    int intento = 1;
    intento <= MAX_INTENTOS;
    intento++
  ) {

    Serial.print("Intento ");
    Serial.print(intento);
    Serial.print("/");
    Serial.println(MAX_INTENTOS);

    int16_t estado = radio.begin(
      915.0,
      125.0,
      9,
      7,
      RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
      10,
      8,
      1.8,
      false
    );

    if (estado == RADIOLIB_ERR_NONE) {

      estado =
          radio.setDio2AsRfSwitch(true);

      if (estado != RADIOLIB_ERR_NONE) {

        Serial.print(
          "setDio2AsRfSwitch(): "
        );

        Serial.println(estado);

        delay(1000);

        continue;
      }

      radio.setRfSwitchPins(
        D5,
        RADIOLIB_NC
      );

      Serial.println("SX1262 OK");

      return;
    }

    Serial.print(
      "radio.begin() fallo: "
    );

    Serial.println(estado);

    Serial.println(
      "Esperando antes de reintentar..."
    );

    delay(1500);
  }

  detener(
    "No fue posible iniciar SX1262",
    -20
  );
}

// ============================================================
// CONFIGURAR LORAWAN
// ============================================================

void configurarLoRaWAN() {

  Serial.println();
  Serial.println(
    "Configurando LoRaWAN AU915..."
  );

  Serial.print("SubBand: ");
  Serial.println(subBand);

  // ----------------------------------------------------------
  // 1. CONFIGURAR CREDENCIALES
  // ----------------------------------------------------------

  int16_t estado =
      node.beginOTAA(
        joinEUI,
        devEUI,
        NULL,
        appKey
      );

  if (estado != RADIOLIB_ERR_NONE) {

    detener(
      "node.beginOTAA()",
      estado
    );
  }

  // ----------------------------------------------------------
  // 2. RESTAURAR NONCES
  // ----------------------------------------------------------

  bool noncesRestaurados =
      restaurarNonces();

  // ----------------------------------------------------------
  // 3. RESTAURAR SESION
  // ----------------------------------------------------------

  bool sesionRestaurada = false;

  if (noncesRestaurados) {

    sesionRestaurada =
        restaurarSesion();
  }

  Serial.println();

  if (sesionRestaurada) {

    Serial.println(
      "Intentando restaurar sesion LoRaWAN..."
    );

  } else {

    Serial.println(
      "Intentando JOIN OTAA..."
    );
  }

  // ----------------------------------------------------------
  // 4. ACTIVAR / RESTAURAR
  // ----------------------------------------------------------

  estado = node.activateOTAA();

  guardarNonces();

  // ----------------------------------------------------------
  // 5. COMPROBAR RESULTADO
  // ----------------------------------------------------------

  if (
    estado ==
      RADIOLIB_LORAWAN_NEW_SESSION
  ) {

    Serial.println();
    Serial.println(
      "JOIN LORAWAN EXITOSO"
    );

    guardarSesion();

  }

  else if (
    estado ==
      RADIOLIB_LORAWAN_SESSION_RESTORED
  ) {

    Serial.println();
    Serial.println(
      "SESION LORAWAN RESTAURADA"
    );

    guardarSesion();

  }

  else {

    Serial.println();

    Serial.print(
      "JOIN/RESTORE fallido. Codigo: "
    );

    Serial.println(estado);

    Serial.println(
      "NONCES ya fueron guardados."
    );

    detener(
      "node.activateOTAA()",
      estado
    );
  }

  Serial.print("Region: AU915");
  Serial.println();

  Serial.print("SubBand: ");
  Serial.println(subBand);

  Serial.println(
    "================================="
  );
}

// ============================================================
// CONFIGURAR MICROFONO
// ============================================================

void configurarMicrofono() {

  Serial.println();
  Serial.println(
    "Inicializando microfono PDM..."
  );

  PDM.onReceive(recibirPDM);

  PDM.setBufferSize(512);

  PDM.setGain(40);

  if (!PDM.begin(1, 16000)) {

    Serial.println(
      "ERROR inicializando PDM"
    );

    while (true) {
      delay(1000);
    }
  }

  Serial.println("Microfono PDM OK");
}

// ============================================================
// MOSTRAR PAYLOAD
// ============================================================

void mostrarPayload(
  uint8_t* payload,
  size_t longitud
) {

  Serial.print("Payload HEX = ");

  for (
    size_t i = 0;
    i < longitud;
    i++
  ) {

    if (payload[i] < 0x10) {
      Serial.print("0");
    }

    Serial.print(
      payload[i],
      HEX
    );

    Serial.print(" ");
  }

  Serial.println();
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  unsigned long inicio =
      millis();

  while (
    !Serial &&
    millis() - inicio < 5000
  ) {

    delay(10);
  }

  // ----------------------------------------------------------
  // FLASH
  // ----------------------------------------------------------

  sistemaArchivosOK =
      iniciarMemoria();

  if (!sistemaArchivosOK) {

    Serial.println();
    Serial.println("ADVERTENCIA:");
    Serial.println("No hay almacenamiento persistente.");
    Serial.println("No se continuara para evitar reutilizar DevNonce.");

    while (true) {
      delay(1000);
    }
  }

  // ----------------------------------------------------------
  // RADIO (PRIMERO, antes que la pantalla)
  // ----------------------------------------------------------

  configurarRadio();

  // ----------------------------------------------------------
  // PANTALLA REDONDA (DESPUÉS del radio)
  // ----------------------------------------------------------

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  mostrarEnPantalla(
    "Iniciando...",
    "Cargando",
    "modulos",
    "Espere"
  );

  // ----------------------------------------------------------
  // LORAWAN
  // ----------------------------------------------------------

  configurarLoRaWAN();

  // ----------------------------------------------------------
  // MICROFONO
  // ----------------------------------------------------------

  configurarMicrofono();

  Serial.println();
  Serial.println("=================================");
  Serial.println("Wio-SX1262 Audio LoRaWAN");
  Serial.println("AU915 / FSB2");
  Serial.println("RadioLib + persistencia LoRaWAN");
  Serial.println("=================================");

  Serial.println();
  Serial.println("Sistema iniciado");
  Serial.println();

  mostrarEnPantalla(
    "Sistema",
    "Iniciado",
    "Esperando",
    "datos..."
  );
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  if (
    millis() - ultimaTransmision <
    INTERVALO_TX
  ) {

    return;
  }

  ultimaTransmision =
      millis();

  uint32_t suma;
  uint32_t muestras;
  uint16_t pico;

  // ----------------------------------------------------------
  // COPIAR DATOS PDM
  // ----------------------------------------------------------

  noInterrupts();

  suma = sumaAbsoluta;

  muestras =
      cantidadMuestras;

  pico =
      valorPico;

  sumaAbsoluta = 0;

  cantidadMuestras = 0;

  valorPico = 0;

  interrupts();

  if (muestras == 0) {

    Serial.println(
      "No se recibieron muestras del microfono"
    );

    return;
  }

  // ----------------------------------------------------------
  // CALCULAR PROMEDIO
  // ----------------------------------------------------------

  uint32_t promedio =
      suma / muestras;

  // ----------------------------------------------------------
  // PAYLOAD
  //
  // 4 bytes AVG
  // 2 bytes PEAK
  // ----------------------------------------------------------

  uint8_t payload[6];

  payload[0] =
      (promedio >> 24) & 0xFF;

  payload[1] =
      (promedio >> 16) & 0xFF;

  payload[2] =
      (promedio >> 8) & 0xFF;

  payload[3] =
      promedio & 0xFF;

  payload[4] =
      (pico >> 8) & 0xFF;

  payload[5] =
      pico & 0xFF;

  // ----------------------------------------------------------
  // MOSTRAR DATOS
  // ----------------------------------------------------------

  Serial.println();

  Serial.println(
    "-------------------------"
  );

  Serial.print("AVG = ");

  Serial.println(promedio);

  Serial.print("PEAK = ");

  Serial.println(pico);

  mostrarPayload(
    payload,
    sizeof(payload)
  );

  // ----------------------------------------------------------
  // MOSTRAR EN PANTALLA
  // ----------------------------------------------------------

  char linea1[20];
  char linea2[20];
  char linea3[20];

  snprintf(
    linea1,
    sizeof(linea1),
    "AVG: %lu",
    (unsigned long)promedio
  );

  snprintf(
    linea2,
    sizeof(linea2),
    "PEAK: %u",
    pico
  );

  snprintf(
    linea3,
    sizeof(linea3),
    "Enviando..."
  );

  mostrarEnPantalla(
    "Datos listos",
    linea1,
    linea2,
    linea3
  );

  // ----------------------------------------------------------
  // TRANSMITIR
  // ----------------------------------------------------------

  int16_t estado =
      node.sendReceive(
        payload,
        sizeof(payload),
        10
      );

  // ----------------------------------------------------------
  // GUARDAR SESION DESPUES DEL UPLINK
  // ----------------------------------------------------------

  guardarSesion();

  guardarNonces();

  // ----------------------------------------------------------
  // RESULTADO TX
  // ----------------------------------------------------------

  if (
    estado < RADIOLIB_ERR_NONE
  ) {

    Serial.print(
      "Error LoRaWAN TX: "
    );

    Serial.println(estado);

    mostrarEnPantalla(
      "Error TX",
      linea1,
      linea2,
      "Sin enlace"
    );

  }

  else if (estado > 0) {

    Serial.print(
      "Uplink OK + downlink recibido en ventana RX: "
    );

    Serial.println(estado);

    mostrarEnPantalla(
      "TX OK",
      linea1,
      linea2,
      "Con downlink"
    );

  }

  else {

    Serial.println(
      "LoRaWAN uplink OK - sin downlink"
    );

    mostrarEnPantalla(
      "TX OK",
      linea1,
      linea2,
      "Enviado a TTN"
    );
  }
}