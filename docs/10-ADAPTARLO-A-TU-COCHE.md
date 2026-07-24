# 10. Adaptarlo a tu coche

Ningún coche de RC es igual que otro, así que casi seguro tendrás que ajustar un
par de cosas. Todo lo tocable está en forma de `#define` al principio de
[`main/main.c`](../main/main.c), para no tener que rebuscar en el código.

## La tabla de ajustes

| Parámetro | Qué hace | Valor por defecto |
|-----------|----------|-------------------|
| `PIN_LED` | Pin del LED RGB de la placa | `48` |
| `ANGULO_MIN` / `ANGULO_MAX` | Giro máximo del servo a izquierda / derecha | `65` / `115` |
| `ESC_NEUTRO` | Punto muerto del variador (ni gas ni freno) | `90` |
| `DEAD_ZONE` | Zona muerta del stick de dirección | `4000` |
| `GAS_ECO/NORMAL/SPORT` | Aceleración máxima por modo | `60` / `30` / `0` |
| `FRENO_ECO/NORMAL/SPORT` | Freno máximo por modo | `120` / `150` / `180` |
| `TIEMPO_PARA_CAMBIAR` | Milisegundos con L1+R1 para cambiar de modo | `2000` |

## Problemas típicos y cómo resolverlos

- **El coche gira poco o se pasa de frenada.** Juega con `ANGULO_MIN` y
  `ANGULO_MAX`. Acércalos entre sí para un giro más suave, sepáralos para un giro
  más agresivo. Ve poco a poco: si te pasas, el servo puede forzar la dirección
  contra el tope físico.
- **El coche se desvía solo yendo recto.** Sube un poco `DEAD_ZONE`. Si aun con el
  stick centrado tira hacia un lado, o bien el joystick tiene mucho juego, o bien
  el trapecio de dirección necesita un ajuste mecánico.
- **Va demasiado bruto.** Empieza siempre en modo Eco (es el de por defecto al
  conectar) y baja los límites de gas. Recuerda: menos de 90 es acelerar, así que
  un `GAS_ECO` más cercano a 90 significa menos caña.
- **El motor va al revés o el freno no frena.** Es cosa de la calibración del
  variador. Cambia los valores de `GAS_*` y `FRENO_*`, o recalibra el ESC siguiendo
  su manual (casi todos tienen un modo de calibración de recorrido). Si no tienes
  claro cómo entiende tu variador la señal, repasa la [sección de PWM](06-PWM.md).

## Un método que funciona

Antes de salir a rodar, calibra sobre la mesa:

1. Coche **levantado**, ruedas al aire.
2. Conecta y comprueba que el centro del stick deja la dirección recta. Si no,
   ajusta `DEAD_ZONE` o el centrado mecánico.
3. Prueba los extremos de dirección y ajusta `ANGULO_MIN`/`ANGULO_MAX` hasta que
   el servo llegue justo a los topes sin forzar.
4. Prueba gas y freno en modo Eco, y solo cuando lo tengas fino, sube a Normal y
   Sport.

Con eso tendrás el coche a tu gusto y sin sustos.

---

« [Anterior: Puesta en marcha](09-PUESTA-EN-MARCHA.md) · [📚 Índice](README.md) · [Siguiente: Hoja de ruta »](11-HOJA-DE-RUTA.md)
