# 3. Leer el mando: USB Host, XInput y TinyUSB

Este es el tramo más técnico y, para mí, el más interesante. Cuando enchufas un
mando de Xbox al ESP32-S3, pasan tres cosas que conviene separar.

## USB Host: quién manda en el cable

En un cable USB siempre hay dos papeles: el **host** (el que manda, normalmente
tu ordenador) y el **device** (el que obedece, como un ratón o un mando). El host
da corriente, pregunta "¿tú quién eres?" y organiza toda la conversación.

Tu PC es siempre el host. Un mando es siempre un device. Para que el ESP32 pueda
leer un mando, tiene que **hacer de host**, o sea, comportarse como si fuera el
ordenador. El ESP32-S3 puede hacerlo porque tiene el hardware necesario; por eso
es el elegido para la emisora.

## XInput: el idioma raro de los mandos de Xbox

Aquí viene la trampa. Uno esperaría que un mando fuese un dispositivo USB estándar
y corriente (lo que se llama HID, como un teclado o un ratón). Pues los mandos de
Xbox **no**. Microsoft usa su propio protocolo, llamado **XInput**, que no es HID
normal. Si le hablas al mando como si fuera un ratón, no te entiende.

Así que necesitas un traductor que sepa hablar XInput: pedirle los datos como él
espera e interpretar lo que devuelve (posición de sticks, gatillos, botones...).

## TinyUSB: la fontanería, y el driver de Ryzee119

Montar toda la maquinaria USB desde cero sería una locura, así que uso dos
librerías que hacen el trabajo pesado:

- **TinyUSB** pone toda la fontanería del USB Host: enumerar el dispositivo,
  gestionar los "endpoints", mover los bytes de un lado a otro.
- El **driver `tusb_xinput` de Ryzee119** se apoya en TinyUSB y añade el idioma
  XInput por encima. Es quien de verdad entiende que "esto es un mando de Xbox".

Ambas vienen incluidas en la carpeta `components/` de este repo, así que no tienes
que descargar nada aparte.

## El parche que tuve que hacer (y por qué)

Aquí un detalle honesto, por si te toca hacer lo mismo. El driver de Ryzee119
estaba escrito para una versión un poco más antigua de TinyUSB. En la versión que
uso, TinyUSB cambió una regla interna:

> Cuando el sistema le pregunta a un driver "¿reconoces este dispositivo?", antes
> el driver respondía sí/no. Ahora tiene que responder **cuántos bytes de la
> descripción ha consumido** (un número), no un simple sí/no.

Traducido al código, la función `xinputh_open` devolvía un `bool` (verdadero o
falso) y tuve que cambiarla para que devolviera un `uint16_t` (el número de bytes
que había procesado). Si no lo haces, el proyecto ni siquiera compila, porque los
tipos no encajan. Puedes ver el cambio comentado como `CORRECCIÓN 1` dentro de
[`components/src/lib/tusb_xinput/xinput_host.c`](../components/src/lib/tusb_xinput/xinput_host.c).

Ese parche es la razón por la que la librería viene **incluida** en el repo en vez
de descargarse sola: si la bajaras de su sitio original, vendría sin el arreglo y
no compilaría.

Hubo un segundo retoque, esta vez en el `CMakeLists.txt` de TinyUSB
([`components/tinyusb/CMakeLists.txt`](../components/tinyusb/CMakeLists.txt)). Con un
GCC más moderno (el que trae el ESP-IDF nuevo), el compilador empezó a avisar de un
`-Wtype-limits` dentro de `get_driver()` de `usbh.c`: como aquí **no usamos ningún
driver USB de fábrica** (solo el de XInput como driver de aplicación), un contador
interno vale cero y una comparación queda "siempre falsa". No es un fallo real, pero
como ESP-IDF trata los avisos como errores, tumbaba la compilación. La solución es
decirle que **no** convierta ese aviso concreto en error para el código de TinyUSB
(que es de terceros), con `target_compile_options(... -Wno-error=type-limits)`. Así
el proyecto compila igual en el ordenador de casa que en uno con un GCC más nuevo.

## El detalle del mando de Xbox Series

Un apunte curioso que hay en la configuración
([`main/tusb_config.h`](../main/tusb_config.h)): tuve que **ampliar los buffers**
de USB. Los mandos modernos de Xbox tienen una "carta de presentación" (su
descriptor USB) enorme, porque además de los botones llevan audio para los cascos,
varios endpoints, etc. Con los valores de fábrica el mando no llegaba a presentarse
entero y fallaba al conectarse. Subiendo el `CFG_TUH_ENUMERATION_BUFSIZE` a 512 y
el número máximo de endpoints a 16, entra sin problemas.

---

## La lección de la vibración: cuando el rumble tumbaba el enlace

Este apartado es el más largo del capítulo y no está aquí por gusto: es el fallo
más difícil que he tenido en el proyecto, y de él salió una regla que se aplica a
cualquier pila USB. Si algún día haces vibrar un mando desde un ESP32, esto te
puede ahorrar una tarde entera.

### El síntoma: un puzle que no encajaba

Los síntomas eran tres, y a primera vista no tenían nada que ver entre sí:

1. De vez en cuando, la emisora **se colgaba** (el host USB se quedaba mudo y
   dejaba de leer el mando).
2. A veces el mando se quedaba con la **vibración pegada**, zumbando sin parar.
3. Y lo más raro: el **`PING` de la pantalla se disparaba** por encima de un
   segundo y se ponía a bailar. El enlace de radio se degradaba... al vibrar el
   mando.

Que un motorcito de vibración afecte al Wi-Fi no tiene ningún sentido. Hasta que
lo tiene.

### La causa: reentrada en una pila que no la soporta

El driver de XInput ofrece una función para hacer vibrar el mando:

```c
tuh_xinput_set_rumble(dev_addr, instance, izq, der, block);
```

Ese último parámetro, `block`, parece inofensivo: "espera a que el comando salga
de verdad antes de volver". Yo lo tenía a `true`, que suena a lo más seguro. Es
justo lo contrario.

Con `block = true`, el driver llama por dentro a `wait_for_tx_complete()`, que es
básicamente esto:

```c
while (usbh_edpt_busy(...)) tuh_task();
```

Léelo despacio. Está llamando a **`tuh_task()`**. Y `tuh_task()` es la función
principal de la pila USB, la que hay que llamar en bucle desde su tarea... y
dentro de la cual corren los callbacks. Es decir: si pides una vibración desde el
callback del reporte del mando, que corre **dentro** de `tuh_task()`, acabas
llamando a `tuh_task()` desde dentro de `tuh_task()`.

Eso se llama **reentrada**, y **la pila USB de TinyUSB no es reentrante**. Sus
estructuras internas (colas, estado de los endpoints, la máquina de estados del
control) dan por supuesto que solo hay una ejecución en marcha. Al reentrar, ese
estado se corrompe: de ahí los cuelgues del host y la vibración que se quedaba
enganchada.

Y había un agravante. `tuh_xinput_send_report` llama a `usbh_edpt_claim()` para
reservar el endpoint, y **si el endpoint está ocupado falla en silencio**: no
manda nada. Pero `set_rumble` **devuelve `true` de todas formas**. Así que un
comando de "apaga la vibración" podía perderse por el camino sin que nadie se
enterara, y el motor se quedaba girando para siempre.

### El efecto colateral que explica el PING

Y aquí se cierra el círculo del misterio de la radio. Un motor de vibración
atascado en marcha **consume bastante corriente del puerto USB de la emisora**. Ese
consumo **hunde la tensión de alimentación** de la placa, y el bloque más sensible
a una alimentación flojeando es, precisamente, la **radio**: el amplificador de
Wi-Fi es lo que más pico de corriente pide de todo el chip. Resultado: reintentos,
paquetes perdidos y un `PING` que se va a más de un segundo y fluctúa.

O sea que el `PING` no se estropeaba "a la vez" que el rumble: se estropeaba
**por** el rumble. Un fallo de software en el USB acababa manifestándose como un
problema de radio, pasando por la electrónica de por medio. Nada mal.

La guinda: **el mando conserva su estado de vibración aunque reinicies el ESP32**.
La vibración vive en el mando, no en la emisora. Así que reiniciar la placa no
arreglaba nada; el zumbido seguía ahí, tan tranquilo, hundiendo la tensión. Es
tremendamente desorientador cuando estás depurando, porque rompe la suposición más
básica de todas: "si reinicio, empiezo de cero".

### La arquitectura final: separar querer de hacer

El arreglo no fue parchear la llamada, sino reorganizar quién puede hablar con el
USB. La idea es la que se usa siempre en estos casos: **partir en dos la
operación**, la intención y la ejecución.

**Las funciones que cualquiera puede llamar solo fijan un objetivo.** No tocan el
USB en absoluto:

```c
static void vibrar_lr(uint8_t l, uint8_t r, uint32_t ms) {
    rumbleObjL = l;                  // "quiero esta fuerza"
    rumbleObjR = r;
    DURACION_VIBRACION = ms;
    vibracionActiva = true;
    tiempoInicioVibracion = millis();
}
```

`vibrar_lr()`, su versión simple `vibrar()` y `rumble_off()` son las tres así:
escriben un par de variables y vuelven. Por eso son **seguras de llamar desde
cualquier sitio**, incluido el callback del reporte. Cuando aporreas la cruceta en
el modo configuración y cada paso pide su tic de vibración, no puede pasar nada:
solo se está sobreescribiendo un número.

**Y hay un único sitio que habla con el USB:** `servicio_rumble()`, llamada desde
`tusb_host_task`, fuera de cualquier callback, y **siempre con `block = false`**:

```c
static void servicio_rumble(void) {
    if (millis() - tUltimoRumble < 40) return;      // no saturamos el endpoint
    bool cambio   = (rumbleObjL != rumbleEnvL) || (rumbleObjR != rumbleEnvR);
    bool insistir = (rumbleObjL == 0 && rumbleObjR == 0 && reenviosOff > 0);
    if (!cambio && !insistir) return;

    tUltimoRumble = millis();
    tuh_xinput_set_rumble(mando_dev_addr, mando_instance, rumbleObjL, rumbleObjR, false);
    rumbleEnvL = rumbleObjL;
    rumbleEnvR = rumbleObjR;
    if (insistir) reenviosOff--;
}
```

Tiene tres defensas que merecen nombre:

- **Throttle de 40 ms.** No se manda un comando más a menudo que eso, para no
  saturar el endpoint (que es lo que provocaba los fallos silenciosos).
- **Solo envía si hay cambio.** Compara lo que se quiere (`rumbleObj*`) con lo
  último que se envió (`rumbleEnv*`). Si son iguales, no hay nada que decir.
- **Cuatro reenvíos del "apagar".** Como un envío puede descartarse en silencio,
  `rumble_off()` deja un contador a 4 y el "apagar" se repite hasta cuatro veces.
  Encender de más es una molestia; **quedarse encendido es el fallo grave**, así
  que el apagado se manda con insistencia.

Y un último remate: **al montar el mando se le manda un "apagar"**. Como el mando
recuerda su vibración entre reinicios, lo primero que hace la emisora al detectar
un mando nuevo es levantar la bandera `pedirRumbleOff` para limpiar cualquier
zumbido que hubiera quedado colgado de una sesión anterior. Arrancar en un estado
conocido, en vez de dar por supuesto que el otro extremo está limpio.

### Las dos reglas que quedan escritas

Si te llevas dos cosas de este apartado, que sean estas:

> **1. Nunca llames a una función que hable con el USB desde el callback del
> reporte.** Ese callback corre **dentro** de `tuh_task()`. Cualquier llamada que
> pueda acabar volviendo a entrar en la pila la corrompe. El callback **anota
> banderas y variables**; el trabajo de verdad lo hace la tarea.
>
> **2. Nunca uses `block = true` en este driver.** No es "la opción segura": es la
> que provoca la reentrada. `block = false` y un servicio con throttle que
> reintente si hace falta.

La primera regla es general y aparece por todas partes en este proyecto: el
temporizado del cambio de modo, los 2 segundos del modo configuración, la
escritura en la NVS: todo se decide en la tarea, nunca en el callback. Está
contado con más detalle en
[FreeRTOS y núcleos](07-FREERTOS-Y-NUCLEOS.md).

---

« [Anterior: Arquitectura del sistema](02-ARQUITECTURA-DEL-SISTEMA.md) · [📚 Índice](README.md) · [Siguiente: Comunicación ESP-NOW »](04-COMUNICACION-ESP-NOW.md)
