# 5. Lógica de conducción

Entre leer el mando y mandar el paquete, la emisora hace unas cuantas cuentas para
que el coche se comporte bien. Son tres piezas.

## Zona muerta: que el coche vaya recto

Ningún joystick vuelve al centro perfecto. Siempre se queda en 30, en -50, en un
valor pequeñito que no es cero. Si le hiciéramos caso, el coche iría desviándose
solo aunque tú no toques nada.

La solución es la **zona muerta** (*deadzone*): un margen alrededor del centro que
simplemente ignoramos. En el código son 4000 unidades (`DEAD_ZONE`). Si el stick
está dentro de ese margen, la dirección se queda clavada en el centro
(`anguloCentro`). Solo cuando lo empujas más allá empieza a girar de verdad.

## El mapa: de un rango a otro

El stick da valores de -32768 a +32767. El servo entiende ángulos (en el código,
entre `anguloMin` y `anguloMax`). Alguien tiene que traducir de una escala a la
otra, y de eso se encarga la función `map_value`, que es una regla de tres de toda
la vida:

```c
long map_value(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
```

Le das un valor `x` en su rango original (`in_min`...`in_max`) y te lo devuelve en
el rango de destino (`out_min`...`out_max`).

## Mapeo por mitades: cada lado por su cuenta, y trim de "ruedas rectas"

Aquí hay un truco que merece la pena entender. En lugar de mapear el stick entero
de un tirón (de un extremo al otro), lo hacemos **en dos mitades separadas**,
tomando el centro como punto de partida:

- Stick **a la derecha** (fuera de la zona muerta): mapeamos de `anguloCentro`
  hasta `anguloMax`.
- Stick **a la izquierda**: mapeamos de `anguloCentro` hasta `anguloMin`.

¿Por qué complicarlo así? Por dos razones muy prácticas:

1. **Ajustar cada lado de forma independiente.** Muchos chasis no giran lo mismo a
   izquierda que a derecha (las cogidas del bastidor, la geometría de la dirección…).
   Con tres valores separados (`anguloMin`, `anguloCentro`, `anguloMax`) puedes
   afinar cuánto gira a cada lado sin que uno afecte al otro.
2. **El centro es tu trim.** Es el ángulo con el stick en reposo, o sea, tus
   "ruedas rectas". Si el coche se va solo hacia un lado, subes o bajas ese número
   hasta que vaya derecho, y los dos topes siguen igual.

Después de mapear, el resultado pasa por `constrain_value`, que recorta cualquier
valor que se salga de los límites. Así el servo nunca recibe un ángulo imposible
que podría forzar el mecanismo.

### Ojo: estos tres valores se ajustan en caliente

Habrás notado que los escribo en minúscula (`anguloCentro`, `anguloMin`,
`anguloMax`) y no como los `ANGULO_*` en mayúsculas de antes. No es un capricho de
estilo: es que ya **no son constantes del código, son variables**.

Los `#define ANGULO_CENTRO`, `ANGULO_MIN` y `ANGULO_MAX` siguen existiendo en
[`main/main.c`](../main/main.c), pero ahora solo hacen de **valor de fábrica**: lo
que se usa si nunca has guardado nada. En marcha, el mapeo trabaja con las
variables, y esas se ajustan **con el mando, en el coche, sin recompilar nada**,
desde el **modo configuración**, y se guardan en la memoria de la emisora para el
siguiente arranque.

Cómo entrar en ese modo, qué botón hace qué y el método para calibrar sobre la
mesa lo tienes en [Adaptarlo a tu coche](11-ADAPTARLO-A-TU-COCHE.md).

## Los tres modos de conducción

No es lo mismo dejar el coche a un crío que salir a correr. Por eso hay tres modos,
y lo único que cambia entre ellos es **cuánta aceleración y freno máximos
permiten**:

| Modo | Color LED | Gas máx. | Freno máx. |
|------|-----------|----------|------------|
| Eco | 🟢 Verde | suave (120) | suave (60) |
| Normal | 🔵 Azul | medio (150) | medio (30) |
| Sport | 🔴 Rojo | a tope (180) | a tope (0) |

Cambias de modo manteniendo **L1 + R1** dos segundos. Cuando lo detecta, la
emisora hace vibrar el mando y cambia el color del LED para confirmártelo, y rota
Eco → Normal → Sport → Eco.

Fíjate en el patrón: el "gas" sube (120 → 180) cuanto más agresivo es el modo, y
el "freno" baja (60 → 0). Ambos se miden respecto al **neutro (90)**: el gas se va
por encima (hacia 180) y el freno por debajo (hacia 0). Cuanto más lejos del 90,
más fuerza. Cómo un número acaba moviendo el motor lo tienes en la
[sección de PWM](06-PWM.md).

Este sentido depende de cómo esté **cableado el ESC / orientado el motor**. Si en
tu coche el gatillo de gas frena y el de freno acelera, no tienes que reescribir
estos seis números a mano: descomenta `MOTOR_INVERTIDO` al principio de
[`main/main.c`](../main/main.c) y se intercambian los dos juegos de valores
automáticamente. (El receptor tiene un ajuste hermano, `THROTTLE_INVERTED`, que
debe ir coordinado para que la barra de LEDs del coche se encienda al acelerar.)

---

« [Anterior: Comunicación ESP-NOW](04-COMUNICACION-ESP-NOW.md) · [📚 Índice](README.md) · [Siguiente: PWM »](06-PWM.md)
