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
**NEUTRO** (motor parado y ruedas rectas) y la tarea de envío lo sigue mandando a
50 Hz. Resultado: el coche se detiene **al instante**, sin esperar a que salte
ningún temporizador en el receptor.

> El detalle de cómo conviven la tarea de envío a 50 Hz y la lectura del mando (dos
> núcleos, un estado compartido y un candado) está en
> [FreeRTOS y núcleos](07-FREERTOS-Y-NUCLEOS.md).

## El paquete de ida: 7 bytes

Cuando mandas datos por radio muchas veces por segundo, cada byte cuenta. Cuanto
más pequeño el mensaje, menos tarda en salir y menos probable es que se pierda o
choque con otro. Por eso la orden que viaja al coche sigue siendo minúscula:
**7 bytes**.

En el código ([`main/main.c`](../main/main.c)) es esta estructura:

```c
typedef struct __attribute__((packed)) MensajeRadio {
  uint8_t  modo_conduccion; // 0 = Eco, 1 = Normal, 2 = Sport
  uint8_t  pwm_motor;       // valor para el variador (0-180, siendo 90 el neutro)
  uint8_t  angulo_servo;    // ángulo de la dirección, en grados (topes ajustables)
  uint32_t t_ms;            // marca de tiempo del emisor (para medir la latencia)
} MensajeRadio;
```

Los tres primeros números son todo lo que necesita el coche para moverse: un byte
cada uno. Los otros cuatro bytes son el **sello de tiempo**, y ahora vemos para
qué sirven.

El `__attribute__((packed))` que ves ahí es importante: le dice al compilador que
**no deje huecos** entre los campos. Los procesadores, por su cuenta, a veces dejan
espacios en blanco entre datos para leer más rápido (aquí, típicamente, meterían un
hueco antes del `uint32_t` para alinearlo). No lo queremos: el receptor tiene que
recibir exactamente los mismos 7 bytes, en el mismo orden y sin huecos, para que
los números coincidan a los dos lados de la radio.

> ### ⚠️ Si cambias la estructura, flashea los DOS extremos
>
> Esto es el error más frustrante que te puedes encontrar, así que queda avisado.
> El receptor **comprueba el tamaño** del paquete que le llega y descarta lo que
> no mida lo que espera. Es lo correcto (así no interpreta basura), pero significa
> que emisora y receptor tienen que compartir **exactamente la misma
> estructura**.
>
> Este paquete pasó de 3 a 7 bytes al añadir `t_ms`. Si flasheas solo la emisora,
> el coche recibe paquetes de 7 bytes, espera 3, los tira todos... y **no se
> mueve**, sin ningún mensaje de error. Parece una avería de radio y no lo es.
>
> Regla: cada vez que toques `MensajeRadio` o `Telemetria`, **flashea las dos
> placas**.

## El canal de vuelta: telemetría

Durante mucho tiempo la radio iba en un solo sentido, y eso dejaba una pregunta
sin respuesta: *¿está llegando de verdad?* Sabes que el paquete salió, pero no
cómo se ve el enlace **desde el coche**.

Ahora el coche contesta. Su paquete de vuelta es aún más pequeño, **5 bytes**:

```c
typedef struct __attribute__((packed)) Telemetria {
  uint32_t t_ms_eco;        // eco del t_ms del último control recibido -> RTT
  int8_t   rssi_coche;      // RSSI (dBm) que ve el COCHE de los paquetes de control
} Telemetria;
```

No va a 50 Hz: la telemetría es información de apoyo, no de control, así que el
coche la manda **unas 5 veces por segundo**. Con eso sobra para una pantalla, y no
le robamos hueco de radio al enlace que de verdad importa.

### Cómo se mide la latencia sin relojes sincronizados

Este es el truco bonito del asunto. Queremos saber cuánto tarda una orden en ir y
volver, pero los dos ESP32 arrancaron en momentos distintos, así que sus relojes
no tienen nada que ver entre sí. Comparar "la hora del coche" con "la hora de la
emisora" no diría absolutamente nada.

La solución es no comparar relojes, sino **comparar el reloj de la emisora consigo
mismo**:

1. La emisora sella cada paquete con su propio `millis()`, justo **antes de
   enviarlo** (en `control_tx_task`, en el momento real del envío, no antes).
2. El coche recibe el paquete, se guarda ese número y **lo devuelve tal cual** en
   `t_ms_eco`. No lo interpreta, no lo modifica: es un espejo.
3. Cuando la respuesta vuelve, la emisora resta:

```c
rtt_ms = millis() - t.t_ms_eco;
```

El resultado es el **RTT** (*Round-Trip Time*, tiempo de ida y vuelta) medido de
principio a fin **con un solo reloj**. Da igual la hora que crea que es el coche.
Es la misma idea del `ping` de toda la vida.

En el código, quien hace esa cuenta es el callback que registramos al arrancar:

```c
esp_now_register_recv_cb(on_telemetria);
```

Ese callback corre en el contexto de la tarea de Wi-Fi, y se limita a tres cosas:
calcular el RTT, guardar el RSSI y **apuntar la hora** en que llegó la telemetría.
Esa última marca es la que permite decidir si el coche sigue ahí: si la última
telemetría llegó hace **más de un segundo**, la pantalla da el enlace por caído.

### El RSSI: lo importante es lo que oye el coche

El otro dato que devuelve el coche es el **RSSI** (*Received Signal Strength
Indicator*), la potencia con la que le llega la señal, en **dBm**. Siempre es un
número negativo, y cuanto más cerca de cero, mejor: −60 dBm es una señal fuerte,
−100 dBm es estar al borde.

Fíjate en el detalle: es el RSSI que ve **el coche**, no el que vemos nosotros.
Es lo que interesa. El enlace se rompe cuando el coche deja de oír nuestras
órdenes, y ahí un dato medido en la emisora no serviría de mucho: podríamos estar
oyendo perfectamente su telemetría mientras nuestros paquetes de control se
pierden por el camino.

Cómo se traduce todo esto a barritas en la pantalla, con qué umbrales y por qué
esos y no otros, está en
[Pantalla y telemetría](12-PANTALLA-Y-TELEMETRIA.md).

### El receptor no necesita saber nuestra MAC

Un detalle que ahorra trabajo: en la emisora tenemos que poner a mano la MAC del
coche (`client_mac`), porque somos nosotros los que iniciamos la conversación.
Pero **en el receptor no hay que hardcodear la MAC de la emisora**.

Cuando llega un paquete por ESP-NOW, la información de recepción trae de serie
quién lo envió, así que el receptor **aprende la dirección del propio paquete** y
contesta a ese remitente. Ventaja práctica: puedes cambiar de placa emisora, o
flashear el receptor en otro coche, sin tocar ni una línea del código del
receptor.

---

« [Anterior: Leer el mando](03-LEER-EL-MANDO.md) · [📚 Índice](README.md) · [Siguiente: Lógica de conducción »](05-LOGICA-DE-CONDUCCION.md)
