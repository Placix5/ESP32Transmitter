# 9. Requisitos y entorno

Antes de poder compilar y meter el programa en la placa necesitas dos cosas: el
**hardware** y un **entorno de desarrollo** funcionando en el ordenador. El
hardware es fácil de reunir; el entorno es la parte que más echa para atrás a
quien empieza, así que vamos con calma. La buena noticia es que se instala **una
sola vez** y luego te olvidas.

## Lo que necesitas tener a mano (hardware)

### La emisora (este repositorio)

| Pieza | Ejemplo / nota |
|-------|----------------|
| Placa ESP32-**S3** | Cualquier ESP32-S3-DevKitC o similar. **Tiene que ser un S3**: es el que puede hacer de USB Host. |
| Mando de Xbox | Xbox One o Series, con **cable USB**. |
| Cable USB | Para enchufar el mando al S3 (OTG si tu placa lo pide). |
| LED RGB | El integrado de la placa (pin 48 en la mayoría de DevKitC). |

### El coche (receptor, repositorio aparte)

| Pieza | Ejemplo / nota |
|-------|----------------|
| Placa ESP32-**C3** | Pequeña y barata, va dentro del coche. |
| Variador (ESC) | HobbyWing QuicRun 1060, o cualquier ESC estándar de 50 Hz. |
| Servo de dirección | MG996R, o cualquier servo estándar. |
| Coche RC | El chasis que ya tengas. |

> **Aviso importante de alimentación:** el servo se alimenta desde la salida del
> variador (el BEC del ESC), **nunca** desde el pin de 5 V del ESP32. Un servo como
> el MG996R pide mucha corriente en los picos y te reiniciaría la placa.

## El entorno: qué vamos a instalar

Este proyecto está hecho con **ESP-IDF**, que es el kit de desarrollo oficial de
Espressif (los fabricantes del ESP32). No es Arduino; es la herramienta "seria"
con la que Espressif espera que programes sus chips, y es la que da acceso a cosas
como el USB Host que aquí necesitamos.

ESP-IDF por su cuenta se maneja desde la terminal, lo cual asusta un poco. Por eso
vamos a instalarlo a través de la **extensión de ESP-IDF para Visual Studio Code**,
que te pone botones para compilar, flashear y ver los mensajes de la placa sin
escribir un solo comando. La extensión, además, se encarga de descargar e instalar
todo ESP-IDF por ti.

Lo montamos en cuatro pasos.

### Paso 1 · Instala Visual Studio Code

Descárgalo de [code.visualstudio.com](https://code.visualstudio.com/) e instálalo
con las opciones por defecto. Está para Windows, macOS y Linux.

### Paso 2 · Instala la extensión ESP-IDF

1. Abre VSCode.
2. Ve al panel de extensiones: icono de los cuadraditos en la barra lateral, o
   pulsa `Ctrl+Shift+X` (`Cmd+Shift+X` en Mac).
3. Busca **ESP-IDF**. La que quieres es la de **Espressif Systems** (fíjate en el
   editor, para no instalar una copia).
4. Pulsa **Install**.

### Paso 3 · Instala ESP-IDF con el asistente

Ahora la extensión va a descargar el ESP-IDF de verdad (son varios GB, así que ten
paciencia y buena conexión).

1. Abre la paleta de comandos con `Ctrl+Shift+P` (`Cmd+Shift+P` en Mac). Es una
   barra de búsqueda de acciones que usaremos mucho.
2. Escribe y elige **`ESP-IDF: Configure ESP-IDF Extension`** (en versiones
   recientes puede llamarse **`ESP-IDF: Open ESP-IDF Installation Manager`**).
3. Elige la opción **EXPRESS** (la instalación exprés, la más sencilla).
4. Cuando te pida la versión de ESP-IDF, elige una **5.x** reciente (por ejemplo la
   última estable que te ofrezca).
5. Deja las rutas que propone por defecto y dale a instalar.

A partir de ahí, la extensión descarga ESP-IDF y todas sus herramientas (el
compilador, etc.). En **Windows** se ocupa también de instalar por su cuenta
Python y Git si no los tienes. En **macOS y Linux** necesitas tener ya instalados
Python 3 y Git antes de empezar.

Cuando termine, te avisará. Es normal que tarde un buen rato la primera vez.

> **Consejo que ahorra dolores de cabeza:** deja que se instale en una ruta
> **corta y sin espacios, tildes ni ñ** (por ejemplo `C:\esp` en Windows). ESP-IDF
> es muy tiquismiquis con las rutas raras, y muchos fallos misteriosos de
> instalación vienen de ahí.

### Paso 4 · Comprueba que funciona

Para asegurarte de que todo quedó bien:

1. Paleta de comandos (`Ctrl+Shift+P`) → **`ESP-IDF: Doctor Command`**.
2. Te generará un informe. Si está todo en verde / sin errores, ya lo tienes.

Sabrás también que funciona porque abajo del todo, en la **barra de estado azul**
de VSCode, aparecerá una fila de iconos nuevos de ESP-IDF (un enchufe para el
puerto, el nombre del chip, un cilindro para compilar, un rayo para flashear...).
Los usaremos en la [siguiente sección](10-PUESTA-EN-MARCHA.md).

## Si algo se atasca en la instalación

- **Se queda parado o falla al descargar.** Casi siempre es la conexión, un
  antivirus o un proxy cortando la descarga. Vuelve a lanzar el asistente; retoma
  donde estaba. Si estás en una red muy restrictiva, el instalador ofrece un
  espejo de descarga alternativo.
- **El Doctor se queja de Python o Git.** Vuelve a ejecutar el asistente del
  Paso 3; deja que instale lo que falte (o instálalos tú a mano en macOS/Linux).
- **Rutas con espacios o tildes.** Reinstala ESP-IDF en una ruta limpia como
  `C:\esp`. Es la causa número uno de fallos raros.
- **Falta espacio en disco.** ESP-IDF ocupa varios GB entre el framework y las
  herramientas; asegúrate de tener sitio de sobra.

Con el entorno listo, ya podemos compilar el proyecto y meterlo en la placa.

---

« [Anterior: El receptor](08-EL-RECEPTOR.md) · [📚 Índice](README.md) · [Siguiente: Puesta en marcha »](10-PUESTA-EN-MARCHA.md)
