/*
 * GloveBot - Módulo Receptor (Garra)
 * 
 * Hardware:
 * - mbed LPC1768 (ARM Cortex-M3)
 * - HC-08 (Bluetooth 4.0 BLE - Slave)
 * - Servo Motor MG995
 * - Display OLED 128x32 I2C (SSD1306)
 * - Fonte 5V-2A externa
 * 
 * Funcionamento:
 * Recebe comandos via Bluetooth do módulo emissor (luva),
 * processa o ângulo recebido e controla o servo motor da garra.
 * Exibe status em tempo real no display OLED.
 * 
 * Protocolo recebido: "A<angulo>\n"
 */

#include "mbed.h"

// ============ HARDWARE ============
PwmOut servo(p21);
BufferedSerial bt(p13, p14, 9600);
DigitalOut led(LED1);
I2C oled(p9, p10);

// ============ CONSTANTES ============
const float SERVO_PERIOD_MS = 20.0f;
const float SERVO_MIN_MS = 1.0f;
const float SERVO_MAX_MS = 2.0f;
const int OLED_ADDR = 0x78;  // 0x3C << 1

// ============ FONTE 5x7 (numeros) ============
const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // espaço
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
};

// ============ LETRAS A-Z ============
const uint8_t letters[][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x7F,0x20,0x18,0x20,0x7F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x03,0x04,0x78,0x04,0x03}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z
};

// ============ FUNCOES OLED ============
void oledCmd(uint8_t cmd) {
    char data[2] = {0x00, (char)cmd};
    oled.write(OLED_ADDR, data, 2);
}

void oledData(uint8_t* buf, int len) {
    char data[129];
    data[0] = 0x40;
    for (int i = 0; i < len; i++) data[i+1] = buf[i];
    oled.write(OLED_ADDR, data, len+1);
}

void oledInit() {
    ThisThread::sleep_for(100ms);
    oledCmd(0xAE);
    oledCmd(0xD5); oledCmd(0x80);
    oledCmd(0xA8); oledCmd(0x1F);
    oledCmd(0xD3); oledCmd(0x00);
    oledCmd(0x40);
    oledCmd(0x8D); oledCmd(0x14);
    oledCmd(0x20); oledCmd(0x00);
    oledCmd(0xA1);
    oledCmd(0xC8);
    oledCmd(0xDA); oledCmd(0x02);
    oledCmd(0x81); oledCmd(0x8F);
    oledCmd(0xD9); oledCmd(0xF1);
    oledCmd(0xDB); oledCmd(0x40);
    oledCmd(0xA4);
    oledCmd(0xA6);
    oledCmd(0xAF);
}

void oledClear() {
    for (int page = 0; page < 4; page++) {
        oledCmd(0xB0 | page);
        oledCmd(0x00);
        oledCmd(0x10);
        uint8_t zeros[128] = {0};
        oledData(zeros, 128);
    }
}

void oledChar(int page, int col, char c) {
    oledCmd(0xB0 | page);
    oledCmd(0x00 | (col & 0x0F));
    oledCmd(0x10 | ((col >> 4) & 0x0F));
    
    uint8_t buf[6] = {0};
    if (c >= '0' && c <= '9') {
        for (int i = 0; i < 5; i++) buf[i] = font5x7[c - '0' + 1][i];
    } else if (c >= 'A' && c <= 'Z') {
        for (int i = 0; i < 5; i++) buf[i] = letters[c - 'A'][i];
    } else if (c == ':') {
        buf[0] = 0x00; buf[1] = 0x36; buf[2] = 0x36; buf[3] = 0x00; buf[4] = 0x00;
    }
    oledData(buf, 6);
}

void oledText(int page, int col, const char* text) {
    int x = col;
    while (*text) {
        oledChar(page, x, *text);
        x += 6;
        text++;
    }
}

// ============ FUNCOES SERVO ============
void setServoAngle(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    float pulse = SERVO_MIN_MS + (angle / 180.0f) * (SERVO_MAX_MS - SERVO_MIN_MS);
    servo.pulsewidth_ms(pulse);
}

void updateDisplay(int angle) {
    char buf[20];
    oledClear();
    oledText(0, 30, "GLOVEBOT");
    
    sprintf(buf, "ANG:%d", angle);
    oledText(2, 0, buf);
    
    const char* status;
    if (angle < 60) status = "FECHADO";
    else if (angle < 120) status = "MEIO";
    else status = "ABERTO";
    oledText(2, 60, status);
}

// ============ MAIN ============
int main() {
    servo.period_ms(SERVO_PERIOD_MS);
    setServoAngle(0);
    
    oledInit();
    oledClear();
    oledText(0, 20, "GLOVEBOT");
    oledText(2, 10, "INICIANDO");
    ThisThread::sleep_for(1500ms);
    updateDisplay(0);
    
    char buf[8];
    int bufIdx = 0;
    bool reading = false;
    char c;
    int curAngle = 0;
    
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
                    setServoAngle(angle);
                    if (angle != curAngle) {
                        curAngle = angle;
                        updateDisplay(curAngle);
                    }
                }
                reading = false;
                bufIdx = 0;
            } else if (reading && bufIdx < 7) {
                buf[bufIdx++] = c;
            }
        }
    }
}