# 11. Adaptarlo a tu coche

Ningún coche de RC es igual que otro, así que casi seguro tendrás que ajustar un
par de cosas. Todo lo tocable está en forma de `#define` al principio de
[`main/main.c`](../main/main.c), para no tener que rebuscar en el código.

## La tabla de ajustes

| Parámetro | Qué hace | Valor por defecto |
|-----------|----------|-------------------|
| `PIN_LED` | Pin del LED RGB de la placa | `48` |
| `ANGULO_MIN` / `ANGULO_MAX` | Giro máximo del servo a izquierda / derecha | `45` / `115` |
| `ANGULO_CENTRO` | Ruedas rectas (*trim*): el ángulo con el stick en reposo | `85` |
| `ESC_NEUTRO` | Punto muerto del variador (ni gas ni freno) | `90` |
| `DEAD_ZONE` | Zona muerta del stick de dirección | `4000` |
| `GAS_ECO/NORMAL/SPORT` | Aceleración máxima por modo | `120` / `150` / `180` |
| `FRENO_ECO/NORMAL/SPORT` | Freno máximo por modo | `60` / `30` / `0` |
| `MOTOR_INVERTIDO` | Intercambia gas/freno si el motor va al revés | comentado (desactivado) |
| `TIEMPO_PARA_CAMBIAR` | Milisegundos con L1+R1 para cambiar de modo | `2000` |

## Problemas típicos y cómo resolverlos

- **El coche gira poco o se pasa de frenada.** Juega con `ANGULO_MIN` y
  `ANGULO_MAX`, que son **independientes**: cada uno controla un lado. Si gira más a
  un lado que al otro (típico por las cogidas del chasis), ajusta solo el tope de ese
  lado. Ve poco a poco: si te pasas, el servo puede forzar la dirección contra el
  tope físico.
- **El coche se desvía solo yendo recto.** Lo primero, ajusta el *trim*: sube o baja
  `ANGULO_CENTRO` hasta que con el stick en reposo las ruedas queden rectas. Si aun
  así tira, sube un poco `DEAD_ZONE`; y si sigue, o el joystick tiene mucho juego o
  el trapecio de dirección necesita un ajuste mecánico.
- **Va demasiado bruto.** Empieza siempre en modo Eco (es el de por defecto al
  conectar) y baja los límites de gas. Recuerda: por defecto, más de 90 es acelerar,
  así que un `GAS_ECO` más cercano a 90 significa menos caña.
- **El motor va al revés (el gas frena y el freno acelera).** Depende de cómo esté
  cableado el ESC u orientado el motor. La forma limpia de arreglarlo es descomentar
  `MOTOR_INVERTIDO` al principio de [`main/main.c`](../main/main.c): intercambia los
  juegos de `GAS_*` y `FRENO_*` de golpe, sin tocarlos uno a uno. Si al hacerlo la
  barra de LEDs del coche se enciende al frenar, cuadra también `THROTTLE_INVERTED`
  en el receptor. Como alternativa, muchos ESC tienen su propio modo de calibración
  de recorrido (mira su manual).

## Un método que funciona

Antes de salir a rodar, calibra sobre la mesa:

1. Coche **levantado**, ruedas al aire.
2. Conecta y comprueba que el centro del stick deja la dirección recta. Si no,
   ajusta `ANGULO_CENTRO` (el *trim*) y, si hace falta, `DEAD_ZONE` o el centrado
   mecánico.
3. Prueba los extremos de dirección y ajusta `ANGULO_MIN`/`ANGULO_MAX` (cada lado
   por separado) hasta que el servo llegue justo a los topes sin forzar.
4. Prueba gas y freno en modo Eco, y solo cuando lo tengas fino, sube a Normal y
   Sport.

Con eso tendrás el coche a tu gusto y sin sustos.

---

« [Anterior: Puesta en marcha](10-PUESTA-EN-MARCHA.md) · [📚 Índice](README.md) · [Siguiente: Hoja de ruta »](12-HOJA-DE-RUTA.md)
