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

## Unicast: hablarle solo al coche

En una primera versión la emisora gritaba los paquetes por **broadcast** (a la
dirección `FF:FF:FF:FF:FF:FF`, que significa "para todo el que escuche"). Era lo
más simple para empezar, pero ahora la emisora habla **directamente a la MAC del
coche** (*unicast*): el paquete va dirigido a un único destinatario.

En el código, esa dirección es `client_mac`. Un detalle que despista a todo el
mundo la primera vez: **ESP-NOW viaja sobre la interfaz Wi-Fi**, así que hay que
poner la MAC de la **Wi-Fi en modo STA** del receptor, **no** la de Bluetooth ni la
de Ethernet. Un mismo ESP32 tiene varias MAC derivadas de una base (Wi-Fi STA, Wi-Fi
AP, Bluetooth, Ethernet), y si te equivocas de una, los paquetes salen "bien" pero
no llegan a nadie.

## Long Range y radio siempre despierta

Dos ajustes hacen el enlace mucho más resistente, y ambos se configuran al arrancar:

- **Long Range (`WIFI_PROTOCOL_LR`).** Es una modulación propia de Espressif, más
  lenta en bits por segundo pero muchísimo más robusta: más alcance y más aguante
  frente a interferencias y saturación del canal. **Cuidado:** tiene que estar
  activada **en los dos extremos**. Si la emisora va en Long Range y el receptor no
  (o al revés), dejan de comunicarse por completo.
- **Sin ahorro de energía (`WIFI_PS_NONE`).** Por defecto el Wi-Fi "se echa
  microsiestas" para gastar menos. Aquí no lo queremos: el enlace de control
  necesita el radio siempre despierto para responder al instante, así que
  desactivamos el *power save*.

> **Ojo con el canal Wi-Fi.** ESP-NOW solo funciona si emisora y receptor están en
> el **mismo canal** de radio. Si algún día no se hablan aunque el código esté
> bien, casi siempre es esto (o haberte equivocado de MAC, o tener el Long Range
> solo en un lado). Fijar el canal a mano en las dos placas es una de las mejoras
> pendientes.

## Un flujo constante de 50 Hz (y por qué importa)

La emisora **no** envía "cuando pasa algo". Una tarea dedicada dispara el último
estado conocido al coche **50 veces por segundo, a ritmo fijo**, se mueva o no el
mando. Esto es a propósito y resuelve un problema real:

Un mando de Xbox solo reporta **cuando algo cambia**. Si dejas los sticks quietos,
puede pasar un buen rato sin decir nada. Si enviásemos "por cada reporte del mando",
en esos silencios no saldría ningún paquete... y el coche no sabría distinguir entre
"todo sigue igual" y "he perdido la señal". Con un flujo constante, en cambio, el
receptor puede medir de verdad cuánto hace que no recibe nada y actuar en
consecuencia. Además es más regular (y normalmente menos tráfico) que disparar por
cada mínimo movimiento.

## Failsafe: si se va el mando, el coche para

Ese flujo constante habilita una red de seguridad en la propia emisora. Si
**desenchufas el mando** (o se queda sin cable), la emisora fuerza el estado a
**NEUTRO** —motor parado y ruedas rectas— y la tarea de envío lo sigue mandando a
50 Hz. Resultado: el coche se detiene **al instante**, sin esperar a que salte
ningún temporizador en el receptor.

> El detalle de cómo conviven la tarea de envío a 50 Hz y la lectura del mando (dos
> núcleos, un estado compartido y un candado) está en
> [FreeRTOS y núcleos](07-FREERTOS-Y-NUCLEOS.md).

## El paquete de 3 bytes

Cuando mandas datos por radio muchas veces por segundo, cada byte cuenta. Cuanto
más pequeño el mensaje, menos tarda en salir y menos probable es que se pierda o
choque con otro. Por eso la orden que viaja al coche es minúscula: **3 bytes**.

En el código ([`main/main.c`](../main/main.c)) es esta estructura:

```c
typedef struct __attribute__((packed)) MensajeRadio {
  uint8_t modo_conduccion;  // 0 = Eco, 1 = Normal, 2 = Sport
  uint8_t pwm_motor;        // valor para el variador (0-180, siendo 90 el neutro)
  uint8_t angulo_servo;     // ángulo de la dirección, en grados (topes ajustables)
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
