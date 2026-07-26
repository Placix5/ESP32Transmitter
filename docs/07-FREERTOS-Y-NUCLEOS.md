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

En el código real, dentro de ese mismo bucle también se comprueban dos cosas con
temporización: el **cambio de modo** (mantener L1 + R1 dos segundos) y el **apagado
de la vibración** pasado su tiempo. Podría parecer más natural hacerlo en el
callback que recibe los datos del mando, pero hay una trampa: **el mando solo envía
reportes cuando algo cambia**. Si mantienes los dos botones quietos, deja de
reportar… y un temporizador que solo se revisara "cuando llega un reporte" nunca
llegaría a cumplirse (por eso a veces el modo no cambiaba).

La solución es revisar esos tiempos aquí, en `tusb_host_task`, que gira a cadencia
fija en cada vuelta del bucle, pase lo que pase con el mando. El callback del mando
se limita a **anotar** el estado de los botones; quien decide "ya han pasado los
2 segundos" es esta tarea.

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

        esp_now_send(client_mac, (uint8_t *)&copia, sizeof(copia));

        vTaskDelayUntil(&ultimaHora, periodo);
    }
}
```

Un par de detalles interesantes:

- **`vTaskDelayUntil`, no `vTaskDelay`.** La diferencia es sutil pero importante:
  `vTaskDelayUntil` cuenta desde un punto fijo, así que el envío cae cada 20 ms de
  reloj de forma regular, sin que se le vaya acumulando el tiempo que tarda en hacer
  su trabajo. El resultado es un pulso de 50 Hz mucho más estable.
- **Va clavada en el núcleo 1**, separada del USB (que vive en el núcleo 0). Cada
  trabajo en su núcleo: el mando en uno, la radio en el otro, sin estorbarse.

Por qué este flujo constante importa tanto (failsafe del receptor, detección de
pérdida de señal) está contado en
[Comunicación ESP-NOW](04-COMUNICACION-ESP-NOW.md).

## El estado compartido y el candado (`portMUX`)

Fíjate en que ahora hay dos tareas, en dos núcleos, tocando la misma información: el
callback del mando (núcleo 0) **escribe** el último estado a enviar, y
`control_tx_task` (núcleo 1) lo **lee** para mandarlo. Esa información es una
variable compartida, `estadoTx`.

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
protegido es minúsculo (copiar 3 bytes), el candado se suelta enseguida y no frena
a nadie. Y `estadoTx` arranca en **NEUTRO**, así que mientras no haya mando enchufado
el coche recibe "parado y recto" — la base del failsafe que se explicó antes.

## `app_main` puede terminar

Una cosa que despista al venir de Arduino: aquí `app_main` (nuestro "setup")
**termina y no pasa nada**. En Arduino el `loop()` tiene que seguir vivo para
siempre; en ESP-IDF, una vez creadas las tareas (la del USB en el núcleo 0 y la de
transmisión en el núcleo 1), `app_main` puede acabar tranquilamente porque las
tareas siguen corriendo por su cuenta.

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
