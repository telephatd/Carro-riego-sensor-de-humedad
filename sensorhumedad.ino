
/*
  ESP32 DE LA PLANTA
*/

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

const int PIN_HUMEDAD = 34;


const bool SECO_ES_MAYOR = true;
const int UMBRAL_SECO = 2800;
const int UMBRAL_HUMEDO = 2200;

const uint8_t ID_MENSAJE = 0xA7;

uint8_t direccionBroadcast[] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

typedef struct __attribute__((packed)) {
  uint8_t id;
  uint8_t necesitaAgua;
  uint16_t humedad;
} MensajeHumedad;

bool necesitaAgua = false;

int leerHumedad() {
  long suma = 0;

  for (int i = 0; i < 10; i++) {
    suma += analogRead(PIN_HUMEDAD);
    delay(5);
  }

  return suma / 10;
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_HUMEDAD, INPUT);
  analogReadResolution(12);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error iniciando ESP-NOW");
    while (true) delay(1000);
  }

  
  esp_now_peer_info_t peerInfo = {};

  memcpy(peerInfo.peer_addr, direccionBroadcast, 6);

  peerInfo.channel = 1;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Error agregando destinatario");
    while (true) delay(1000);
  }

  Serial.println("Estacion de humedad iniciada");
}

void loop() {

  int humedad = leerHumedad();

 

  if (SECO_ES_MAYOR) {

    if (humedad >= UMBRAL_SECO) {
      necesitaAgua = true;
    }
    else if (humedad <= UMBRAL_HUMEDO) {
      necesitaAgua = false;
    }

  } else {

    if (humedad <= UMBRAL_SECO) {
      necesitaAgua = true;
    }
    else if (humedad >= UMBRAL_HUMEDO) {
      necesitaAgua = false;
    }
  }

  static unsigned long ultimoEnvio = 0;

  if (millis() - ultimoEnvio >= 1000) {

    MensajeHumedad mensaje;

    mensaje.id = ID_MENSAJE;
    mensaje.necesitaAgua = necesitaAgua ? 1 : 0;
    mensaje.humedad = humedad;

    esp_err_t resultado = esp_now_send(
      direccionBroadcast,
      (uint8_t *)&mensaje,
      sizeof(mensaje)
    );

    Serial.printf(
      "Humedad ADC: %d | Necesita agua: %s\n",
      humedad,
      necesitaAgua ? "SI" : "NO"
    );

    if (resultado != ESP_OK) {
      Serial.println("Error enviando mensaje");
    }

    ultimoEnvio = millis();
  }

  delay(50);
}
