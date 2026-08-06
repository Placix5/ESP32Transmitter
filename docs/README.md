# Documentación

Bienvenido a la parte que de verdad importa: **entender cómo funciona esto por
dentro**.

No hace falta que sepas programar ni que hayas tocado un microcontrolador antes.
Cada sección está pensada para leerse sola, poco a poco, con analogías y ejemplos.
Si las lees en orden, al terminar entenderás cada pieza del proyecto: qué es un
ESP32, por qué hay dos, cómo se lee un mando de Xbox, qué es ESP-NOW y cómo un
número acaba moviendo las ruedas.

## Cómo leer esto

Puedes ir en orden como si fuera un pequeño libro, o saltar directo a lo que te
interese. Cada página tiene al pie una navegación para moverte.

### Los cimientos
1. [**Por qué existe**](01-POR-QUE-EXISTE.md) — la historia del alcance y la idea
   de partir el sistema en dos.
2. [**Arquitectura del sistema**](02-ARQUITECTURA-DEL-SISTEMA.md) — por qué dos
   ESP32 distintos y el viaje completo de una orden.

### Las dos conversaciones
3. [**Leer el mando**](03-LEER-EL-MANDO.md) — USB Host, XInput y TinyUSB, y el
   parche que hubo que hacer para que compilara.
4. [**Comunicación ESP-NOW**](04-COMUNICACION-ESP-NOW.md) — por qué gana al
   Bluetooth y qué viaja en el paquete de 3 bytes.

### Lo que ocurre en medio
5. [**Lógica de conducción**](05-LOGICA-DE-CONDUCCION.md) — zona muerta, mapeo de
   valores y los tres modos.
6. [**PWM**](06-PWM.md) — cómo un simple número mueve un servo y un motor.
7. [**FreeRTOS y núcleos**](07-FREERTOS-Y-NUCLEOS.md) — tareas, los dos núcleos
   del S3 y por qué el orden de arranque importa.

### El otro lado y la práctica
8. [**El receptor**](08-EL-RECEPTOR.md) — qué hace (y qué debe hacer) el ESP32-C3
   del coche.
9. [**Requisitos y entorno**](09-ENTORNO-ESP-IDF.md) — el hardware necesario y cómo
   instalar ESP-IDF en VSCode paso a paso.
10. [**Puesta en marcha**](10-PUESTA-EN-MARCHA.md) — compilar, flashear y conducir
    con los botones de VSCode (o por terminal).
11. [**Adaptarlo a tu coche**](11-ADAPTARLO-A-TU-COCHE.md) — el modo configuración,
    el trim guardado en memoria y la calibración de tu chasis.
12. [**Pantalla y telemetría**](12-PANTALLA-Y-TELEMETRIA.md) — la OLED, la latencia
    real con el coche y el icono de cobertura.
13. [**Hoja de ruta**](13-HOJA-DE-RUTA.md) — lo que todavía no está bien y en qué
    puedes echar una mano.

---

« [Volver a la portada](../README.md) · [Empezar: Por qué existe »](01-POR-QUE-EXISTE.md)
