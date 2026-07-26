# 12. Hoja de ruta

Prefiero ser sincero con el estado del proyecto. Funciona y se conduce bien, pero
está vivo y le quedan cosas. Si te apetece colaborar, cualquiera de estas es un
buen punto de entrada.

## Ya resuelto (antes estaba en esta lista)

Para que se note el progreso, dejo aquí lo que ya está hecho:

- ✅ **Fail-safe en la emisora.** Si se desenchufa el mando, la emisora pasa a NEUTRO
  al instante y lo sigue enviando a 50 Hz. Ver
  [Comunicación ESP-NOW](04-COMUNICACION-ESP-NOW.md).
- ✅ **Envío a ritmo fijo (50 Hz).** Ya no se manda "por cada reporte del mando",
  sino un flujo constante que permite al receptor detectar de verdad la pérdida de
  señal. Ver [FreeRTOS y núcleos](07-FREERTOS-Y-NUCLEOS.md).
- ✅ **Enlace unicast + Long Range.** Se dejó el broadcast: ahora la emisora habla
  directamente a la MAC del coche, con Long Range y sin ahorro de energía para más
  alcance y menos latencia.
- ✅ **El receptor tiene su propio repo** (`ESP32Receiver`), como proyecto ESP-IDF
  nativo.

## Lo que todavía no está bien (y me consta)

- **No hay cifrado.** El enlace ya es *unicast* (dirigido a la MAC del coche), pero
  va sin cifrar: cualquiera en el mismo canal y con Long Range podría escuchar o
  suplantar. Lo suyo sería activar el cifrado de ESP-NOW. Ver
  [Comunicación ESP-NOW](04-COMUNICACION-ESP-NOW.md).
- **El canal Wi-Fi no está fijado a mano.** Es la causa más típica de que dos
  placas no se hablen; conviene fijarlo explícitamente en ambos lados.
- **No se comprueban los errores de arranque.** Si el LED o la radio fallan al
  iniciarse, el programa sigue como si nada. Habría que abortar con un mensaje
  claro.
- **Sin telemetría.** Estaría bien que el coche devolviera datos (batería, señal)
  para verlos en la emisora.

## Ideas para más adelante

Son cosas que me rondan, sin prisa:

- Una pequeña pantalla en la emisora para ver el modo y el estado sin depender del
  LED.
- Soporte para más de un mando o más de un coche a la vez.
- Ajustar la curva de aceleración (que no sea lineal, para más control en lo bajo).
- Una carcasa impresa en 3D para la emisora.

## Cómo ayudar

Si has llegado hasta aquí, ya sabes más de este proyecto que yo cuando lo empecé.
Cualquier idea, corrección o *pull request* es bienvenida. Y si lo montas y te
funciona (o no), cuéntamelo por los *issues*: saber que alguien más lo ha probado
ya es de ayuda.

La gracia de esto no era solo hacer andar un coche, sino que se entienda cómo
anda.

---

« [Anterior: Adaptarlo a tu coche](11-ADAPTARLO-A-TU-COCHE.md) · [📚 Índice](README.md) · [Volver a la portada](../README.md)
