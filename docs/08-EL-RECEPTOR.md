# 8. El receptor: el otro lado de la radio

El código del receptor no está en este repositorio (irá en el suyo propio), pero
para entender el sistema completo conviene saber qué hace. Y la buena noticia es
que es **mucho más sencillo** que la emisora. Su vida entera cabe en cuatro pasos:

1. **Escuchar por ESP-NOW.** Al arrancar, el C3 se pone a la espera de paquetes.
   No inicia ninguna conexión; solo escucha.
2. **Desempaquetar los 3 bytes.** Cuando llega un paquete, saca de él los tres
   números: modo, valor del motor y ángulo del servo. Como la estructura está
   `packed` a los dos lados, los bytes encajan exactos.
3. **Generar dos señales PWM.** Con esos números, produce el pulso de 50 Hz que ya
   conoces (ver [PWM](06-PWM.md)): uno para el servo (ángulo de dirección) y otro
   para el variador (potencia del motor).
4. **Vigilar que la emisora siga ahí (fail-safe).** Esto es lo más importante por
   seguridad: si el receptor deja de recibir paquetes durante un tiempo (porque
   apagaste la emisora, se agotó la batería o te fuiste de rango), debe **poner el
   motor en punto muerto** por su cuenta. Un coche de RC que se queda "a tope"
   porque perdió la señal es un problema serio.

> **Nota honesta:** ese fail-safe vive en el receptor, no en la emisora. La emisora
> de ahora, si se desconecta el mando, simplemente **deja de enviar**. Funciona
> porque el receptor cubre esa parte, pero lo suyo sería tener también una red de
> seguridad en la emisora. Está apuntado en la [hoja de ruta](12-HOJA-DE-RUTA.md).

## Por qué el receptor es tan simple

Fíjate en el reparto de trabajo: toda la parte "lista" del sistema —leer el mando,
interpretar XInput, decidir modos, hacer las cuentas— vive en la emisora. Al coche
le llega el trabajo ya hecho, en forma de tres números listos para usar. Por eso el
receptor puede ser un chip pequeño y barato: solo tiene que escuchar y mover dos
salidas.

Es una decisión de diseño a propósito. Cuanto menos tenga que pensar el coche,
menos cosas pueden fallar dentro de él y más fácil es mantenerlo.

---

« [Anterior: FreeRTOS y núcleos](07-FREERTOS-Y-NUCLEOS.md) · [📚 Índice](README.md) · [Siguiente: Requisitos y entorno »](09-ENTORNO-ESP-IDF.md)
