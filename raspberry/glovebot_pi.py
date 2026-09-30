import json
import math
import time
import paho.mqtt.client as mqtt

TOKEN = "SEU_TOKEN_AQUI"
DEVICE = "glovebot"

MODO_TESTE = True            # True = ângulo simulado | False = lê o mbed pela USB
PORTA_SERIAL = "/dev/ttyACM0"
BAUD = 9600

INTERVALO_ENVIO = 2.0        # segundos entre envios ao Ubidots
LIMITE_FECHADA = 60          # abaixo disso = garra fechada
LIMITE_ABERTA = 120          # acima disso = garra aberta
TIMEOUT_CONEXAO = 2.0        # sem dados por mais que isso = desconectado


def ler_angulo_serial(ser):
    linha = ser.readline().decode(errors="ignore").strip()
    if linha.isdigit():
        valor = int(linha)
        if 0 <= valor <= 180:
            return valor
    return None


def main():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(TOKEN, "")
    client.connect("industrial.api.ubidots.com", 1883, 60)
    client.loop_start()

    ser = None
    if not MODO_TESTE:
        import serial
        ser = serial.Serial(PORTA_SERIAL, BAUD, timeout=0.1)

    inicio = time.time()
    angulo = 0
    ultimo_angulo = None
    ultimo_dado = 0.0
    soma_variacao = 0.0
    ultimo_extremo = None    # "aberta" ou "fechada"
    agarramentos = 0
    ultimo_envio = time.time()

    print("Modo:", "TESTE (simulado)" if MODO_TESTE else "REAL (serial)")

    try:
        while True:
            agora = time.time()
            novo = None

            if MODO_TESTE:
                t = agora - inicio
                # nos últimos 6 s de cada 40 s, simula queda de conexão
                if (t % 40) < 34:
                    novo = int(90 + 90 * math.sin(t / 1.5))
                time.sleep(0.05)
            else:
                novo = ler_angulo_serial(ser)

            if novo is not None:
                angulo = novo
                ultimo_dado = agora
                if ultimo_angulo is not None:
                    soma_variacao += abs(novo - ultimo_angulo)
                ultimo_angulo = novo

                # conta 1 agarramento quando a garra vai de aberta para fechada
                if novo > LIMITE_ABERTA:
                    ultimo_extremo = "aberta"
                elif novo < LIMITE_FECHADA:
                    if ultimo_extremo == "aberta":
                        agarramentos += 1
                    ultimo_extremo = "fechada"

            if agora - ultimo_envio >= INTERVALO_ENVIO:
                dt = agora - ultimo_envio
                conectado = 1 if (agora - ultimo_dado) < TIMEOUT_CONEXAO else 0

                if angulo < LIMITE_FECHADA:
                    status = 0      # fechada
                elif angulo > LIMITE_ABERTA:
                    status = 2      # aberta
                else:
                    status = 1      # meio

                dados = {
                    "angulo": angulo,
                    "status_garra": status,
                    "contador_agarramentos": agarramentos,
                    "status_conexao": conectado,
                    "velocidade_movimento": round(soma_variacao / dt, 1),
                }
                client.publish("/v1.6/devices/" + DEVICE, json.dumps(dados))
                print("Enviado:", dados)

                soma_variacao = 0.0
                ultimo_envio = agora

    except KeyboardInterrupt:
        print("Encerrando...")
    finally:
        client.loop_stop()
        client.disconnect()


main()
