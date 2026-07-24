# 11. Hoja de ruta

Prefiero ser sincero con el estado del proyecto. Funciona y se conduce bien, pero
está vivo y le quedan cosas. Si te apetece colaborar, cualquiera de estas es un
buen punto de entrada.

## Lo que todavía no está bien (y me consta)

- **El receptor todavía no tiene repo propio.** Está por publicar.
- **No hay emparejamiento ni cifrado.** Ahora mismo se emite por broadcast a todo
  el que escuche. Lo suyo sería que emisora y coche se reconozcan entre sí. Ver
  [Comunicación ESP-NOW](04-COMUNICACION-ESP-NOW.md).
- **El canal Wi-Fi no está fijado a mano.** Es la causa más típica de que dos
  placas no se hablen; conviene fijarlo explícitamente en ambos lados.
- **Falta fail-safe en la emisora.** El receptor sí para el motor si se queda sin
  señal, pero la emisora debería tener también su propia red de seguridad. Ver
  [El receptor](08-EL-RECEPTOR.md).
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

« [Anterior: Adaptarlo a tu coche](10-ADAPTARLO-A-TU-COCHE.md) · [📚 Índice](README.md) · [Volver a la portada](../README.md)
