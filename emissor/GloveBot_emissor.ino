/*
 * GloveBot - Modulo Emissor (Luva)
 *
 * Hardware: LilyPad Arduino USB (ATmega32U4), MPU-6050 (I2C, pinos 2/3),
 *           HC-08 Master (SoftwareSerial, pinos 9/10)
 *
 * Le o giro lateral da mao (roll) no MPU-6050, filtra, converte para o
 * angulo da garra (0 = fechada, 180 = aberta) e envia por Bluetooth.
 * Protocolo enviado: "A<angulo>\n" a cada 50 ms.
 */
#include <Wire.h>
#include <MPU6050.h>
#include <SoftwareSerial.h>

#define BT_RX  9    // recebe do TXD do HC-08
#define BT_TX  10   // envia ao RXD do HC-08

#define ANGLE_MIN   0
#define ANGLE_MAX   180
#define SEND_INTERVAL 50

// === SENSIBILIDADE ===
// Faixa de giro da mao (graus de roll) que corresponde ao curso completo.
// Mais largo = mais facil controlar posicoes intermediarias.
#define ROLL_FECHADO  -5    // mao nessa posicao = garra fechada
#define ROLL_ABERTO   -50   // mao nessa posicao = garra aberta

// === TREMOR ===
#define ALPHA     0.30f     // filtro suave (menor = mais suave e mais lento)
#define DEADBAND  2         // variacao minima (graus) para mudar o angulo

MPU6050 mpu;
SoftwareSerial btSerial(BT_RX, BT_TX);

float rollFiltrado = 0;
bool primeiraLeitura = true;
int anguloEnviado = 0;

void setup() {
  Wire.begin();
  mpu.initialize();
  btSerial.begin(9600);
  Serial.begin(9600);

  if (!mpu.testConnection()) {
    Serial.println("ERRO: MPU-6050 nao encontrado!");
    while (true);
  }
  Serial.println("MPU-6050 OK. GloveBot emissor v3 iniciado.");
}

float calcRoll(int16_t ax, int16_t ay, int16_t az) {
  return atan2((float)ay, (float)az) * (180.0 / M_PI);
}

float mapFloat(float x, float inMin, float inMax, float outMin, float outMax) {
  return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

void loop() {
  static unsigned long lastSend = 0;

  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();

    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);

    float roll = calcRoll(ax, ay, az);

    // Filtro passa-baixa (media movel exponencial) contra tremor
    if (primeiraLeitura) {
      rollFiltrado = roll;
      primeiraLeitura = false;
    } else {
      rollFiltrado = ALPHA * roll + (1.0f - ALPHA) * rollFiltrado;
    }

    float alvo = mapFloat(rollFiltrado, ROLL_ABERTO, ROLL_FECHADO, ANGLE_MAX, ANGLE_MIN);
    int angle = constrain((int)alvo, ANGLE_MIN, ANGLE_MAX);

    // Zona morta: ignora variacoes pequenas; nos extremos sempre atualiza
    if (abs(angle - anguloEnviado) >= DEADBAND ||
        angle == ANGLE_MIN || angle == ANGLE_MAX) {
      anguloEnviado = angle;
    }

    btSerial.print("A");
    btSerial.print(anguloEnviado);
    btSerial.print("\n");

    Serial.print("Roll: "); Serial.print(rollFiltrado, 1);
    Serial.print("  Angle: "); Serial.println(anguloEnviado);
  }
}
