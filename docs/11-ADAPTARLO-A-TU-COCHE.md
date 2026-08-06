# 11. Adaptarlo a tu coche

Ningún coche de RC es igual que otro, así que casi seguro tendrás que ajustar un
par de cosas. La buena noticia es que **lo que más se ajusta ya no se toca en el
código**: el centro y los topes de la dirección se afinan con el mando, en el
coche, mirando la pantalla, y se guardan en la memoria de la emisora.

Los `#define` del principio de [`main/main.c`](../main/main.c) siguen ahí, pero han
cambiado de papel: ahora son los **valores de partida** (los de "fábrica"), no la
única forma de cambiarlos.

---

## El modo configuración: ajustar la dirección sin recompilar

Esta es la función estrella. Antes, para mover el trim un grado había que abrir el
editor, cambiar un número, compilar, flashear y volver a probar. Cada ajuste,
varios minutos. Ahora se hace en el sitio, en segundos.

### Entrar y salir

Mantén el **D-pad hacia ABAJO durante 2 segundos**. Notarás tres cosas a la vez:

- el mando **vibra** un pulso,
- el **LED de la emisora pasa a blanco parpadeante** (se enciende y apaga cada
  400 ms), para que no haya duda de que estás en modo ajuste,
- la **pantalla cambia** al menú de configuración.

Para salir, exactamente lo mismo: D-pad ABAJO otros 2 segundos. El mando vibra
más flojo y el **LED vuelve al color del modo** en el que ibas (🟢 Eco,
🔵 Normal, 🔴 Sport).

### Lo que ves en la pantalla

```
CONFIG DIR
─────────────────────────────────
CENTRO: 85
IZQ (LB): 45
DER (RB): 115

A=GUARDAR B=DESHACER
X=POR DEFECTO
```

Los tres números son los que estás editando, en grados de servo, y se actualizan
al instante. Abajo, la chuleta de botones... salvo cuando acabas de pulsar algo:
entonces sale una **confirmación en negativo** (banda rellena con el texto en
hueco) durante **un segundo y medio**, y luego vuelve la chuleta.

| Mensaje | Qué acabas de hacer |
|---------|---------------------|
| `GUARDADO` | Los valores están escritos en la memoria de la emisora. |
| `POR DEFECTO` | Se han cargado los valores de fábrica (los `#define`). |
| `DESHECHO` | Se ha vuelto a lo último que guardaste. |

### La tabla de controles

| Botones | Qué ajusta |
|---------|-----------|
| **D-pad ← / →** | **Centro** (`anguloCentro`): el trim de "ruedas rectas". |
| **LB + ← / →** | **Tope izquierdo** (`anguloMin`): cuánto gira como máximo a la izquierda. |
| **RB + ← / →** | **Tope derecho** (`anguloMax`): cuánto gira como máximo a la derecha. |
| **A** | **Guardar** en la memoria de la emisora (permanente). |
| **B** | **Deshacer**: vuelve a lo último guardado. |
| **X** | **Por defecto**: carga los valores de fábrica del código. |

Cada pulsación mueve el valor **un grado**, con un tic corto de vibración para que
lo notes sin mirar. Y sí, es "un grado por pulsación" a propósito: la emisora
detecta el **flanco** de la pulsación (el momento en que el botón pasa de suelto a
pulsado), así que mantener el D-pad apretado no encadena pasos. Para diez grados,
diez pulsaciones. Es lento a propósito: el trim se ajusta de grado en grado, no a
puñados.

### El servo te enseña dónde estás

Aquí está el detalle que hace que esto sea cómodo de verdad: **el servo se mueve
al ángulo que estás editando**. En modo configuración se ignora el stick y la
dirección va a:

- **sin bumpers** → al **centro**,
- **manteniendo LB** → al **tope izquierdo**,
- **manteniendo RB** → al **tope derecho**.

Así que no ajustas números a ciegas. Aprietas LB y las ruedas se van al tope
izquierdo; le das a ← o → y las ves moverse grado a grado hasta donde tú quieras;
sueltas LB y vuelven al centro. Lo ves con los ojos, no lo calculas.

### Seguridad: el gas se queda a cero

Dos decisiones de seguridad que conviene entender, porque no son un descuido:

**Dentro de config, el gas y el freno se fuerzan a NEUTRO.** Estás con las manos
en el coche, tocando las ruedas, mirando la dirección. Que un roce en el gatillo
lo pusiera en marcha sería una forma tonta de hacerse daño (o de romper algo). La
**dirección sí sigue viva**, porque es justo lo que necesitas ver.

**Fuera de config, el D-pad no hace absolutamente nada.** Podría haber dejado el
trim ajustable en marcha, pero un manotazo involuntario a la cruceta mientras
conduces te descolocaría la dirección en el peor momento posible. Si quieres
ajustar, entras en config; y para entrar hay que mantener dos segundos, que no se
hace por accidente.

---

## Dónde se guarda todo eso: la NVS

Cuando pulsas **A**, los tres valores se escriben en la **NVS**. Vale la pena
explicar qué es, porque es una de las herramientas más útiles del ESP32 y se usa
en todas partes.

**NVS** significa *Non-Volatile Storage*, almacenamiento no volátil. Es una
**partición aparte de la memoria flash** del chip, separada de donde vive tu
programa, organizada como un almacén de **clave-valor**: guardas "centro = 85" y
lo recuperas por su nombre. Sobrevive a apagones, a desenchufar la placa y a
reflashear el programa (mientras no borres la partición).

En el código son tres claves dentro de un *namespace* llamado `trim`:

| Namespace | Clave | Tipo | Contenido |
|-----------|-------|------|-----------|
| `trim` | `centro` | `int16` | `anguloCentro` |
| `trim` | `min` | `int16` | `anguloMin` |
| `trim` | `max` | `int16` | `anguloMax` |

Al arrancar, `cargar_trim()` intenta leer las tres. **Si están, se usan; si no
hay nada guardado, se quedan los `#define`.** Por eso un proyecto recién
flasheado en una placa nueva arranca con los valores del código, y a partir de la
primera vez que guardas, arranca con los tuyos.

### ¿Por qué hay que pulsar A? ¿No podría guardarse solo?

Podría, pero sería una mala idea, y el motivo es físico: **la memoria flash se
desgasta al escribir**. Cada celda aguanta un número finito de ciclos de
borrado/escritura (del orden de decenas de miles). Si guardásemos en cada
pulsación del D-pad, una sesión de calibración de cinco minutos podría meter
cientos de escrituras para nada, porque solo la última importa.

Así que el guardado es **manual y explícito**: tocas los números todo lo que
quieras (eso es gratis, son variables en RAM) y escribes en flash **una vez**,
cuando ya lo tienes.

> **Dato tranquilizador:** la NVS de ESP-IDF hace *wear-leveling*, o sea, va
> repartiendo las escrituras por distintas zonas de la partición en vez de
> machacar siempre la misma celda. No es que la flash sea de cristal; es que no
> hay ninguna razón para gastarla a lo tonto.

### La instantánea de lo confirmado

Junto a los valores que estás editando, la emisora guarda en RAM una copia de los
**valores confirmados**: `savedCentro`, `savedMin` y `savedMax`. Son lo último que
guardaste con **A** o, si no has guardado nada en esta sesión, lo que se cargó al
arrancar.

Esa copia es lo que hace que **B (deshacer)** sea instantáneo y no tenga que
volver a leer la flash. Y se actualiza justo en el momento de guardar: en cuanto
pulsas **A**, lo que acabas de guardar pasa a ser el nuevo "punto de retorno".

### El matiz importante de B y X

**`B` y `X` solo cambian los valores en memoria.** Ni deshacer ni cargar los
valores de fábrica escriben nada en la NVS. Si pulsas **X**, pruebas los valores
de fábrica, no te gustan y sales de config sin más, al siguiente arranque la
emisora volverá a cargar de la NVS **lo que tenías guardado**, no los de fábrica.

Es a propósito: puedes **probar sin comprometerte**. Para que un cambio persista,
siempre hay que pasar por **A**.

### `sanea_trim()`: que los números no se puedan liar

Cada vez que se toca un valor (al ajustar, al deshacer, al cargar por defecto o
al leer de la NVS) se llama a `sanea_trim()`, que impone dos reglas:

- **`anguloMin ≤ anguloCentro ≤ anguloMax`.** No tiene sentido un tope izquierdo
  que esté a la derecha del centro. Si empujas un valor más allá del otro, el otro
  se lo lleva por delante.
- **Todo dentro del rango del servo**: el centro se queda entre 20 y 160, y los
  topes entre 0 y 180.

Es un cinturón de seguridad para que ninguna combinación de pulsaciones (ni un
valor raro leído de la flash) pueda mandar al servo a un ángulo imposible y
forzar el mecanismo.

---

## El método de calibración, sobre la mesa

Media hora bien invertida. Coche **levantado**, con las ruedas al aire.

1. **Enciende todo** y enchufa el mando. Comprueba en la pantalla que sale
   `MANDO: OK` y que el `PING` responde: si el coche no contesta, poco vas a
   calibrar.
2. **Entra en config**: D-pad ABAJO, 2 segundos. LED blanco parpadeando.
3. **El centro primero.** Sin tocar bumpers, el servo está en `anguloCentro`.
   Mira las ruedas de frente y dale a **←** o **→** hasta que queden **rectas**.
   Ese es tu trim.
4. **El tope izquierdo.** Mantén **LB**: el servo se va a `anguloMin`. Con LB
   apretado, ve pulsando **←** para girar más, o **→** para girar menos, hasta que
   la rueda llegue **justo antes de tocar el tope mecánico**. Ni un grado más: si
   el servo empuja contra el tope, calienta, consume y se puede cargar la
   dirección.
5. **El tope derecho.** Lo mismo con **RB** y `anguloMax`.
6. **Vuelve a comprobar el centro** (suelta los bumpers). Los topes no lo mueven,
   pero un repaso no cuesta nada.
7. **Guarda con A.** Debe salir `GUARDADO` en la pantalla y un pulso fuerte de
   vibración.
8. **Sal de config** (D-pad ABAJO 2 s) y **prueba a rodar** despacio, en modo Eco.
   Comprueba en la pantalla que `DIR:` marca `+100` y `-100` con el stick a cada
   lado, y `0` en reposo.
9. Si algo no te cuadra, vuelve a entrar y afina. Y **reinicia la emisora** una
   vez, para confirmar que lo guardado se recupera de verdad al arrancar.

Solo cuando la dirección esté fina, sube a Normal y a Sport.

> Si te lías con los números y quieres empezar de cero: **X** (valores de
> fábrica) y luego **A** para dejarlos guardados. Vuelta a la casilla de salida.

---

## La tabla de ajustes del código

Lo que sigue tocándose recompilando. El trim ya no hace falta cambiarlo aquí
(úsalo solo si quieres cambiar los valores de **fábrica** con los que arranca una
placa nueva).

| Parámetro | Qué hace | Valor por defecto |
|-----------|----------|-------------------|
| `PIN_LED` | Pin del LED RGB de la placa | `48` |
| `PIN_OLED_SDA` | Pin de datos I2C de la pantalla | `4` |
| `PIN_OLED_SCL` | Pin de reloj I2C de la pantalla | `5` |
| `OLED_ADDR` | Dirección I2C del panel SSD1306 | `0x3C` |
| `ANGULO_MIN` / `ANGULO_MAX` | Giro máximo a izquierda / derecha **de fábrica** | `45` / `115` |
| `ANGULO_CENTRO` | Ruedas rectas (*trim*) **de fábrica** | `85` |
| `ESC_NEUTRO` | Punto muerto del variador (ni gas ni freno) | `90` |
| `DEAD_ZONE` | Zona muerta del stick de dirección | `4000` |
| `GAS_ECO/NORMAL/SPORT` | Aceleración máxima por modo | `120` / `150` / `180` |
| `FRENO_ECO/NORMAL/SPORT` | Freno máximo por modo | `60` / `30` / `0` |
| `MOTOR_INVERTIDO` | Intercambia gas/freno si el motor va al revés | comentado (desactivado) |
| `TIEMPO_PARA_CAMBIAR` | Milisegundos con L1+R1 para cambiar de modo | `2000` |
| `client_mac` | MAC Wi-Fi STA del receptor (no es un `#define`, es un array) | la de tu C3 |

> Los tres `ANGULO_*` marcados como "de fábrica" son los que se usan **solo si la
> NVS está vacía** o si pulsas **X** en config. En marcha, quien manda son
> `anguloCentro`, `anguloMin` y `anguloMax`.

## Problemas típicos y cómo resolverlos

- **El coche se desvía solo yendo recto.** Entra en config y ajusta el **centro**
  con el D-pad hasta que las ruedas queden rectas. Guarda con **A**. Si aun así
  tira, sube un poco `DEAD_ZONE` (eso sí hay que recompilarlo); y si sigue, o el
  joystick tiene mucho juego o el trapecio de dirección necesita un ajuste
  mecánico.
- **El coche gira poco, o el servo hace ruido al final del recorrido.** Ajusta los
  **topes** en config: `LB + ←/→` para el izquierdo y `RB + ←/→` para el derecho.
  Son independientes, así que si gira más a un lado que al otro (muy típico por
  las cogidas del chasis) ajustas solo ese lado. Si el servo zumba o calienta al
  llegar al tope, te has pasado: recórtalo un par de grados.
- **Guardo el trim y al reiniciar se pierde.** Comprueba que salió el mensaje
  `GUARDADO` en la pantalla. Si sales de config sin pulsar **A**, los cambios se
  quedan solo en RAM. Y si nada se guarda nunca, mira en el monitor serie si la
  NVS se está inicializando bien (el programa la borra y la recrea si detecta que
  está corrupta).
- **Va demasiado bruto.** Empieza siempre en modo Eco (es el de por defecto al
  conectar) y baja los límites de gas. Recuerda: por defecto, más de 90 es
  acelerar, así que un `GAS_ECO` más cercano a 90 significa menos caña.
- **El motor va al revés (el gas frena y el freno acelera).** Depende de cómo esté
  cableado el ESC u orientado el motor. La forma limpia de arreglarlo es
  descomentar `MOTOR_INVERTIDO` al principio de
  [`main/main.c`](../main/main.c): intercambia los juegos de `GAS_*` y `FRENO_*`
  de golpe, sin tocarlos uno a uno. La pantalla se entera y sigue mostrando el gas
  como positivo. Si al hacerlo la barra de LEDs del coche se enciende al frenar,
  cuadra también `THROTTLE_INVERTED` en el receptor. Como alternativa, muchos ESC
  tienen su propio modo de calibración de recorrido (mira su manual).
- **La pantalla no se enciende.** Revisa VCC, GND, y que SDA vaya a GPIO 4 y SCL a
  GPIO 5. Si el módulo es de los que vienen en `0x3D`, cambia `OLED_ADDR`. El
  monitor serie te lo dice claro: el driver avisa si el panel no responde en la
  dirección configurada. Más detalles en
  [Pantalla y telemetría](12-PANTALLA-Y-TELEMETRIA.md).

---

« [Anterior: Puesta en marcha](10-PUESTA-EN-MARCHA.md) · [📚 Índice](README.md) · [Siguiente: Pantalla y telemetría »](12-PANTALLA-Y-TELEMETRIA.md)
