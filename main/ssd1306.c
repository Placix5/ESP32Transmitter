#include "ssd1306.h"
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "ssd1306";

static i2c_master_dev_handle_t s_dev;
static bool s_ready = false;

// Buffer de pantalla: 128 columnas x 8 páginas (cada byte = 8 píxeles verticales).
static uint8_t s_buf[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
// Buffer de salida para el volcado (control byte 0x40 + datos). Estático para no
// meter 1 KB en la pila de la tarea que llame a flush.
static uint8_t s_out[1 + sizeof(s_buf)];

// Envía una tanda de comandos (control byte 0x00 = "lo que sigue son comandos").
static bool ssd1306_cmd(const uint8_t *cmds, size_t n) {
    uint8_t tmp[40];
    if (n + 1 > sizeof(tmp)) return false;
    tmp[0] = 0x00;
    memcpy(&tmp[1], cmds, n);
    return i2c_master_transmit(s_dev, tmp, n + 1, 100) == ESP_OK;
}

// Secuencia de arranque estándar para un 128x64.
static const uint8_t init_seq[] = {
    0xAE,             // display off
    0x20, 0x00,       // memory addressing mode = horizontal
    0xB0,             // page start address
    0xC8,             // COM output scan direction = remapped
    0x00, 0x10,       // column start (low/high nibble)
    0x40,             // start line = 0
    0x81, 0x7F,       // contrast
    0xA1,             // segment re-map
    0xA6,             // normal display (no invertido)
    0xA8, 0x3F,       // multiplex ratio = 63 (64 filas)
    0xA4,             // salida sigue el contenido de RAM
    0xD3, 0x00,       // display offset = 0
    0xD5, 0x80,       // clock divide / oscillator
    0xD9, 0xF1,       // pre-charge period
    0xDA, 0x12,       // COM pins hardware config
    0xDB, 0x40,       // VCOMH deselect level
    0x8D, 0x14,       // charge pump ON
    0xAF,             // display on
};

bool ssd1306_init(int sda_gpio, int scl_gpio, uint8_t addr) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,                 // -1 = elige puerto libre automáticamente
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    if (i2c_new_master_bus(&bus_cfg, &bus) != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo crear el bus I2C");
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(bus, &dev_cfg, &s_dev) != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo añadir el dispositivo 0x%02X", addr);
        return false;
    }

    // Sondeo: si no responde, casi seguro es cableado o dirección equivocada.
    if (i2c_master_probe(bus, addr, 100) != ESP_OK) {
        ESP_LOGW(TAG, "El panel no responde en 0x%02X (revisa SDA/SCL/VCC y la direccion)", addr);
    }

    s_ready = true;
    if (!ssd1306_cmd(init_seq, sizeof(init_seq))) {
        ESP_LOGE(TAG, "Fallo enviando la secuencia de init");
        s_ready = false;
        return false;
    }

    ssd1306_clear();
    ssd1306_flush();
    ESP_LOGI(TAG, "OLED SSD1306 lista en 0x%02X (SDA=%d, SCL=%d)", addr, sda_gpio, scl_gpio);
    return true;
}

void ssd1306_clear(void) {
    memset(s_buf, 0x00, sizeof(s_buf));
}

void ssd1306_set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) return;
    uint16_t idx = (y / 8) * SSD1306_WIDTH + x;
    uint8_t mask = 1 << (y & 7);
    if (on) s_buf[idx] |= mask;
    else    s_buf[idx] &= ~mask;
}

void ssd1306_fill_rect(int x, int y, int w, int h, bool on) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            ssd1306_set_pixel(x + i, y + j, on);
}

void ssd1306_draw_rect(int x, int y, int w, int h, bool on) {
    for (int i = 0; i < w; i++) {
        ssd1306_set_pixel(x + i, y, on);
        ssd1306_set_pixel(x + i, y + h - 1, on);
    }
    for (int j = 0; j < h; j++) {
        ssd1306_set_pixel(x, y + j, on);
        ssd1306_set_pixel(x + w - 1, y + j, on);
    }
}

// --- Fuente 5x7 (columnas, bit0 = fila superior). Índice = carácter - 0x20 ---
// Solo el rango 0x20..0x5A (espacio..'Z'); los símbolos que no uso quedan en blanco.
#define FONT_FIRST 0x20
#define FONT_LAST  0x5A
static const uint8_t font5x7[FONT_LAST - FONT_FIRST + 1][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // 0x20 espacio
    {0x00,0x00,0x00,0x00,0x00}, // 0x21 !
    {0x00,0x00,0x00,0x00,0x00}, // 0x22 "
    {0x00,0x00,0x00,0x00,0x00}, // 0x23 #
    {0x00,0x00,0x00,0x00,0x00}, // 0x24 $
    {0x23,0x13,0x08,0x64,0x62}, // 0x25 %
    {0x00,0x00,0x00,0x00,0x00}, // 0x26 &
    {0x00,0x00,0x00,0x00,0x00}, // 0x27 '
    {0x00,0x1C,0x22,0x41,0x00}, // 0x28 (
    {0x00,0x41,0x22,0x1C,0x00}, // 0x29 )
    {0x00,0x00,0x00,0x00,0x00}, // 0x2A *
    {0x08,0x08,0x3E,0x08,0x08}, // 0x2B +
    {0x00,0x00,0x00,0x00,0x00}, // 0x2C ,
    {0x08,0x08,0x08,0x08,0x08}, // 0x2D -
    {0x00,0x60,0x60,0x00,0x00}, // 0x2E .
    {0x20,0x10,0x08,0x04,0x02}, // 0x2F /
    {0x3E,0x51,0x49,0x45,0x3E}, // 0x30 0
    {0x00,0x42,0x7F,0x40,0x00}, // 0x31 1
    {0x42,0x61,0x51,0x49,0x46}, // 0x32 2
    {0x21,0x41,0x45,0x4B,0x31}, // 0x33 3
    {0x18,0x14,0x12,0x7F,0x10}, // 0x34 4
    {0x27,0x45,0x45,0x45,0x39}, // 0x35 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 0x36 6
    {0x01,0x71,0x09,0x05,0x03}, // 0x37 7
    {0x36,0x49,0x49,0x49,0x36}, // 0x38 8
    {0x06,0x49,0x49,0x29,0x1E}, // 0x39 9
    {0x00,0x36,0x36,0x00,0x00}, // 0x3A :
    {0x00,0x00,0x00,0x00,0x00}, // 0x3B ;
    {0x00,0x08,0x14,0x22,0x41}, // 0x3C <
    {0x14,0x14,0x14,0x14,0x14}, // 0x3D =
    {0x41,0x22,0x14,0x08,0x00}, // 0x3E >
    {0x00,0x00,0x00,0x00,0x00}, // 0x3F ?
    {0x00,0x00,0x00,0x00,0x00}, // 0x40 @
    {0x7E,0x11,0x11,0x11,0x7E}, // 0x41 A
    {0x7F,0x49,0x49,0x49,0x36}, // 0x42 B
    {0x3E,0x41,0x41,0x41,0x22}, // 0x43 C
    {0x7F,0x41,0x41,0x22,0x1C}, // 0x44 D
    {0x7F,0x49,0x49,0x49,0x41}, // 0x45 E
    {0x7F,0x09,0x09,0x09,0x01}, // 0x46 F
    {0x3E,0x41,0x49,0x49,0x7A}, // 0x47 G
    {0x7F,0x08,0x08,0x08,0x7F}, // 0x48 H
    {0x00,0x41,0x7F,0x41,0x00}, // 0x49 I
    {0x20,0x40,0x41,0x3F,0x01}, // 0x4A J
    {0x7F,0x08,0x14,0x22,0x41}, // 0x4B K
    {0x7F,0x40,0x40,0x40,0x40}, // 0x4C L
    {0x7F,0x02,0x0C,0x02,0x7F}, // 0x4D M
    {0x7F,0x04,0x08,0x10,0x7F}, // 0x4E N
    {0x3E,0x41,0x41,0x41,0x3E}, // 0x4F O
    {0x7F,0x09,0x09,0x09,0x06}, // 0x50 P
    {0x3E,0x41,0x51,0x21,0x5E}, // 0x51 Q
    {0x7F,0x09,0x19,0x29,0x46}, // 0x52 R
    {0x46,0x49,0x49,0x49,0x31}, // 0x53 S
    {0x01,0x01,0x7F,0x01,0x01}, // 0x54 T
    {0x3F,0x40,0x40,0x40,0x3F}, // 0x55 U
    {0x1F,0x20,0x40,0x20,0x1F}, // 0x56 V
    {0x7F,0x20,0x18,0x20,0x7F}, // 0x57 W
    {0x63,0x14,0x08,0x14,0x63}, // 0x58 X
    {0x03,0x04,0x78,0x04,0x03}, // 0x59 Y
    {0x61,0x51,0x49,0x45,0x43}, // 0x5A Z
};

void ssd1306_draw_char(int x, int y, char c) {
    if (c >= 'a' && c <= 'z') c -= 32;                 // minúsculas -> mayúsculas
    if (c < FONT_FIRST || c > FONT_LAST) c = ' ';      // fuera de rango -> espacio
    const uint8_t *g = font5x7[(uint8_t)c - FONT_FIRST];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            ssd1306_set_pixel(x + col, y + row, (g[col] >> row) & 1);
        }
    }
    // La 6ª columna se deja en blanco como separación entre caracteres.
}

void ssd1306_draw_string(int x, int y, const char *s) {
    while (*s) {
        if (x + 5 > SSD1306_WIDTH) break;              // no salir de pantalla
        ssd1306_draw_char(x, y, *s++);
        x += 6;
    }
}

void ssd1306_flush(void) {
    if (!s_ready) return;
    // Fijamos la ventana a toda la pantalla: columnas 0..127, páginas 0..7.
    uint8_t win[] = { 0x21, 0, 127, 0x22, 0, 7 };
    ssd1306_cmd(win, sizeof(win));

    // Datos: control byte 0x40 + los 1024 bytes del buffer.
    s_out[0] = 0x40;
    memcpy(&s_out[1], s_buf, sizeof(s_buf));
    i2c_master_transmit(s_dev, s_out, sizeof(s_out), 100);
}
