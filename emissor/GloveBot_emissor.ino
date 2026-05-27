/*
 * GloveBot - Módulo Emissor (Luva)
 * 
 * Hardware:
 * - LilyPad Arduino USB (ATmega32U4)
 * - MPU-6050 (acelerômetro/giroscópio I2C)
 * - HC-08 (Bluetooth 4.0 BLE)
 * - Bateria Li-ion wearable
 * 
 * Funcionamento:
 * Lê a inclinação do pulso pelo MPU-6050, calcula um ângulo de 0 a 180°
 * e envia o comando via Bluetooth para o receptor controlar a garra.
 * 
 * Protocolo de envio: "A<angulo>\n"
 * Exemplo: "A90\n" -> garra em 90° (semi-aberta)
 */

#include <Wire.h>
#include <MPU6050.h>
#include <SoftwareSerial.h>

// Pinos do HC-08 na LilyPad
#define BT_RX  9   // recebe do TX do HC-08
#define BT_TX  10  // envia ao RX do HC-08

// Limites do ângulo de abertura da garra
#define ANGLE_MIN   0    // garra fechada
#define ANGLE_MAX   180  // garra aberta

// Intervalo de envio (ms)
#define SEND_INTERVAL 50

MPU6050 mpu;
SoftwareSerial btSerial(BT_RX, BT_TX);

void setup() {
  Wire.begin();
  mpu.initialize();
  btSerial.begin(9600);
  Serial.begin(9600);

  if (!mpu.testConnection()) {
    Serial.println("ERRO: MPU-6050 nao encontrado!");
    while (true);
  }
  Serial.println("MPU-6050 OK. GloveBot emissor iniciado.");
}

// Calcula o pitch (inclinação frontal) a partir da aceleração
float calcPitch(int16_t ax, int16_t ay, int16_t az) {
  return atan2((float)ax, (float)az) * (180.0 / M_PI);
}

void loop() {
  static unsigned long lastSend = 0;

  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();

    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);

    float pitch = calcPitch(ax, ay, az);

    // Mapeia -90° a +90° para 0° a 180° (servo)
    int angle = map((int)pitch, -90, 90, ANGLE_MIN, ANGLE_MAX);
    angle = constrain(angle, ANGLE_MIN, ANGLE_MAX);

    // Envia comando via Bluetooth
    btSerial.print("A");
    btSerial.print(angle);
    btSerial.print("\n");

    // Debug no monitor serial
    Serial.print("Pitch: "); Serial.print(pitch, 1);
    Serial.print("  Angle: "); Serial.println(angle);
  }
}