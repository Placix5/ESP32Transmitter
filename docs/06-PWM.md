# 6. PWM: el idioma del servo y el variador

Aquí llegamos a cómo un simple número mueve un motor. La respuesta es **PWM**, y
merece la pena entenderlo porque es el idioma de casi todo el radiocontrol.

## Qué es PWM

PWM significa *Pulse Width Modulation*, modulación por ancho de pulso. Suena a
mucho, pero la idea es sencilla: **encender y apagar un pin muy rápido**, y jugar
con *cuánto tiempo* está encendido en cada ciclo.

En radiocontrol, tanto los servos como los variadores esperan una señal muy
concreta:

- Un pulso **50 veces por segundo** (50 Hz). O sea, un ciclo cada 20 milisegundos.
- Lo que importa es **cuánto dura la parte "encendida"** de cada pulso:
  - **1 ms encendido** → servo a un extremo / motor a tope en un sentido.
  - **1,5 ms** → posición central / motor parado (neutro).
  - **2 ms** → servo al otro extremo / motor a tope en el otro sentido.

El servo, por dentro, lee ese ancho de pulso y mueve su brazo hasta el ángulo
correspondiente. El variador hace lo mismo pero con la potencia del motor.

## Por qué existe PWM y no algo más simple

Antiguamente, para regular potencia se usaban resistencias variables (como el mando
de volumen analógico de toda la vida). El problema es que una resistencia que
"sobra" energía la convierte en calor: derrochas batería y el componente se
calienta. PWM lo resuelve de otra forma: en vez de frenar la energía, la enciende y
apaga tan rápido que el motor "ve" un promedio. Casi no se pierde nada en calor.
Por eso todo el mundo lo usa.

## Dónde encaja en este proyecto

Un detalle importante: **la emisora no genera el PWM**. La emisora manda solo
números (ese `pwm_motor` de 0 a 180 y ese `angulo_servo` entre `ANGULO_MIN` y
`ANGULO_MAX`). Quien convierte esos números en pulsos reales es el
[**receptor**](08-EL-RECEPTOR.md), ya dentro del coche.

Y ahora se entiende el juego de números de los modos: el valor del motor va de 0 a
180 siendo **90 el punto muerto**. Por defecto, más de 90 es acelerar hacia
delante (hasta 180) y menos de 90 es frenar o ir marcha atrás (hasta 0). Por eso
"gas a tope" en Sport es 180 y "freno a tope" es 0. Este sentido depende de cómo
esté cableado tu variador y orientado el motor: si el tuyo va al revés, lo
inviertes de golpe con `MOTOR_INVERTIDO` (lo ves en
[Adaptarlo a tu coche](11-ADAPTARLO-A-TU-COCHE.md)).

---

« [Anterior: Lógica de conducción](05-LOGICA-DE-CONDUCCION.md) · [📚 Índice](README.md) · [Siguiente: FreeRTOS y núcleos »](07-FREERTOS-Y-NUCLEOS.md)
