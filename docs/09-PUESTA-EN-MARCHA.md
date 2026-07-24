# 9. Puesta en marcha

Esta sección es la práctica: qué necesitas, cómo compilarlo y cómo conducir.

## Lo que necesitas

### La emisora (este repositorio)

| Pieza | Ejemplo / nota |
|-------|----------------|
| Placa ESP32-**S3** | Cualquier ESP32-S3-DevKitC o similar. **Tiene que ser un S3**: es el que puede hacer de USB Host. |
| Mando de Xbox | Xbox One o Series, con **cable USB**. |
| Cable USB | Para enchufar el mando al S3 (OTG si tu placa lo pide). |
| LED RGB | El integrado de la placa (pin 48 en la mayoría de DevKitC). |

### El coche (receptor, repositorio aparte)

| Pieza | Ejemplo / nota |
|-------|----------------|
| Placa ESP32-**C3** | Pequeña y barata, va dentro del coche. |
| Variador (ESC) | HobbyWing QuicRun 1060, o cualquier ESC estándar de 50 Hz. |
| Servo de dirección | MG996R, o cualquier servo estándar. |
| Coche RC | El chasis que ya tengas. |

> **Aviso importante de alimentación:** el servo se alimenta desde la salida del
> variador (el BEC del ESC), **nunca** desde el pin de 5 V del ESP32. Un servo como
> el MG996R pide mucha corriente en los picos y te reiniciaría la placa.

## Compilar y flashear la emisora

Este proyecto está hecho con **ESP-IDF** (el framework oficial de Espressif), no
con Arduino. Si vienes de Arduino no te asustes: se instala una vez y luego son
tres comandos.

### 1. Instala ESP-IDF

Sigue la [guía oficial de Espressif](https://docs.espressif.com/projects/esp-idf/es/latest/esp32s3/get-started/index.html)
(versión 5.0 o superior). Al terminar tendrás el comando `idf.py` disponible.

### 2. Clona el repositorio

```bash
git clone https://github.com/Placix5/ESP32Transmitter.git
cd ESP32Transmitter
```

No hace falta `--recursive` ni nada raro: las librerías (TinyUSB y el driver
XInput) vienen incluidas dentro de `components/`, ya listas para compilar.

### 3. Compila y flashea

Con el ESP32-S3 conectado por USB:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PUERTO flash monitor
```

Sustituye `PUERTO` por tu puerto serie (`COM4` en Windows, `/dev/ttyUSB0` o
`/dev/ttyACM0` en Linux). El `monitor` del final te deja ver los mensajes de la
placa; sal con `Ctrl+]`.

## Cómo se conduce

1. Enciende el coche (con el receptor C3 y el variador ya montados).
2. Enciende la emisora (el ESP32-S3).
3. Enchufa el mando de Xbox al S3 por USB. El LED se pondrá **verde**: modo Eco,
   listo para conducir.
4. Conduce:
   - **Stick izquierdo (horizontal):** dirección.
   - **Gatillo derecho (RT):** acelerar.
   - **Gatillo izquierdo (LT):** frenar / marcha atrás.
5. Para cambiar de modo, mantén **L1 + R1** dos segundos. El mando vibrará y el
   LED cambiará de color:
   - 🟢 Verde = **Eco** (suave, para empezar)
   - 🔵 Azul = **Normal**
   - 🔴 Rojo = **Sport** (sin límites)

> Consejo: haz las primeras pruebas con el coche **levantado, sin que las ruedas
> toquen el suelo**. Te ahorras más de un susto.

---

« [Anterior: El receptor](08-EL-RECEPTOR.md) · [📚 Índice](README.md) · [Siguiente: Adaptarlo a tu coche »](10-ADAPTARLO-A-TU-COCHE.md)
