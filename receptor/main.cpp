/*
 * GloveBot - Modulo Receptor (Garra)
 *
 * Hardware:
 * - mbed LPC1768 (ARM Cortex-M3)
 * - HC-08 (Bluetooth 4.0 BLE - Slave) em p13/p14
 * - Servo Motor MG995 em p21 (alimentado pela fonte 5V-2A, GND comum)
 * - Raspberry Pi (recebe o angulo pela Serial USB)
 *
 * Funcionamento:
 * Recebe o angulo da luva via Bluetooth, move o servo da garra de forma
 * suave (curso limitado e velocidade maxima) e repassa o angulo pela
 * Serial USB para o Raspberry Pi, que publica os dados no Ubidots.
 *
 * Protocolo recebido (Bluetooth): "A<angulo>\n"  (0 = fechada, 180 = aberta)
 * Protocolo enviado (USB):        "<angulo>\n"   (9600 baud)
 */

#include "mbed.h"

// ============ HARDWARE ============
PwmOut servo(p21);
BufferedSerial bt(p13, p14, 9600);
DigitalOut led(LED1);

// ============ SERVO ============
const float SERVO_PERIOD_MS = 20.0f;
const float SERVO_MIN_MS = 1.0f;
const float SERVO_MAX_MS = 2.0f;
// 1 = garra montada ao contrario (servo em 0 graus = garra aberta)
#define SERVO_INVERTIDO 1

// ============ AJUSTES DO MOVIMENTO ============
// Curso permitido do servo (em graus "logicos" 0..180).
// Se a garra ficar forcando FECHADA, aumente LIM_MIN.
// Se ficar forcando ABERTA, diminua LIM_MAX.
const int LIM_MIN = 15;    // posicao mais fechada permitida
const int LIM_MAX = 165;   // posicao mais aberta permitida
// Velocidade maxima: graus por passo de 20 ms (4 = 200 graus/s).
// Menor = mais suave e menos pico de corrente. Maior = mais rapido.
const int PASSO_MAX = 4;
const int PERIODO_MS = 20;

void setServoAngle(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
#if SERVO_INVERTIDO
    angle = 180 - angle;
#endif
    float pulse = SERVO_MIN_MS + (angle / 180.0f) * (SERVO_MAX_MS - SERVO_MIN_MS);
    // pulsewidth_us: no Mbed OS 6, pulsewidth_ms so aceita inteiros
    servo.pulsewidth_us((int)(pulse * 1000.0f));
}

// ============ MAIN ============
int main() {
    servo.period_ms(SERVO_PERIOD_MS);
    int atual = LIM_MIN;   // posicao atual do servo
    int alvo = LIM_MIN;    // posicao pedida pela luva
    setServoAngle(atual);

    char buf[8];
    int bufIdx = 0;
    bool reading = false;
    char c;

    Timer passo;
    passo.start();

    while (true) {
        if (bt.readable()) {
            bt.read(&c, 1);

            if (c == 'A') {
                bufIdx = 0;
                reading = true;
            } else if (reading && c == '\n') {
                buf[bufIdx] = '\0';
                int angle = atoi(buf);

                if (angle >= 0 && angle <= 180) {
                    led = !led;
                    // Converte 0..180 da luva para o curso permitido do servo
                    alvo = LIM_MIN + (angle * (LIM_MAX - LIM_MIN)) / 180;

                    // Envia o angulo da luva pela Serial USB para o Raspberry Pi
                    printf("%d\n", angle);
                }
                reading = false;
                bufIdx = 0;
            } else if (reading && bufIdx < 7) {
                buf[bufIdx++] = c;
            }
        }

        // A cada 20 ms o servo anda no maximo PASSO_MAX graus em direcao ao alvo
        if (passo.elapsed_time() >= std::chrono::milliseconds(PERIODO_MS)) {
            passo.reset();
            if (atual != alvo) {
                int dif = alvo - atual;
                if (dif > PASSO_MAX) dif = PASSO_MAX;
                if (dif < -PASSO_MAX) dif = -PASSO_MAX;
                atual += dif;
                setServoAngle(atual);
            }
        }
    }
}
