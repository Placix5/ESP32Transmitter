#pragma once
#include <stdint.h>
#include <stdbool.h>

// Driver mínimo para OLED SSD1306 128x64 por I2C (API i2c_master, ESP-IDF >= 5.2).
// Pensado para telemetría sencilla: buffer en RAM + volcado completo.

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64

// Inicializa el bus I2C y el panel. Devuelve false si el panel no responde o falla el init.
bool ssd1306_init(int sda_gpio, int scl_gpio, uint8_t addr);

// --- Dibujo sobre el buffer en RAM (no se ve hasta llamar a ssd1306_flush) ---
void ssd1306_clear(void);                                  // apaga todos los píxeles
void ssd1306_set_pixel(int x, int y, bool on);
void ssd1306_fill_rect(int x, int y, int w, int h, bool on);   // rectángulo relleno
void ssd1306_draw_rect(int x, int y, int w, int h, bool on);   // solo el borde

// --- Texto (fuente 5x7; minúsculas se dibujan como mayúsculas) ---
void ssd1306_draw_char(int x, int y, char c);
void ssd1306_draw_string(int x, int y, const char *s);

// Texto en NEGATIVO: apaga los píxeles del glifo y no toca el resto. Pensado para
// escribir encima de un rectángulo relleno (mensajes destacados).
void ssd1306_draw_string_inv(int x, int y, const char *s);

// Vuelca el buffer entero a la pantalla.
void ssd1306_flush(void);
