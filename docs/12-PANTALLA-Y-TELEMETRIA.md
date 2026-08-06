# 12. Pantalla y telemetría

La emisora lleva una **pantalla OLED** que te enseña en vivo lo que está pasando,
y el coche le **contesta** con datos de vuelta. Las dos cosas van juntas: la
pantalla sin telemetría solo podría enseñarte lo que la emisora ya sabía de sí
misma.

Con un LED de tres colores como única señal, si el coche se quedaba quieto no
había forma de saber si era por falta de señal, por el mando o porque el receptor
estaba apagado. Ahora hay números para distinguirlo.

---

## La pantalla: SSD1306 128x64 por I2C

El bicho es un módulo OLED baratísimo y muy común: **SSD1306 de 128x64 píxeles**,
monocromo, que se conecta por **I2C** (dos cables de datos, dos de corriente).

| Señal | Pin en el ESP32-S3 |
|-------|--------------------|
| SDA (datos) | **GPIO 4** |
| SCL (reloj) | **GPIO 5** |
| VCC | 3,3 V |
| GND | GND |

La dirección I2C del panel es **0x3C** (la habitual; algunos módulos vienen en
0x3D, mira lo que diga el tuyo) y el bus va a **400 kHz**, el modo rápido de I2C.
Todo eso son tres `#define` al principio de [`main/main.c`](../main/main.c):

```c
#define PIN_OLED_SDA 4
#define PIN_OLED_SCL 5
#define OLED_ADDR    0x3C
```

> **Es opcional.** Si no conectas la pantalla, `ssd1306_init()` devuelve `false`,
> avisa por el monitor serie y el resto del programa sigue funcionando
> exactamente igual. El coche se conduce sin pantalla.

### Un driver propio, sin librerías de fuera

Podría haber tirado de cualquiera de las diez librerías que hay por ahí para el
SSD1306, pero preferí escribir un driver mínimo: son unas 250 líneas en
[`main/ssd1306.c`](../main/ssd1306.c) y [`main/ssd1306.h`](../main/ssd1306.h), y
así no arrastro dependencias ni sorpresas de versiones. Para que se compile solo
hay que nombrarlo en [`main/CMakeLists.txt`](../main/CMakeLists.txt):

```cmake
idf_component_register(SRCS "ssd1306.c" "main.c"
                    INCLUDE_DIRS ".")
```

Un detalle importante: el driver usa la **API nueva de I2C de ESP-IDF**
(`driver/i2c_master.h`, disponible desde ESP-IDF 5.2), **no** la antigua
`driver/i2c.h`, que está marcada como obsoleta. Si copias código de tutoriales
viejos te vas a encontrar la vieja por todas partes; la nueva es más limpia (un
"bus" al que le añades "dispositivos") y es la que Espressif mantiene.

La cara pública del driver es corta y se entiende de un vistazo:

| Función | Qué hace |
|---------|----------|
| `ssd1306_init(sda, scl, addr)` | Levanta el bus I2C, sondea el panel y le manda la secuencia de arranque. Devuelve `false` si no responde. |
| `ssd1306_clear()` | Apaga todos los píxeles. |
| `ssd1306_set_pixel(x, y, on)` | Un píxel suelto. |
| `ssd1306_fill_rect(x, y, w, h, on)` | Rectángulo relleno. |
| `ssd1306_draw_rect(x, y, w, h, on)` | Solo el borde. |
| `ssd1306_draw_char(x, y, c)` / `ssd1306_draw_string(x, y, s)` | Texto. |
| `ssd1306_draw_string_inv(x, y, s)` | Texto en **negativo** (ver más abajo). |
| `ssd1306_flush()` | Manda a la pantalla lo dibujado. |

### El buffer: dibujas en RAM, no en la pantalla

Esto es la clave de cómo funcionan casi todas las pantallas pequeñas. **Ninguna
de las funciones de dibujo habla con la pantalla.** Todas escriben en un trozo de
memoria del ESP32 llamado *framebuffer*:

```c
static uint8_t s_buf[SSD1306_WIDTH * SSD1306_HEIGHT / 8];   // 128 * 64 / 8 = 1024 bytes
```

**1 KB exactos.** Y están organizados de una forma que al principio despista: el
SSD1306 no guarda los píxeles por filas, sino en **8 "páginas"** de 8 píxeles de
alto cada una. Así que cada byte del buffer no es un píxel ni una fila: es una
**columna de 8 píxeles verticales**, donde el bit 0 es el de arriba y el bit 7 el
de abajo.

```
byte 0        byte 1        byte 2   ...   byte 127     <- página 0 (filas 0-7)
byte 128      byte 129      ...                         <- página 1 (filas 8-15)
...                                                     <- ... hasta la página 7
```

De ahí sale la cuenta que hace `ssd1306_set_pixel`:

```c
uint16_t idx  = (y / 8) * SSD1306_WIDTH + x;   // qué byte: página * 128 + columna
uint8_t  mask = 1 << (y & 7);                  // qué bit dentro del byte
```

Dibujas todo lo que quieras en ese kilobyte, y solo cuando llamas a
`ssd1306_flush()` se envía de verdad. La ventaja es doble: no ves la pantalla
"construyéndose" a trozos (no hay parpadeo), y ahorras muchísimas conversaciones
por I2C.

### La fuente 5x7 (y por qué no hay minúsculas)

Los caracteres son una tabla en el propio `.c`: para cada letra, **cinco bytes**,
uno por columna, y cada bit encendido es un píxel. La letra ocupa 5 píxeles de
ancho y 7 de alto, y al avanzar dejamos una sexta columna en blanco de
separación, así que **cada carácter come 6 píxeles**. Con 128 de ancho eso da
justo **21 caracteres por línea**, que es el límite que hay que tener en la
cabeza al escribir los textos.

La tabla cubre solo el rango **0x20 a 0x5A** (del espacio a la `Z`): números,
mayúsculas y unos pocos símbolos (`% + - . / : < = > ( )`). No hay minúsculas
para no gastar el doble de memoria en glifos, así que `ssd1306_draw_char` las
convierte a mayúsculas al vuelo:

```c
if (c >= 'a' && c <= 'z') c -= 32;   // minúsculas -> mayúsculas
```

Por eso todos los textos de la pantalla salen en MAYÚSCULAS. Tampoco hay tildes
ni ñ: son caracteres de dos bytes en UTF-8 y se saldrían de la tabla.

### `draw_string_inv`: escribir "en hueco"

`ssd1306_draw_string_inv` hace lo contrario que su hermana: en vez de **encender**
los píxeles de la letra, los **apaga**, y no toca el resto. Por sí sola no se ve
nada (estarías apagando píxeles ya apagados). Su utilidad es escribir **encima de
un rectángulo relleno**, para conseguir texto blanco sobre negro invertido:

```c
ssd1306_fill_rect(0, 45, SSD1306_WIDTH, 11, true);   // banda blanca
ssd1306_draw_string_inv(x, 47, "GUARDADO");          // letras en hueco
```

Se usa para las confirmaciones del [modo configuración](11-ADAPTARLO-A-TU-COCHE.md):
un cartel así se ve de un golpe de vista, sin tener que leer nada.

---

## Volcar solo lo que cambia

La versión ingenua de `flush()` manda el kilobyte entero: le dices al panel
"empieza por el principio" y le sueltas 1024 bytes. Por I2C a 400 kHz, con las
cabeceras de cada byte, eso son **unos 25 milisegundos**. Con ese coste solo daba
para refrescar unas **5 veces por segundo**, y aun así el bus I2C iba cargadito.

Pero pensemos en qué cambia de verdad entre dos refrescos de telemetría: el ping
pasa de 8 a 9 ms, o el porcentaje de gas se mueve. **Una o dos líneas de texto.**
El resto de la pantalla es idéntico. Estábamos reenviando 1000 bytes iguales para
cambiar 20.

La solución es guardar una **copia de lo último enviado** (en el código,
`s_shadow`) y, en cada volcado, comparar página por página:

```c
for (int page = 0; page < SSD1306_HEIGHT / 8; page++) {
    int off = page * SSD1306_WIDTH;
    if (memcmp(&s_buf[off], &s_shadow[off], SSD1306_WIDTH) == 0) continue;  // sin cambios

    // ... posiciona el cursor en esa página y manda sus 128 bytes ...

    memcpy(&s_shadow[off], &s_buf[off], SSD1306_WIDTH);
}
```

Las páginas que no han cambiado **se saltan**. En una pantalla de telemetría
normal cambian una o dos, así que el volcado real pasa de 1024 bytes a **unos
128-256**. Y si no cambia absolutamente nada (el coche quieto, el ping estable),
no se manda **ni un solo byte**.

La pantalla subió de 5 Hz a **~15 Hz** y **a la vez bajó** la carga del bus I2C.
Normalmente ir más rápido cuesta más recursos; aquí, por dejar de hacer trabajo
inútil, salieron ganando las dos cosas.

> **El truco es general.** Cada vez que reenvías periódicamente un bloque de
> datos por un canal lento (I2C, SPI, radio, un puerto serie), pregúntate cuánto
> de ese bloque ha cambiado desde la última vez. Comparar en RAM es
> prácticamente gratis; transmitir, no. Se llama *dirty tracking* y es la misma
> idea que usan las tarjetas gráficas para no redibujar la pantalla entera.

## Por qué la pantalla tiene su propia tarea

La pantalla se refresca en `oled_task`, una tarea aparte:

```c
xTaskCreatePinnedToCore(oled_task, "oled_task", 4096, NULL, 3, NULL, 1);
```

Va **clavada en el núcleo 1**, con **prioridad 3** (por debajo del USB y de la
transmisión, que van a 5) y refresca cada **66 ms**, o sea unas **15 veces por
segundo**.

¿Por qué no meterla en el bucle de `control_tx_task`, que ya gira a 50 Hz? Porque
no cabe. El ciclo de control tiene un presupuesto de **20 ms**, y un volcado
completo del SSD1306 puede llevarse **25**. Si el peor caso de la pantalla se
comiera el ciclo del control, la emisora dejaría de mandar órdenes a ritmo fijo,
que es justo la base del failsafe. Trabajos con exigencias de tiempo distintas,
tareas distintas: el mando y la radio son de tiempo real, la pantalla es
cosmética y puede llegar tarde sin que pase nada.

El reparto completo de tareas entre núcleos está en
[FreeRTOS y núcleos](07-FREERTOS-Y-NUCLEOS.md).

---

## La telemetría: el coche también habla

Con ESP-NOW en unicast sabes si el paquete salió, pero no cómo se ve la cosa
**desde el otro lado**. Por eso el coche contesta: la emisora **sella cada
paquete con la hora** y el coche **le devuelve ese mismo sello**. Comparando lo
que vuelve con la hora actual, la emisora sabe cuánto tardó el viaje de ida y
vuelta, **sin que los dos relojes estén sincronizados** (algo que sería un
problema serio de resolver).

El detalle de las dos estructuras, cómo se sella el tiempo y cómo se calcula el
RTT está contado en [Comunicación ESP-NOW](04-COMUNICACION-ESP-NOW.md). A la
pantalla llegan tres cosas. El **PING** son los milisegundos de ida y vuelta
(`rtt_ms`). La **señal** es el RSSI en dBm que ve **el coche** de nuestros
paquetes (`rssi_coche`), que es lo que de verdad interesa: el enlace se rompe
cuando el coche deja de oírnos, no cuando nosotros dejamos de oírle. Y para saber
**si hay enlace**, se da por bueno que el coche está ahí si llegó telemetría hace
menos de un segundo; si no, la pantalla pone `PING: ---`, `--` en el RSSI y el
icono a cero barras.

### La pantalla normal, línea por línea

```
EMISORA TX             ▂▄▆█ -62
─────────────────────────────────
MODO: SPORT

MANDO: OK

PING: 8 MS

GAS:+45 DIR:-100
```

En la **cabecera** van el título `EMISORA TX` y, arriba a la derecha, el icono de
cobertura con el RSSI en dBm al lado; debajo hay una línea de separación (un
`fill_rect` de 1 píxel de alto).

`MODO:` dice `ECO`, `NORMAL` o `SPORT`, lo mismo que el color del LED pero sin
tener que acordarse de qué color era cuál. `PING:` es la latencia de ida y vuelta
en milisegundos, y `GAS:` y `DIR:` son lo que estás pidiendo ahora mismo, en
porcentaje con signo.

`MANDO:` marca `OK` si el mando está enchufado y enumerado por USB, y `---` si no.
Ojo con la diferencia, porque es fácil confundirse: esa línea habla del **mando**,
no del coche. Puedes tener `MANDO: OK` y `PING: ---` (el mando va, el coche no
contesta) o justo al revés.

### Gas y dirección: porcentajes con signo, centrados en cero

Los números que viajan por la radio (0-180 para el motor, grados para el servo)
son útiles para el receptor pero horribles de leer de un vistazo. ¿Es mucho un
`pwm_motor` de 132? ¿Y un `angulo_servo` de 51? En la pantalla se traducen a algo
que se entiende sin pensar:

El **gas** se mide respecto al neutro (90) y se pasa a porcentaje del recorrido:

```c
int gas_sign = (int)s.pwm_motor - ESC_NEUTRO;      // (al revés si MOTOR_INVERTIDO)
int gas_pct  = gas_sign * 100 / (180 - ESC_NEUTRO);
```

Positivo es **acelerar**, negativo es **frenar**, y el 0 es el punto muerto. Si
tienes `MOTOR_INVERTIDO` activado, la resta se hace al revés, así que en pantalla
el gas sigue saliendo positivo aunque por dentro los números vayan hacia abajo.
Lo que ves es la **intención**, no el número crudo.

La **dirección** se mide respecto al centro ajustado (`anguloCentro`), con un
detalle importante: **cada lado se escala a su propio tope**.

```c
int dir = (int)s.angulo_servo - anguloCentro;
if (dir >= 0) dir_pct = dir * 100 / (anguloMax - anguloCentro);   // derecha
else          dir_pct = dir * 100 / (anguloCentro - anguloMin);   // izquierda
```

Positivo es **derecha**, negativo **izquierda**, 0 es recto. Y como cada mitad se
divide por su propio recorrido, **±100 % significa "el giro máximo de ese lado"**
aunque los topes sean asimétricos. Con el centro en 85, el tope izquierdo en 45 y
el derecho en 115, el stick a la izquierda marca `-100` y a la derecha también
`+100`, pese a que uno recorre 40 grados y el otro 30. Es lo que quieres saber:
"estoy girando todo lo que puede girar por ahí".

### El icono de cobertura y sus umbrales raros

El icono son cuatro barras crecientes, como el de un móvil: 3 píxeles de ancho
cada una con 1 de hueco, y alturas de 2, 4, 6 y 8 píxeles. Las barras vacías no
desaparecen, dejan su **zócalo** de 1 píxel, para que se vea de un vistazo
cuántas faltan.

Lo interesante son los umbrales, en `rssi_a_barras()`:

| RSSI (dBm) | Barras | Lectura |
|------------|--------|---------|
| ≥ −70 | 4 | Enlace fuerte |
| ≥ −82 | 3 | Bien |
| ≥ −92 | 2 | Regular |
| ≥ −100 | 1 | Débil, cerca del borde |
| < −100 | 0 | Perdiéndose |

Si has visto tablas de RSSI para Wi-Fi normal, estos números te van a chocar:
ahí lo típico es considerar "excelente" por encima de −50 dBm y "malo" a partir
de −70. Aquí −70 es la **cobertura llena**. No es un error, y el motivo es
doble:

1. **El RSSI de un ESP32 se estanca por arriba.** Con el coche a un metro de la
   emisora no marca −25 dBm como marcaría un router: se queda plantado sobre
   **−50 / −65 dBm** y de ahí no baja. Culpa del campo cercano y, sobre todo, de
   que la antena es una pista de cobre en la PCB, no una antena decente. Baja de
   verdad **al alejarte**, que es cuando importa.
2. **En Long Range sale todavía más bajo.** La modulación LR reparte la energía
   de otra forma, así que las cifras salen algo más pesimistas que en Wi-Fi
   normal.

Con esos umbrales, **−62 dBm es un enlace excelente** y sale con las cuatro
barras, que es lo honesto. Si hubiera copiado la tabla del móvil, la emisora
marcaría "señal regular" con el coche en la mesa de al lado, y nadie se fiaría
del icono.

> **Una barra no es una emergencia.** En Long Range el receptor sigue
> entendiendo paquetes hasta cerca de **−134 dBm**. Cuando el icono marca una
> barra (−100 dBm) todavía te quedan más de 30 dB de margen, que es muchísimo. El
> icono está calibrado para avisarte **con tiempo**, no para asustarte.

### Qué esperar del PING (y su letra pequeña)

En condiciones normales verás **8-9 ms**, con picos ocasionales a **~20 ms**. Esos
picos no son un fallo: son el enlace fiable haciendo su trabajo. Al ir en unicast,
el receptor confirma cada paquete con un ACK, y cuando un ACK se pierde el
transmisor **reintenta**. Ese reintento se nota como un pico de latencia. Es el
precio de la fiabilidad, y es un buen trato.

Y ahora la parte honesta: **el PING lleva un sesgo de unos 20-30 ms**. El motivo
es que el coche no responde a cada paquete de control, sino que manda telemetría
**unas 5 veces por segundo**. Cuando le llega el momento de contestar, hace eco
del último `t_ms` que recibió, que puede tener ya un rato encima. Así que la
cifra no es "el tiempo de vuelo de un paquete al milisegundo".

¿Para qué sirve entonces? Para lo que de verdad necesitas: **ver tendencias**.
Si el ping se mantiene estable, el enlace está sano. Si empieza a subir y a
bailar, algo va mal: te estás alejando, hay un microondas o un router
saturando el canal, o (esto pasó de verdad, y está contado en
[Leer el mando](03-LEER-EL-MANDO.md)) un motor de vibración atascado está
hundiendo la tensión del USB. Sirve para vigilar cómo evoluciona el enlace, no
para medir un paquete al milisegundo.

### La pantalla de arranque

Nada más encender, antes incluso de levantar el Wi-Fi, la emisora pinta una
pantalla de bienvenida:

```
ESP32 RC
EMISORA  TX


ARRANCANDO...
```

Parece un adorno, pero es el diagnóstico más rápido que tienes: si eso aparece,
la pantalla está bien cableada, la dirección I2C es la correcta y el driver
funciona. Si la pantalla se queda negra, ya sabes que el problema está en el
cable, en la alimentación o en la dirección, y no en el resto del programa.
Un par de segundos después, `oled_task` la sobrescribe con la telemetría en vivo.

### Y la pantalla de configuración

Hay una tercera pantalla, la del **modo configuración**, que sale al mantener el
D-pad hacia abajo dos segundos y sirve para ajustar el trim de la dirección
mirando los números. Como es toda una función en sí misma, está explicada en su
sitio: [Adaptarlo a tu coche](11-ADAPTARLO-A-TU-COCHE.md).

---

« [Anterior: Adaptarlo a tu coche](11-ADAPTARLO-A-TU-COCHE.md) · [📚 Índice](README.md) · [Siguiente: Hoja de ruta »](13-HOJA-DE-RUTA.md)
