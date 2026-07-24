# 10. Puesta en marcha

Con el [entorno ya instalado](09-ENTORNO-ESP-IDF.md), esto es lo divertido:
compilar el proyecto, meterlo en la placa y salir a conducir.

Te lo cuento con la **extensión de VSCode** (a base de botones, lo más cómodo para
empezar) y, al final, la alternativa por **línea de comandos** por si la prefieres.

## Paso 1 · Consigue el proyecto

Clónalo con Git:

```bash
git clone https://github.com/Placix5/ESP32Transmitter.git
```

Si no te manejas con Git, en la página de GitHub puedes pulsar **Code → Download
ZIP** y descomprimirlo donde quieras.

No hace falta `--recursive` ni nada raro: las librerías (TinyUSB y el driver
XInput) vienen incluidas dentro de `components/`, ya listas para compilar.

## Paso 2 · Abre el proyecto en VSCode

**File → Open Folder** y elige la carpeta `ESP32Transmitter` que acabas de
descargar. VSCode reconocerá que es un proyecto ESP-IDF (tiene su `CMakeLists.txt`
en la raíz).

## Paso 3 · Elige el chip y el puerto

Todo esto se hace desde la **barra de estado azul** de abajo, o desde la paleta de
comandos (`Ctrl+Shift+P`) si prefieres buscar por nombre:

1. **El chip.** Pulsa el icono del chip en la barra de abajo, o el comando
   **`ESP-IDF: Set Espressif Device Target`**, y elige **`esp32s3`**.
2. **El puerto.** Enchufa el ESP32-S3 al ordenador por USB. Pulsa el icono del
   enchufe, o el comando **`ESP-IDF: Select Port to Use`**, y elige el puerto que
   aparezca (`COM4` en Windows, `/dev/ttyUSB0` o `/dev/ttyACM0` en Linux).

> Si no aparece ningún puerto, suele ser el cable (hay cables de solo carga que no
> llevan datos) o que falta el driver USB-serie de la placa. Prueba otro cable
> antes que nada.

## Paso 4 · Compila, flashea y monitoriza

La forma fácil es el botón **🔥 (la llama)** de la barra de abajo, que es el comando
**`ESP-IDF: Build, Flash and Start a Monitor on Your Device`**. Hace las tres cosas
de un tirón: compila el proyecto, lo graba en la placa y abre el monitor para ver
los mensajes.

La **primera compilación tarda un buen rato** (tiene que construir medio ESP-IDF);
las siguientes son mucho más rápidas.

Si prefieres ir paso a paso, en la misma barra tienes los botones sueltos:

- **Compilar** (el cilindro) → `ESP-IDF: Build your Project`
- **Flashear** (el rayo) → `ESP-IDF: Flash Your Project`
- **Monitor** (la pantallita) → `ESP-IDF: Monitor Device`

Cuando el monitor esté abierto, se cierra con `Ctrl+]`.

## Alternativa: por línea de comandos

Si te apañas mejor con la terminal, abre un **ESP-IDF Terminal** (la extensión
tiene uno, o usa el "ESP-IDF Command Prompt" que instaló el asistente) y desde la
carpeta del proyecto:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PUERTO flash monitor
```

Sustituye `PUERTO` por tu puerto serie. El `monitor` del final te deja ver los
mensajes de la placa; sal con `Ctrl+]`.

## Cómo se conduce

1. Enciende el coche (con el receptor C3 y el variador ya montados).
2. Enciende la emisora (el ESP32-S3).
3. Enchufa el mando de Xbox al S3 por USB. El LED se pondrá **verde**: modo Eco,
   listo para conducir.
4. Conduce:
   - **Stick izquierdo (horizontal):** dirección.
   - **Gatillo derecho (RT):** acelerar.
   - **Gatillo izquierdo (LT):** frenar / marcha atrás.
5. Para cambiar de modo, mantén **L1 + R1** dos segundos. El mando vibrará y el
   LED cambiará de color:
   - 🟢 Verde = **Eco** (suave, para empezar)
   - 🔵 Azul = **Normal**
   - 🔴 Rojo = **Sport** (sin límites)

> Consejo: haz las primeras pruebas con el coche **levantado, sin que las ruedas
> toquen el suelo**. Te ahorras más de un susto.

---

« [Anterior: Requisitos y entorno](09-ENTORNO-ESP-IDF.md) · [📚 Índice](README.md) · [Siguiente: Adaptarlo a tu coche »](11-ADAPTARLO-A-TU-COCHE.md)
