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

## `app_main` puede terminar

Una cosa que despista al venir de Arduino: aquí `app_main` (nuestro "setup")
**termina y no pasa nada**. En Arduino el `loop()` tiene que seguir vivo para
siempre; en ESP-IDF, una vez creada la tarea del USB, `app_main` puede acabar
tranquilamente porque la tarea sigue corriendo por su cuenta.

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
