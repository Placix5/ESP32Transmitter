# 4. Comunicación ESP-NOW

Una vez el S3 sabe lo que quieres hacer, hay que mandárselo al coche. Aquí es
donde ESP-NOW se gana el sueldo.

## Qué es ESP-NOW

**ESP-NOW** es un protocolo de Espressif que usa la misma antena Wi-Fi del ESP32,
pero **sin Wi-Fi de por medio**. No hay router, no hay contraseña, no hay conexión
que establecer. Es como gritar un mensaje corto directamente al otro chip por su
nombre (su dirección MAC).

Comparado con el Bluetooth de la primera versión, gana en tres cosas que aquí
importan mucho:

- **Alcance.** ESP-NOW llega muchísimo más lejos, sobre todo al aire libre.
  Hablamos de decenas o cientos de metros con línea de visión, frente a los
  5 metros escasos del Bluetooth.
- **Latencia.** Como no hay que "abrir conexión" ni negociar nada, el mensaje sale
  casi al instante. Para conducir, eso se nota.
- **Sencillez.** Mandar un paquete es prácticamente una sola línea de código.

En este proyecto, la emisora manda los paquetes por **broadcast** (a la dirección
`FF:FF:FF:FF:FF:FF`, que significa "para todo el que escuche"). Es lo más simple
para empezar: no hay que emparejar nada, enciendes las dos placas y ya se hablan.
La contrapartida es que no hay ni seguridad ni emparejamiento; cualquiera con un
receptor en el mismo canal podría escuchar. Está en la
[hoja de ruta](11-HOJA-DE-RUTA.md).

> **Ojo con el canal Wi-Fi.** ESP-NOW solo funciona si emisora y receptor están en
> el **mismo canal** de radio. Si algún día no se hablan aunque el código esté
> bien, casi siempre es esto. Fijar el canal a mano en las dos placas es una de las
> mejoras pendientes.

## El paquete de 3 bytes

Cuando mandas datos por radio muchas veces por segundo, cada byte cuenta. Cuanto
más pequeño el mensaje, menos tarda en salir y menos probable es que se pierda o
choque con otro. Por eso la orden que viaja al coche es minúscula: **3 bytes**.

En el código ([`main/main.c`](../main/main.c)) es esta estructura:

```c
typedef struct __attribute__((packed)) MensajeRadio {
  uint8_t modo_conduccion;  // 0 = Eco, 1 = Normal, 2 = Sport
  uint8_t pwm_motor;        // valor para el variador (0-180, siendo 90 el neutro)
  uint8_t angulo_servo;     // ángulo de la dirección (65-115 grados)
} MensajeRadio;
```

Tres números, un byte cada uno. Eso es todo lo que necesita saber el coche para
moverse.

El `__attribute__((packed))` que ves ahí es importante: le dice al compilador que
**no deje huecos** entre los campos. Los procesadores, por su cuenta, a veces dejan
espacios en blanco entre datos para leer más rápido. Aquí no lo queremos: el
receptor tiene que recibir exactamente los mismos 3 bytes, en el mismo orden y sin
huecos, para que los números coincidan a los dos lados de la radio.

---

« [Anterior: Leer el mando](03-LEER-EL-MANDO.md) · [📚 Índice](README.md) · [Siguiente: Lógica de conducción »](05-LOGICA-DE-CONDUCCION.md)
