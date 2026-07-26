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

« [Anterior: Arquitectura del sistema](02-ARQUITECTURA-DEL-SISTEMA.md) · [📚 Índice](README.md) · [Siguiente: Comunicación ESP-NOW »](04-COMUNICACION-ESP-NOW.md)
