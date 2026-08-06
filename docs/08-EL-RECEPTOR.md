# 8. El receptor: el otro lado de la radio

El código del receptor vive en su **propio repositorio**,
[`ESP32Receiver`](https://github.com/Placix5/ESP32Receiver), y es,
como la emisora, un proyecto **ESP-IDF nativo**: ya no es Arduino ni PlatformIO,
así que las dos mitades comparten herramientas y forma de compilar. Para entender el
sistema completo conviene saber qué hace, y la buena noticia es que es **mucho más
sencillo** que la emisora. Su vida entera cabe en cinco pasos:

1. **Escuchar por ESP-NOW.** Al arrancar, el C3 se pone a la espera de paquetes.
   No inicia ninguna conexión; solo escucha.
2. **Desempaquetar los 7 bytes.** Cuando llega un paquete, saca de él los tres
   números que le sirven (modo, valor del motor y ángulo del servo) y se guarda el
   cuarto campo, la **marca de tiempo** de la emisora, para devolverla luego. Como
   la estructura está `packed` a los dos lados, los bytes encajan exactos.
3. **Generar dos señales PWM.** Con esos números, produce el pulso de 50 Hz que ya
   conoces (ver [PWM](06-PWM.md)): uno para el servo (ángulo de dirección) y otro
   para el variador (potencia del motor).
4. **Contestar con telemetría.** Unas **5 veces por segundo** manda de vuelta un
   paquete de 5 bytes con el eco de la marca de tiempo y el **RSSI** con el que le
   están llegando las órdenes. No necesita tener apuntada la MAC de la emisora: la
   **aprende del propio paquete** que acaba de recibir y contesta a ese remitente.
5. **Vigilar que la emisora siga ahí (fail-safe).** Esto es lo más importante por
   seguridad: si el receptor deja de recibir paquetes durante un tiempo (porque
   apagaste la emisora, se agotó la batería o te fuiste de rango), debe **poner el
   motor en punto muerto** por su cuenta. Un coche de RC que se queda "a tope"
   porque perdió la señal es un problema serio.

> **Doble red de seguridad.** Este fail-safe del receptor no está solo: la emisora
> **también** tiene el suyo. Si desenchufas el mando, la emisora pasa a NEUTRO al
> instante y lo sigue enviando a 50 Hz (ver
> [Comunicación ESP-NOW](04-COMUNICACION-ESP-NOW.md)), así que el coche para sin
> esperar a nada. El fail-safe del receptor cubre el otro caso: que la emisora se
> apague, se quede sin batería o te vayas de rango y **dejen de llegar paquetes**.
> Entre los dos tapan los dos agujeros.

> **Un apunte sobre el enlace.** Para que el receptor y la emisora se hablen, el C3
> tiene que estar configurado **exactamente igual** en la parte de radio: mismo
> **Long Range** (`WIFI_PROTOCOL_LR`) y en el mismo canal. Si solo un lado usa Long
> Range, no se comunican. Los detalles están en
> [Comunicación ESP-NOW](04-COMUNICACION-ESP-NOW.md).

> ### ⚠️ Las estructuras tienen que ser gemelas
>
> El receptor **descarta los paquetes que no midan lo que espera**. Es lo correcto,
> pero significa que `MensajeRadio` y `Telemetria` tienen que ser **idénticas** en
> los dos repositorios, campo por campo.
>
> Cuando el paquete de control pasó de 3 a 7 bytes (al añadirle la marca de tiempo),
> flashear solo una de las dos placas dejaba el coche completamente inmóvil, sin un
> solo mensaje de error. Si tocas una estructura, **flashea los dos extremos**.

## Por qué el receptor es tan simple

Fíjate en el reparto de trabajo: toda la parte "lista" del sistema (leer el mando,
interpretar XInput, decidir modos, hacer las cuentas) vive en la emisora. Al coche
le llega el trabajo ya hecho, en forma de tres números listos para usar. Por eso el
receptor puede ser un chip pequeño y barato: solo tiene que escuchar y mover dos
salidas.

Es una decisión de diseño a propósito. Cuanto menos tenga que pensar el coche,
menos cosas pueden fallar dentro de él y más fácil es mantenerlo.

---

« [Anterior: FreeRTOS y núcleos](07-FREERTOS-Y-NUCLEOS.md) · [📚 Índice](README.md) · [Siguiente: Requisitos y entorno »](09-ENTORNO-ESP-IDF.md)
