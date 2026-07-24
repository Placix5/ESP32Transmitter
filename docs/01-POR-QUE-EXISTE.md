# 1. Por qué existe

## El problema que lo empezó todo

Mi primera versión de esto vivía dentro del coche. Un ESP32, un mando de Xbox
conectado por Bluetooth, y a rodar. Se conducía genial... si no te separabas más
de la distancia de un salón.

Medí el alcance real y no pasaba de **5 metros** antes de que empezaran a caerse
las órdenes. Y tiene todo el sentido del mundo: el Bluetooth está diseñado para
los auriculares que llevas puestos y el teclado que tienes en la mesa. Es un
protocolo pensado para gastar poquísima energía a corta distancia, no para
gobernar un coche que se te va al fondo del aparcamiento.

Podría haber intentado exprimir el Bluetooth, pero por mucho que lo apretara
seguía siendo la herramienta equivocada. Necesitaba otra cosa.

> Puedes ver esa primera versión aquí:
> [ESP32-RC-Controller](https://github.com/Placix5/ESP32-RC-Controller). Sigue
> siendo un buen punto de partida si te vale con poco alcance y quieres algo más
> sencillo de montar.

## La idea: partir el sistema en dos

El cambio de mentalidad fue este: **el mando no tiene por qué hablar con el
coche**. El mando puede hablar con una *emisora* que tengo yo en la mano, y esa
emisora ya se encarga de llegar hasta el coche por su cuenta.

Justo como funciona el RC "de verdad": nadie conecta el joystick directamente al
coche. Hay una emisora que lee tus movimientos y los manda por radio.

Así que el sistema quedó en dos mitades:

- **La emisora** (este repositorio). Un ESP32-S3 con el mando de Xbox enchufado
  por USB. Lee lo que haces con los sticks y gatillos.
- **El receptor**. Un ESP32-C3 metido en el coche, que recibe las órdenes y
  mueve el servo y el variador.

Y entre las dos mitades, en vez de Bluetooth, uso **ESP-NOW**: un protocolo de
radio de Espressif que llega muchísimo más lejos.

El bonito efecto secundario es que ahora tengo, literalmente, una emisora casera.
El "mando" físico es un pad de Xbox, pero por debajo es un transmisor de radio de
largo alcance que me costó cuatro perras.

En la siguiente sección vemos por qué elegí dos chips distintos y seguimos el
camino completo de una orden, desde tu pulgar hasta las ruedas.

---

« [📚 Índice](README.md) · [Siguiente: Arquitectura del sistema »](02-ARQUITECTURA-DEL-SISTEMA.md)
