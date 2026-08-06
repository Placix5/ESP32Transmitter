# 7. FreeRTOS y los dos núcleos

El ESP32-S3 tiene **dos núcleos** de procesador, y por debajo de nuestro código
corre **FreeRTOS**, un sistema operativo minúsculo pensado para tiempo real. Esto
nos permite hacer varias cosas "a la vez" repartiéndolas en tareas.

¿Por qué nos importa aquí? Porque el USB es exigente. Para mantener la conversación
con el mando, hay que llamar a una función de TinyUSB (`tuh_task`)
**constantemente**, sin parar. Si el programa se entretiene en otra cosa, la
comunicación con el mando se resiente.

## Una tarea dedicada solo al USB

La solución es darle al USB su propia tarea:

```c
void tusb_host_task(void *arg) {
    while (1) {
        tuh_task();      // atiende al USB
        vTaskDelay(1);   // cede un momento el turno a los demás
    }
}
```

Y al arrancar, la lanzamos **clavada en el núcleo 0**, para que atienda el mando
sin que le molesten otras tareas:

```c
xTaskCreatePinnedToCore(tusb_host_task, "tusb_host_task", 4096, NULL, 5, NULL, 0);
```

Ese `vTaskDelay(1)` de dentro del bucle es pequeño pero clave: le dice a FreeRTOS
"he terminado por ahora, deja correr a los demás un instante". Sin él, esta tarea
acapararía el núcleo entero y el sistema se quejaría (hay un vigilante, el
*watchdog*, que reinicia la placa si una tarea no suelta nunca el control).

### Esta tarea hace algo más que `tuh_task()`

En el código real, dentro de ese mismo bucle se comprueban unas cuantas cosas más:

- el **cambio de modo** (mantener L1 + R1 dos segundos),
- el **apagado de la vibración** pasado su tiempo,
- **entrar y salir del modo configuración** (mantener el D-pad abajo dos segundos),
- la **escritura del trim en la NVS** cuando se ha pedido con el botón A,
- el **envío real de los comandos de vibración** al mando,
- y el **parpadeo del LED** en blanco mientras estás en configuración.

Podría parecer más natural hacer todo eso en el callback que recibe los datos del
mando, pero hay una trampa: **el mando solo envía reportes cuando algo cambia**. Si
mantienes los dos botones quietos, deja de reportar… y un temporizador que solo se
revisara "cuando llega un reporte" nunca llegaría a cumplirse (por eso a veces el
modo no cambiaba).

La solución es revisar esos tiempos aquí, en `tusb_host_task`, que gira a cadencia
fija en cada vuelta del bucle, pase lo que pase con el mando. El callback del mando
se limita a **anotar** el estado de los botones; quien decide "ya han pasado los
2 segundos" es esta tarea.

### El principio: el callback anota, la tarea actúa

Ese reparto no es solo por el asunto de los temporizadores. Es el principio de
diseño más importante de este proyecto, y conviene decirlo con todas las letras:

> **El callback del reporte no habla con el USB. Solo levanta banderas.**

El motivo es que ese callback corre **dentro** de `tuh_task()`. Cualquier función
que pueda acabar volviendo a entrar en la pila USB la corrompe, porque TinyUSB no
es reentrante. Eso provocó cuelgues del host, vibraciones pegadas y, por un camino
retorcido que pasa por la caída de tensión del USB, hasta degradación del enlace de
radio. La historia completa, que es de las buenas, está en
[Leer el mando](03-LEER-EL-MANDO.md).

En el código el patrón se ve a simple vista: hay una colección de banderas
(`pedirGuardar`, `avisoPendiente`, `pedirRumbleOff`, `dpadDownHeld`,
`rumbleObjL`/`rumbleObjR`…) que el callback **escribe** y que `tusb_host_task`
**consume**. El callback termina rápido y sin efectos secundarios; el trabajo que
puede bloquear, tardar o tocar hardware delicado se hace en la tarea.

Es un patrón que reconocerás en cualquier sistema embebido: en una interrupción, en
un callback de red, en un handler de eventos. Haz lo mínimo, anota lo que hay que
hacer, y vuelve.

## La tarea de transmisión: 50 Hz clavados en el núcleo 1

La emisora no envía el paquete "cada vez que el mando dice algo". Tiene una tarea
propia, `control_tx_task`, que manda el último estado al coche **50 veces por
segundo, a ritmo fijo**:

```c
void control_tx_task(void *arg) {
    const TickType_t periodo = pdMS_TO_TICKS(20);   // 20 ms = 50 Hz
    TickType_t ultimaHora = xTaskGetTickCount();
    while (1) {
        MensajeRadio copia;
        portENTER_CRITICAL(&txMux);
        copia = estadoTx;                 // instantánea del estado compartido
        portEXIT_CRITICAL(&txMux);

        copia.t_ms = millis();            // sello de tiempo en el momento real de envío
        esp_now_send(client_mac, (uint8_t *)&copia, sizeof(copia));

        vTaskDelayUntil(&ultimaHora, periodo);
    }
}
```

Tres detalles interesantes:

- **`vTaskDelayUntil`, no `vTaskDelay`.** La diferencia es sutil pero importante:
  `vTaskDelayUntil` cuenta desde un punto fijo, así que el envío cae cada 20 ms de
  reloj de forma regular, sin que se le vaya acumulando el tiempo que tarda en hacer
  su trabajo. El resultado es un pulso de 50 Hz mucho más estable.
- **Va clavada en el núcleo 1**, separada del USB (que vive en el núcleo 0). Cada
  trabajo en su núcleo: el mando en uno, la radio en el otro, sin estorbarse.
- **El sello de tiempo se pone aquí**, fuera del candado y justo antes de
  `esp_now_send`, no cuando se leyó el mando. Así el `t_ms` es de verdad "la hora a
  la que salió el paquete", y la medida de latencia que el coche nos devuelve no
  incluye el rato que ese estado llevaba esperando su turno. Cómo se usa ese sello
  está en [Comunicación ESP-NOW](04-COMUNICACION-ESP-NOW.md).

Por qué este flujo constante importa tanto (failsafe del receptor, detección de
pérdida de señal) está contado también en
[Comunicación ESP-NOW](04-COMUNICACION-ESP-NOW.md).

## La tarea de la pantalla: 15 Hz y sin prisa

La tercera tarea es `oled_task`, y se lanza así:

```c
xTaskCreatePinnedToCore(oled_task, "oled_task", 4096, NULL, 3, NULL, 1);
```

Refresca la pantalla cada **66 ms** (unas **15 veces por segundo**), va en el
**núcleo 1** y, esto es lo importante, con **prioridad 3**, por debajo del USB y de
la transmisión, que van a **5**.

¿Por qué no aprovechar el bucle de `control_tx_task`, que ya gira a 50 Hz? Porque
**no cabe**. El ciclo de control tiene un presupuesto de 20 ms, y un volcado
completo del SSD1306 por I2C puede llevarse **unos 25**. Si el peor caso de la
pantalla se comiera el ciclo del control, la emisora dejaría de mandar a ritmo
fijo, que es la base del failsafe. Un adorno no puede poner en riesgo la función
principal.

Y de ahí la prioridad baja: si en algún momento hay que elegir, quien pasa primero
es el mando y la radio. Que la pantalla se salte un refresco no tiene la menor
importancia; que se salte un paquete de control, sí. **La prioridad de una tarea
debería reflejar las consecuencias de que llegue tarde.**

> El detalle de cómo el driver de la pantalla consigue que 15 Hz salgan **más
> baratos** que los 5 Hz de antes (volcando solo las franjas que han cambiado) está
> en [Pantalla y telemetría](12-PANTALLA-Y-TELEMETRIA.md).

## El reparto final: quién corre dónde

Así queda la emisora una vez arrancada:

| Tarea | Núcleo | Prioridad | Cadencia | De qué se encarga |
|-------|--------|-----------|----------|-------------------|
| `tusb_host_task` | 0 | 5 | cada tick | USB Host, callbacks del mando, temporizados, rumble, NVS, LED |
| `control_tx_task` | 1 | 5 | 20 ms (50 Hz) | Enviar el estado al coche por ESP-NOW |
| `oled_task` | 1 | 3 | 66 ms (~15 Hz) | Pintar la telemetría en la pantalla |
| *(tarea de Wi-Fi de ESP-IDF)* | — | — | por evento | Ejecuta `on_telemetria` cuando llega un paquete del coche |

La lógica del reparto es sencilla: **el núcleo 0 para el USB**, que es el más
exigente y el que peor se lleva con las interrupciones ajenas, y **el núcleo 1 para
la radio y la pantalla**. La telemetría que vuelve no necesita tarea propia: se
atiende en un callback que registra ESP-NOW, y ese callback hace lo mínimo (una
resta, guardar dos números y apuntar la hora) precisamente porque corre en una
tarea que no es nuestra.

## El estado compartido y el candado (`portMUX`)

Fíjate en que hay varias tareas, en dos núcleos, tocando la misma información: el
callback del mando (núcleo 0) **escribe** el último estado a enviar, y tanto
`control_tx_task` como `oled_task` (núcleo 1) lo **leen**, una para mandarlo por
radio y la otra para pintarlo. Esa información es una variable compartida,
`estadoTx`.

Cuando dos núcleos acceden a la vez a lo mismo, hay que tener cuidado: si uno lee
justo en mitad de que el otro está escribiendo, podría llevarse una mezcla
incoherente (el ángulo nuevo con el motor viejo, por ejemplo). Para evitarlo se usa
un **candado**, un `portMUX` llamado `txMux`:

```c
portENTER_CRITICAL(&txMux);
// ... leer o escribir estadoTx de un tirón ...
portEXIT_CRITICAL(&txMux);
```

Entre ese "entrar" y "salir", nadie más puede tocar la variable. Como el trozo
protegido es minúsculo (copiar 7 bytes), el candado se suelta enseguida y no frena
a nadie. Y `estadoTx` arranca en **NEUTRO**, así que mientras no haya mando enchufado
el coche recibe "parado y recto", la base del failsafe que se explicó antes.

### Cuándo no hace falta candado

Compara eso con la telemetría que llega del coche (`rtt_ms`, `rssi_coche`,
`ultimaTelemetria`). Ahí también hay dos contextos implicados (la tarea de Wi-Fi
escribe, `oled_task` lee) y sin embargo no hay ningún `portMUX`. Solo un
`volatile`.

¿Por qué? Porque son **variables sueltas de 32 y 8 bits, alineadas**. El ESP32 las
lee y escribe en una sola instrucción, así que no existe el "a medias": o te llevas
el valor viejo o el nuevo, nunca una mezcla. El `volatile` está para otra cosa: para
decirle al compilador que **no se las guarde en un registro** dando por hecho que
nadie más las cambia.

`estadoTx` sí necesita el candado porque son **varios campos que tienen que
viajar juntos**. Ahí sí puede pasar lo malo: leer el ángulo nuevo con el motor
viejo. La regla es esa: un dato suelto y pequeño se apaña con `volatile`; un
**conjunto que debe ser coherente** necesita candado.

## `app_main` puede terminar

Una cosa que despista al venir de Arduino: aquí `app_main` (nuestro "setup")
**termina y no pasa nada**. En Arduino el `loop()` tiene que seguir vivo para
siempre; en ESP-IDF, una vez creadas las tres tareas (USB en el núcleo 0,
transmisión y pantalla en el núcleo 1), `app_main` puede acabar tranquilamente
porque las tareas siguen corriendo por su cuenta.

## El orden de arranque del USB

Un último detalle de `app_main`: antes de encender la pila USB hay que preparar el
hardware físico del puerto (el "PHY"). El orden correcto es primero configurar el
PHY en modo host y luego arrancar TinyUSB:

```c
usb_new_phy(&phy_config, &phy_handle);  // 1. preparar el hardware del puerto
tusb_init();                            // 2. arrancar la pila USB encima
```

Si lo haces al revés, la pila arranca sin un suelo donde apoyarse y no funciona.

---

« [Anterior: PWM](06-PWM.md) · [📚 Índice](README.md) · [Siguiente: El receptor »](08-EL-RECEPTOR.md)
