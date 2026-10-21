/*
 * GloveBot - Configuracao do HC-08 da LUVA (LilyPad USB)
 * Ponte entre o Monitor Serial e o HC-08, para digitar comandos AT.
 *
 * HC-08 TX -> pino 9 | HC-08 RX -> pino 10 (igual ao codigo da luva)
 * Monitor Serial: 9600 baud, "Ambos NL e CR"
 * ATENCAO: este sketch substitui o codigo da luva. Depois e preciso
 * gravar de novo o codigo do emissor.
 */
#include <SoftwareSerial.h>

SoftwareSerial bt(9, 10);   // RX, TX

void setup() {
  Serial.begin(9600);
  bt.begin(9600);
  while (!Serial) { }
  Serial.println(F("Pronto. Digite AT e envie."));
}

void loop() {
  while (bt.available())     Serial.write(bt.read());
  while (Serial.available()) bt.write(Serial.read());
}
