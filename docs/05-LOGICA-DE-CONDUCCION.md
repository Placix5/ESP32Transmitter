# 5. Lógica de conducción

Entre leer el mando y mandar el paquete, la emisora hace unas cuantas cuentas para
que el coche se comporte bien. Son tres piezas.

## Zona muerta: que el coche vaya recto

Ningún joystick vuelve al centro perfecto. Siempre se queda en 30, en -50, en un
valor pequeñito que no es cero. Si le hiciéramos caso, el coche iría desviándose
solo aunque tú no toques nada.

La solución es la **zona muerta** (*deadzone*): un margen alrededor del centro que
simplemente ignoramos. En el código son 4000 unidades (`DEAD_ZONE`). Si el stick
está dentro de ese margen, la dirección se queda clavada en el centro (90 grados).
Solo cuando lo empujas más allá empieza a girar de verdad.

## El mapa: de un rango a otro

El stick da valores de -32768 a +32767. El servo entiende ángulos de 65 a 115
grados. Alguien tiene que traducir de una escala a la otra, y de eso se encarga la
función `map_value`, que es una regla de tres de toda la vida:

```c
long map_value(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
```

Le das un valor `x` en su rango original (`in_min`...`in_max`) y te lo devuelve en
el rango de destino (`out_min`...`out_max`). Con esto, el extremo del stick se
convierte en el ángulo máximo del servo, el centro en el punto medio, y todo lo de
en medio, proporcional.

Hay un detalle de seguridad: después de mapear, el resultado pasa por
`constrain_value`, que recorta cualquier valor que se salga de los límites. Así el
servo nunca recibe un ángulo imposible que podría forzar el mecanismo.

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
