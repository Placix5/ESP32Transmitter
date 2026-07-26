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
(`ANGULO_CENTRO`). Solo cuando lo empujas más allá empieza a girar de verdad.

## El mapa: de un rango a otro

El stick da valores de -32768 a +32767. El servo entiende ángulos (en el código,
entre `ANGULO_MIN` y `ANGULO_MAX`). Alguien tiene que traducir de una escala a la
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
tomando `ANGULO_CENTRO` como punto de partida:

- Stick **a la derecha** (fuera de la zona muerta): mapeamos de `ANGULO_CENTRO`
  hasta `ANGULO_MAX`.
- Stick **a la izquierda**: mapeamos de `ANGULO_CENTRO` hasta `ANGULO_MIN`.

¿Por qué complicarlo así? Por dos razones muy prácticas:

1. **Ajustar cada lado de forma independiente.** Muchos chasis no giran lo mismo a
   izquierda que a derecha (las cogidas del bastidor, la geometría de la dirección…).
   Con tres valores separados —`ANGULO_MIN`, `ANGULO_CENTRO`, `ANGULO_MAX`— puedes
   afinar cuánto gira a cada lado sin que uno afecte al otro.
2. **`ANGULO_CENTRO` es tu trim.** Es el ángulo con el stick en reposo, o sea, tus
   "ruedas rectas". Si el coche se va solo hacia un lado, subes o bajas ese número
   hasta que vaya derecho, y los dos topes siguen igual.

Después de mapear, el resultado pasa por `constrain_value`, que recorta cualquier
valor que se salga de los límites. Así el servo nunca recibe un ángulo imposible
que podría forzar el mecanismo. Cómo dejar estos tres valores a tu gusto lo tienes
en [Adaptarlo a tu coche](11-ADAPTARLO-A-TU-COCHE.md).

## Los tres modos de conducción

No es lo mismo dejar el coche a un crío que salir a correr. Por eso hay tres modos,
y lo único que cambia entre ellos es **cuánta aceleración y freno máximos
permiten**:

| Modo | Color LED | Gas máx. | Freno máx. |
|------|-----------|----------|------------|
| Eco | 🟢 Verde | suave (60) | suave (120) |
| Normal | 🔵 Azul | medio (30) | medio (150) |
| Sport | 🔴 Rojo | a tope (0) | a tope (180) |

Cambias de modo manteniendo **L1 + R1** dos segundos. Cuando lo detecta, la
emisora hace vibrar el mando y cambia el color del LED para confirmártelo, y rota
Eco → Normal → Sport → Eco.

Si te fijas en la tabla, los números de "gas" bajan cuanto más agresivo es el
modo, y los de "freno" suben. Eso no es un error; tiene que ver con cómo el
variador entiende la señal, y lo explico en la [sección de PWM](06-PWM.md).

---

« [Anterior: Comunicación ESP-NOW](04-COMUNICACION-ESP-NOW.md) · [📚 Índice](README.md) · [Siguiente: PWM »](06-PWM.md)
