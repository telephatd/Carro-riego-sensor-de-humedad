
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>



const uint8_t ID_MENSAJE = 0xA7;



typedef struct __attribute__((packed)) {
  uint8_t id;
  uint8_t necesitaAgua;
  uint16_t humedad;
} MensajeHumedad;

volatile bool mensajeNuevo = false;
volatile uint8_t solicitudRecibida = 0;
volatile uint16_t humedadRecibida = 0;

bool estadoRecibido = false;
bool sueloEstaSeco = false;


portMUX_TYPE muxMensajes = portMUX_INITIALIZER_UNLOCKED;



void recibirDatos(
  const esp_now_recv_info_t *info,
  const uint8_t *datos,
  int longitud
) {

  // Comprobar que el mensaje tenga el tamaño correcto

  if (longitud != sizeof(MensajeHumedad)) {
    return;
  }

  MensajeHumedad mensaje;

  memcpy(&mensaje, datos, sizeof(mensaje));

  if (mensaje.id != ID_MENSAJE) {
    return;
  }

  // Guardar informacion recibida

  portENTER_CRITICAL(&muxMensajes);

  solicitudRecibida = mensaje.necesitaAgua;
  humedadRecibida = mensaje.humedad;
  mensajeNuevo = true;

  portEXIT_CRITICAL(&muxMensajes);
}


const int ENA_IZQ = 25;
const int IN1_IZQ = 26;
const int IN2_IZQ = 27;

const int ENB_IZQ = 14;
const int IN3_IZQ = 16;
const int IN4_IZQ = 17;



const int ENA_DER = 18;
const int IN1_DER = 19;
const int IN2_DER = 21;

const int ENB_DER = 22;
const int IN3_DER = 23;
const int IN4_DER = 13;





const bool INV_DEL_IZQ = false;
const bool INV_TRA_IZQ = false;
const bool INV_DEL_DER = false;
const bool INV_TRA_DER = false;




const int VELOCIDAD_IZQUIERDA = 115;
const int VELOCIDAD_DERECHA = 115;



const unsigned long TIEMPO_IDA_MS = 5000;



const unsigned long TIEMPO_REGRESO_MS = 5000;



const unsigned long TIEMPO_RIEGO_MS = 5000;



const int PIN_BOMBA = 33;


const bool BOMBA_ACTIVA_HIGH = true;

void controlarBomba(bool encender) {

  bool nivel = (encender == BOMBA_ACTIVA_HIGH);

  digitalWrite(PIN_BOMBA, nivel ? HIGH : LOW);
}



void controlarMotor(
  int pwm,
  int in1,
  int in2,
  bool invertir,
  int velocidad
) {

  velocidad = constrain(velocidad, -255, 255);

  if (invertir) {
    velocidad = -velocidad;
  }

  if (velocidad > 0) {

    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);

  }
  else if (velocidad < 0) {

    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);

  }
  else {

    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }

  ledcWrite(pwm, abs(velocidad));
}



void moverTodos(int velocidadIzquierda, int velocidadDerecha) {

  controlarMotor(
    ENA_IZQ, IN1_IZQ, IN2_IZQ,
    INV_DEL_IZQ, velocidadIzquierda
  );

  
  controlarMotor(
    ENB_IZQ, IN3_IZQ, IN4_IZQ,
    INV_TRA_IZQ, velocidadIzquierda
  );


  controlarMotor(
    ENA_DER, IN1_DER, IN2_DER,
    INV_DEL_DER, velocidadDerecha
  );


  controlarMotor(
    ENB_DER, IN3_DER, IN4_DER,
    INV_TRA_DER, velocidadDerecha
  );
}

void avanzar() {

  moverTodos(
    VELOCIDAD_IZQUIERDA,
    VELOCIDAD_DERECHA
  );
}

void retroceder() {

  moverTodos(
    -VELOCIDAD_IZQUIERDA,
    -VELOCIDAD_DERECHA
  );
}

void detener() {

  moverTodos(0, 0);
}



enum Estado {
  ESPERA,
  AVANZANDO,
  REGANDO,
  REGRESANDO,
  ESPERA_HUMEDAD
};

Estado estado = ESPERA;

unsigned long tiempoInicio = 0;


void setup() {

  Serial.begin(115200);

 

  pinMode(IN1_IZQ, OUTPUT);
  pinMode(IN2_IZQ, OUTPUT);
  pinMode(IN3_IZQ, OUTPUT);
  pinMode(IN4_IZQ, OUTPUT);

  pinMode(IN1_DER, OUTPUT);
  pinMode(IN2_DER, OUTPUT);
  pinMode(IN3_DER, OUTPUT);
  pinMode(IN4_DER, OUTPUT);



  pinMode(PIN_BOMBA, OUTPUT);
  controlarBomba(false);

 

  bool pwmOK = true;

  pwmOK &= ledcAttach(ENA_IZQ, 1000, 8);
  pwmOK &= ledcAttach(ENB_IZQ, 1000, 8);
  pwmOK &= ledcAttach(ENA_DER, 1000, 8);
  pwmOK &= ledcAttach(ENB_DER, 1000, 8);

  detener();

  if (!pwmOK) {

    Serial.println("ERROR CONFIGURANDO PWM");

    while (true) {
      detener();
      controlarBomba(false);
      delay(1000);
    }
  }

 
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {

    Serial.println("ERROR ESP-NOW");

    while (true) {
      detener();
      controlarBomba(false);
      delay(1000);
    }
  }

  esp_now_register_recv_cb(recibirDatos);

  Serial.println("============================");
  Serial.println("VEHICULO AGRICOLA INICIADO");
  Serial.println("COMUNICACION ESP-NOW ACTIVA");
  Serial.println("ESPERANDO SOLICITUD DE RIEGO");
  Serial.println("============================");
}



void loop() {

 

  bool hayMensaje = false;
  uint8_t solicitudLocal = 0;
  uint16_t humedadLocal = 0;

  portENTER_CRITICAL(&muxMensajes);

  if (mensajeNuevo) {

    solicitudLocal = solicitudRecibida;
    humedadLocal = humedadRecibida;

    mensajeNuevo = false;

    hayMensaje = true;
  }

  portEXIT_CRITICAL(&muxMensajes);

  if (hayMensaje) {

    sueloEstaSeco = (solicitudLocal == 1);
    estadoRecibido = true;

    Serial.println("----------------------------");

    Serial.print("Humedad ADC recibida: ");
    Serial.println(humedadLocal);

    Serial.print("Suelo necesita agua: ");
    Serial.println(sueloEstaSeco ? "SI" : "NO");

    Serial.println("----------------------------");
  }



  switch (estado) {

    

    case ESPERA:

      detener();
      controlarBomba(false);

      if (estadoRecibido && sueloEstaSeco) {

        Serial.println("SOLICITUD RECIBIDA");
        Serial.println("INICIANDO RECORRIDO");

        avanzar();

        tiempoInicio = millis();

        estado = AVANZANDO;
      }

      break;

    case AVANZANDO:

      avanzar();

      if (millis() - tiempoInicio >= TIEMPO_IDA_MS) {

        detener();

        Serial.println("DESTINO ALCANZADO");
        Serial.println("INICIANDO RIEGO");

        controlarBomba(true);

        tiempoInicio = millis();

        estado = REGANDO;
      }

      break;

    

    case REGANDO:

      detener();

      if (millis() - tiempoInicio >= TIEMPO_RIEGO_MS) {

        controlarBomba(false);

        Serial.println("RIEGO FINALIZADO");
        Serial.println("INICIANDO REGRESO");

        retroceder();

        tiempoInicio = millis();

        estado = REGRESANDO;
      }

      break;

   
    case REGRESANDO:

      retroceder();

      if (millis() - tiempoInicio >= TIEMPO_REGRESO_MS) {

        detener();

        Serial.println("VEHICULO EN ESTACION");
        Serial.println("ESPERANDO QUE EL SUELO SE HUMEDAZCA");

        estado = ESPERA_HUMEDAD;
      }

      break;

   

    case ESPERA_HUMEDAD:

      detener();
      controlarBomba(false);

      if (estadoRecibido && !sueloEstaSeco) {

        Serial.println("SUELO HUMEDO");
        Serial.println("SISTEMA LISTO PARA OTRA MISION");

        estado = ESPERA;
      }

      break;
  }

  delay(5);
}



