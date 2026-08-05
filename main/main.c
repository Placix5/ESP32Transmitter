#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h" 

// --- LIBRERÍAS USB Y LED ---
#include "esp_private/usb_phy.h" 
#include "led_strip.h"
#include "ssd1306.h"
#include "tusb.h"
#include "xinput_host.h"

// --- LIBRERÍAS ESP-NOW ---
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "nvs_flash.h"

// --- CONFIGURACIÓN ---
#define PIN_LED 48

// --- PANTALLA OLED (SSD1306 128x64, I2C) ---
#define PIN_OLED_SDA 4
#define PIN_OLED_SCL 5
#define OLED_ADDR    0x3C

// Configuración de límites del Servo (perillas independientes: ajustar por prueba y error)

// =============== VALORES ORIGINALES ===============
// ESTOS ÁNGULOS SON LOS ESTÁNDAR PERO PARA EL MODELO DKS-PRO CON PROBLEMAS EN LA DIRECCIÓN, 
// HAY QUE AJUSTARLOS A MANO POR ESO HAY DOS JUEGOS DE VALORES (COMENTADOS Y ACTIVOS) PARA QUE 
// PUEDAS PROBAR CUAL TE VA MEJOR

//#define ANGULO_MIN 65                  // Izquierda ORIGINAL
//#define ANGULO_MAX 115                 // Derecha ORIGINAL
//#define ANGULO_CENTRO 90              // Centro ORIGINAL

#define ANGULO_MIN 45                  // Tope stick a la IZQUIERDA (más pequeño = más giro izq.)
#define ANGULO_MAX 115                 // Tope stick a la DERECHA  (más grande  = más giro der.)
#define ANGULO_CENTRO 85               // Recto / trim (subir o bajar para enderezar las ruedas)
#define ESC_NEUTRO 90

// Configuración de la zona muerta
#define DEAD_ZONE 4000 

// --- CONFIGURACIÓN DE POTENCIA POR MODO ---
// Según cómo esté orientado el motor / cableado el ESC, "acelerar" necesita valores de
// PWM por ENCIMA del neutro (90 -> 180) o por DEBAJO (90 -> 0). Si el coche va al revés
// (el gatillo de gas frena y el de freno acelera), descomenta MOTOR_INVERTIDO: se
// intercambian los juegos de valores sin tocar nada más.
//#define MOTOR_INVERTIDO

#ifndef MOTOR_INVERTIDO
  // Normal: gas acelera hacia arriba (90 -> 180), freno hacia abajo (90 -> 0)
  #define GAS_ECO 120
  #define GAS_NORMAL 150
  #define GAS_SPORT 180

  #define FRENO_ECO 60
  #define FRENO_NORMAL 30
  #define FRENO_SPORT 0
#else
  // Invertido: gas acelera hacia abajo (90 -> 0), freno hacia arriba (90 -> 180)
  #define GAS_ECO 60
  #define GAS_NORMAL 30
  #define GAS_SPORT 0

  #define FRENO_ECO 120
  #define FRENO_NORMAL 150
  #define FRENO_SPORT 180
#endif

// Configuración de modos de conducción
typedef enum {
  Eco,
  Normal,
  Sport
} ModoConduccion;

// --- ESTRUCTURA MENSAJES ---
// IDA (emisora -> coche). t_ms = millis() del emisor al enviar; el coche lo devuelve
// tal cual para medir el round-trip. DEBE ser idéntica en el receptor.
typedef struct __attribute__((packed)) MensajeRadio {
  uint8_t  modo_conduccion;
  uint8_t  pwm_motor;
  uint8_t  angulo_servo;
  uint32_t t_ms;            // marca de tiempo del emisor (para medir la latencia)
} MensajeRadio;

// VUELTA (coche -> emisora). Telemetría a baja frecuencia. Idéntica en el receptor.
typedef struct __attribute__((packed)) Telemetria {
  uint32_t t_ms_eco;        // eco del t_ms del último control recibido -> RTT
  int8_t   rssi_coche;      // RSSI (dBm) que ve el COCHE de los paquetes de control
} Telemetria;

// --- VARIABLES GLOBALES ---
ModoConduccion modoSeleccionado = Eco;

// Estado que se transmite al coche. Lo ESCRIBE el callback USB (core 0) y lo LEE la
// tarea control_tx_task (core 1), así que se protege con un portMUX. Arranca en NEUTRO
// para que, mientras no haya mando, el coche reciba "parado y recto".
MensajeRadio estadoTx = { Eco, ESC_NEUTRO, ANGULO_CENTRO, 0 };
portMUX_TYPE txMux = portMUX_INITIALIZER_UNLOCKED;
bool mandoConectado = false;

// Telemetría recibida del coche (la escribe el callback en la tarea WiFi, la lee la
// pantalla). Tipos de 32/8 bits alineados => lectura atómica, con volatile basta.
volatile uint32_t rtt_ms = 0;            // latencia ida y vuelta (ms)
volatile int8_t   rssi_coche = 0;        // RSSI que ve el coche
volatile uint32_t ultimaTelemetria = 0;  // millis() del último paquete de vuelta

// Enlace y red
uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// MAC de la interfaz WiFi STA del receptor (ESP-NOW usa WiFi, NO la MAC de BT/Ethernet).
// El ESP32-C3 derivaba: WiFi STA = base(+0)=EC, AP(+1)=ED, BT(+2)=EE, Ethernet(+3)=EF.
uint8_t client_mac[] = {0xDC, 0x06, 0x75, 0xF9, 0x62, 0xEC};

// Variables para el cambio de modo (Botones LB + RB)
uint32_t tiempoInicioPulsacion = 0;
bool comboLBRBPulsado = false;   // ambos botones mantenidos ahora mismo
bool modoCambiado = false;
const uint32_t TIEMPO_PARA_CAMBIAR = 2000;

// Último mando conocido (para poder vibrar / cambiar modo desde la tarea USB)
uint8_t mando_dev_addr = 0;
uint8_t mando_instance = 0;

// Variables para controlar cuándo apagar la vibración
bool vibracionActiva = false;
uint32_t tiempoInicioVibracion = 0;
uint32_t DURACION_VIBRACION = 0;

// Hardware local
led_strip_handle_t led_strip;

// --- FUNCIONES AUXILIARES ---
uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

long map_value(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

long constrain_value(long x, long min, long max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

// ---------------------------------------------------------
// ENLACE ENTRE TINYUSB Y XINPUT
// ---------------------------------------------------------
extern usbh_class_driver_t const usbh_xinput_driver;
usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count) {
    *driver_count = 1; 
    return &usbh_xinput_driver; 
}

// --- LÓGICA DEL MANDO ---
void cambiarModo(uint8_t dev_addr, uint8_t instance) {
  switch (modoSeleccionado) {
    case Eco:
      modoSeleccionado = Normal;
      tuh_xinput_set_rumble(dev_addr, instance, 0, 100, true);
      DURACION_VIBRACION = 250; 
      // Azul para Modo Normal (R:0, G:0, B:255)
      led_strip_set_pixel(led_strip, 0, 0, 0, 255); 
      break;
    case Normal:
      modoSeleccionado = Sport;
      tuh_xinput_set_rumble(dev_addr, instance, 0, 255, true);
      DURACION_VIBRACION = 500; 
      // Rojo para Modo Sport (R:255, G:0, B:0)
      led_strip_set_pixel(led_strip, 0, 255, 0, 0);
      break;
    case Sport:
      modoSeleccionado = Eco;
      tuh_xinput_set_rumble(dev_addr, instance, 50, 0, true);
      DURACION_VIBRACION = 150; 
      // Verde para Modo Eco (R:0, G:255, B:0)
      led_strip_set_pixel(led_strip, 0, 0, 255, 0);
      break;
  }
  
  // Refrescamos el LED para aplicar el nuevo color
  led_strip_refresh(led_strip);

  // Guardamos el tiempo exacto en el que empezamos a vibrar
  vibracionActiva = true;
  tiempoInicioVibracion = millis();
}

// --- CALLBACKS DE EVENTOS USB ---
void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance, const xinputh_interface_t *xinput_itf) {
    // Cuando el mando se conecta, fijamos el LED al color inicial (Verde = Eco)
    led_strip_set_pixel(led_strip, 0, 0, 255, 0);
    led_strip_refresh(led_strip);

    mandoConectado = true;

    tuh_xinput_receive_report(dev_addr, instance);
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance) {
    // Apagamos LED si se desconecta
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);

    // Reseteamos estados para no arrastrar una pulsación/vibración de un mando que ya no está
    comboLBRBPulsado = false;
    vibracionActiva = false;
    mandoConectado = false;

    // FAILSAFE (lado emisor): si el mando se va, forzamos NEUTRO inmediato para que el
    // coche pare al instante, sin esperar al timeout del receptor. La tarea TX seguirá
    // enviando este estado neutro a 50 Hz.
    portENTER_CRITICAL(&txMux);
    estadoTx.modo_conduccion = (uint8_t)modoSeleccionado;
    estadoTx.pwm_motor       = ESC_NEUTRO;
    estadoTx.angulo_servo    = ANGULO_CENTRO;
    portEXIT_CRITICAL(&txMux);
}

void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance, xinputh_interface_t const *xinput_itf, uint16_t len) {
    xinput_gamepad_t const *pad = &xinput_itf->pad;
    
    // 1. DIRECCIÓN
    // Mapeamos cada mitad del stick por separado alrededor de ANGULO_CENTRO, para poder
    // ajustar de forma INDEPENDIENTE cuánto gira a cada lado (las cogidas hacen que gire
    // más a un lado que al otro) y usar ANGULO_CENTRO como trim de "ruedas rectas".
    int16_t rawX = pad->sThumbLX;
    int angulo = ANGULO_CENTRO; // Reposo = recto

    if (rawX > DEAD_ZONE) {
        // Stick a la derecha: centro -> ANGULO_MAX
        angulo = map_value(rawX, DEAD_ZONE, 32767, ANGULO_CENTRO, ANGULO_MAX);
    } else if (rawX < -DEAD_ZONE) {
        // Stick a la izquierda: centro -> ANGULO_MIN
        angulo = map_value(rawX, -DEAD_ZONE, -32768, ANGULO_CENTRO, ANGULO_MIN);
    }
    angulo = constrain_value(angulo, ANGULO_MIN, ANGULO_MAX);
      
    // 2. DETECCIÓN DE LA COMBINACIÓN LB + RB
    // Aquí SOLO registramos el estado de los botones. El temporizado (mantener 2 s) se
    // evalúa en tusb_host_task con cadencia fija, porque el mando únicamente envía reportes
    // cuando algo cambia: si mantienes los botones quietos puede dejar de enviarlos y el
    // "millis() - inicio" nunca se comprobaría (por eso a veces no cambiaba de modo).
    mando_dev_addr = dev_addr;
    mando_instance = instance;
    bool comboAhora = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) && (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
    if (comboAhora && !comboLBRBPulsado) {
        // Flanco de pulsación: arrancamos el cronómetro
        comboLBRBPulsado = true;
        modoCambiado = false;
        tiempoInicioPulsacion = millis();
    } else if (!comboAhora) {
        // Al soltar siempre llega un reporte (es un cambio de estado), así que esto sí es fiable
        comboLBRBPulsado = false;
    }

    // 3. GATILLOS
    int gas = pad->bRightTrigger;
    int freno = pad->bLeftTrigger;
    int valorMotor = ESC_NEUTRO;

    if (gas > 0) {
        switch(modoSeleccionado) {
            case Eco:    valorMotor = map_value(gas, 0, 255, ESC_NEUTRO, GAS_ECO); break;
            case Normal: valorMotor = map_value(gas, 0, 255, ESC_NEUTRO, GAS_NORMAL); break;
            case Sport:  valorMotor = map_value(gas, 0, 255, ESC_NEUTRO, GAS_SPORT); break;
        }
    } else if (freno > 0) {
        switch(modoSeleccionado) {
            case Eco:    valorMotor = map_value(freno, 0, 255, ESC_NEUTRO, FRENO_ECO); break;
            case Normal: valorMotor = map_value(freno, 0, 255, ESC_NEUTRO, FRENO_NORMAL); break;
            case Sport:  valorMotor = map_value(freno, 0, 255, ESC_NEUTRO, FRENO_SPORT); break;
        }
    }
      
    // (El apagado de la vibración también se gestiona en tusb_host_task, por el mismo
    //  motivo: no depender de que sigan llegando reportes del mando.)

    // 4. ACTUALIZAR EL ESTADO A TRANSMITIR
    // Ya NO enviamos aquí. Solo guardamos el último estado; el envío lo hace
    // control_tx_task a 50 Hz constantes. Así el failsafe del receptor mide bien la
    // pérdida de señal aunque el mando deje de reportar al mantener los sticks quietos.
    portENTER_CRITICAL(&txMux);
    estadoTx.angulo_servo    = (uint8_t)angulo;
    estadoTx.pwm_motor       = (uint8_t)valorMotor;
    estadoTx.modo_conduccion = (uint8_t)modoSeleccionado;
    portEXIT_CRITICAL(&txMux);

    // Pedimos el siguiente paquete de datos al mando
    tuh_xinput_receive_report(dev_addr, instance);
}


// --- TAREA DEL KERNEL USB ---
void tusb_host_task(void *arg) {
    while (1) {
        tuh_task();

        // CAMBIO DE MODO: mantener LB + RB durante TIEMPO_PARA_CAMBIAR.
        // Se evalúa aquí (cadencia fija ~cada tick) y NO en el callback del reporte,
        // porque el mando deja de enviar reportes si mantienes los botones quietos.
        if (comboLBRBPulsado && !modoCambiado &&
            (millis() - tiempoInicioPulsacion) >= TIEMPO_PARA_CAMBIAR) {
            cambiarModo(mando_dev_addr, mando_instance);
            modoCambiado = true;
        }

        // APAGADO DE LA VIBRACIÓN: mismo motivo, cadencia fija.
        // IMPORTANTE: leemos millis() FRESCO aquí. Si reutilizásemos una marca tomada
        // antes de cambiarModo(), tiempoInicioVibracion (fijado dentro) sería mayor y la
        // resta sin signo se desbordaría, apagando el rumble en la misma vuelta.
        if (vibracionActiva && (millis() - tiempoInicioVibracion) >= DURACION_VIBRACION) {
            tuh_xinput_set_rumble(mando_dev_addr, mando_instance, 0, 0, true);
            vibracionActiva = false;
        }

        vTaskDelay(1);
    }
}

// --- RECEPCIÓN DE TELEMETRÍA DEL COCHE (contexto tarea WiFi) ---
// Llega ~5 veces/s. Calculamos la latencia (ahora - eco del t_ms) y guardamos el RSSI.
void on_telemetria(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == (int)sizeof(Telemetria)) {
        Telemetria t;
        memcpy(&t, data, sizeof(t));
        rtt_ms = millis() - t.t_ms_eco;
        rssi_coche = t.rssi_coche;
        ultimaTelemetria = millis();
    }
}

// --- TAREA DE TRANSMISIÓN DE CONTROL (50 Hz) ---
// Envía el último estado al coche a cadencia FIJA, envíe o no reportes el mando. Ese
// flujo constante es lo que (1) permite que el failsafe del receptor detecte de verdad
// la pérdida de señal, y (2) resulta más regular (y normalmente menos tráfico) que
// enviar por cada reporte del mando.
void control_tx_task(void *arg) {
    const TickType_t periodo = pdMS_TO_TICKS(20);   // 20 ms = 50 Hz
    TickType_t ultimaHora = xTaskGetTickCount();
    while (1) {
        MensajeRadio copia;
        portENTER_CRITICAL(&txMux);
        copia = estadoTx;                 // instantánea atómica del estado
        portEXIT_CRITICAL(&txMux);

        copia.t_ms = millis();            // sello de tiempo en el momento real de envío
        esp_now_send(client_mac, (uint8_t *)&copia, sizeof(copia));

        vTaskDelayUntil(&ultimaHora, periodo);
    }
}

// --- ICONO DE COBERTURA (estilo señal de móvil) ---
// Traduce el RSSI (dBm) a 0..4 barras y las dibuja crecientes; las vacías dejan su zócalo.
static int rssi_a_barras(int rssi) {
    // Umbrales realistas para ESP-NOW: el RSSI se estanca sobre -50/-65 aunque estén
    // pegados, así que -62 debe ser "lleno". Baja de verdad al alejarte.
    if (rssi >= -70)  return 4;   // enlace fuerte (cerca o media distancia)
    if (rssi >= -82)  return 3;   // bien
    if (rssi >= -92)  return 2;   // regular
    if (rssi >= -100) return 1;   // débil, cerca del borde
    return 0;                     // perdiéndose
}
static void dibujar_senal(int x, int y_base, int barras) {
    for (int i = 0; i < 4; i++) {
        int h  = 2 + i * 2;          // alturas 2,4,6,8 px
        int bx = x + i * 4;          // 3 px de ancho + 1 de hueco
        if (i < barras) ssd1306_fill_rect(bx, y_base - h, 3, h, true);
        else            ssd1306_fill_rect(bx, y_base - 1, 3, 1, true);   // zócalo (barra vacía)
    }
}

// --- TAREA DE PANTALLA (telemetría, ~15 Hz) ---
// Va APARTE de la tarea TX: un volcado completo del SSD1306 tarda ~25 ms por I2C,
// demasiado para meterlo en el bucle de 50 Hz del control.
void oled_task(void *arg) {
    char linea[24];
    while (1) {
        MensajeRadio s;
        portENTER_CRITICAL(&txMux);
        s = estadoTx;                       // instantánea del estado transmitido
        portEXIT_CRITICAL(&txMux);
        ModoConduccion modo = modoSeleccionado;
        bool conectado = mandoConectado;

        uint32_t ahora = millis();
        bool enlace = (ahora - ultimaTelemetria) < 1000;   // telemetría reciente = hay enlace con el coche
        uint32_t rtt = rtt_ms;
        int rssi = rssi_coche;
        int barras = enlace ? rssi_a_barras(rssi) : 0;

        const char *nombreModo = (modo == Eco) ? "ECO" : (modo == Normal) ? "NORMAL" : "SPORT";

        // GAS con signo: + acelerar, - frenar (respeta MOTOR_INVERTIDO). En % de recorrido.
#ifdef MOTOR_INVERTIDO
        int gas_sign = ESC_NEUTRO - (int)s.pwm_motor;
#else
        int gas_sign = (int)s.pwm_motor - ESC_NEUTRO;
#endif
        int gas_pct = gas_sign * 100 / (180 - ESC_NEUTRO);

        // DIR con signo: + derecha, - izquierda, centrado en ANGULO_CENTRO (cada lado a su escala).
        int dir = (int)s.angulo_servo - ANGULO_CENTRO;
        int dir_pct;
        if (dir >= 0) dir_pct = (ANGULO_MAX > ANGULO_CENTRO) ? dir * 100 / (ANGULO_MAX - ANGULO_CENTRO) : 0;
        else          dir_pct = (ANGULO_CENTRO > ANGULO_MIN) ? dir * 100 / (ANGULO_CENTRO - ANGULO_MIN) : 0;

        ssd1306_clear();

        // Cabecera: título + icono de cobertura y RSSI arriba a la derecha
        ssd1306_draw_string(0, 0, "EMISORA TX");
        dibujar_senal(90, 8, barras);
        if (enlace) snprintf(linea, sizeof(linea), "%d", rssi);
        else        snprintf(linea, sizeof(linea), "--");
        ssd1306_draw_string(108, 1, linea);

        ssd1306_fill_rect(0, 10, SSD1306_WIDTH, 1, true);   // separador

        snprintf(linea, sizeof(linea), "MODO: %s", nombreModo);
        ssd1306_draw_string(0, 13, linea);

        ssd1306_draw_string(0, 24, conectado ? "MANDO: OK" : "MANDO: ---");

        if (enlace) snprintf(linea, sizeof(linea), "PING: %lu MS", (unsigned long)rtt);
        else        snprintf(linea, sizeof(linea), "PING: ---");
        ssd1306_draw_string(0, 35, linea);

        // Gas y dirección centrados en 0 (acelerar/derecha +, frenar/izquierda -)
        snprintf(linea, sizeof(linea), "GAS:%+d DIR:%+d", gas_pct, dir_pct);
        ssd1306_draw_string(0, 46, linea);

        ssd1306_flush();
        vTaskDelay(pdMS_TO_TICKS(66));   // ~15 Hz; gracias al volcado por páginas, es barato
    }
}

void app_main(void) {
    // Pequeño respiro al arrancar para que el voltaje se estabilice
    vTaskDelay(pdMS_TO_TICKS(500));

    // 1. Inicializar LED 
    led_strip_config_t strip_config = { .strip_gpio_num = PIN_LED, .max_leds = 1, };
    led_strip_rmt_config_t rmt_config = { .resolution_hz = 10 * 1000 * 1000, };
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    led_strip_clear(led_strip);

    // 1b. Inicializar la pantalla OLED y mostrar una pantalla de arranque.
    //     Luego oled_task la sobrescribe con la telemetría en vivo.
    if (ssd1306_init(PIN_OLED_SDA, PIN_OLED_SCL, OLED_ADDR)) {
        ssd1306_clear();
        ssd1306_draw_string(0, 0,  "ESP32 RC");
        ssd1306_draw_string(0, 12, "EMISORA  TX");
        ssd1306_draw_string(0, 40, "ARRANCANDO...");
        ssd1306_flush();
    }

    // 2. Inicializar NVS (con autolimpieza para evitar Boot Loops)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 3. Inicializar WiFi y ESP-NOW
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    // Sin power save: el enlace de control necesita el radio siempre despierto (menos latencia).
    esp_wifi_set_ps(WIFI_PS_NONE);

    // LONG RANGE: modulación propietaria de Espressif, mucho más robusta y con más alcance.
    // ¡CUIDADO! El receptor DEBE tener EXACTAMENTE esto mismo o dejan de comunicarse por completo.
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);

    esp_now_init();
    
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, client_mac, 6);
    peerInfo.channel = 0;     
    peerInfo.encrypt = false; 
    esp_now_add_peer(&peerInfo);

    // Recepción de la telemetría de vuelta del coche.
    esp_now_register_recv_cb(on_telemetria);

    // 4. Inicializar USB Host
    usb_phy_config_t phy_config = { 
        .controller = USB_PHY_CTRL_OTG, 
        .target = USB_PHY_TARGET_INT, 
        .otg_mode = USB_OTG_MODE_HOST, 
        .otg_speed = USB_PHY_SPEED_UNDEFINED, 
    };
    usb_phy_handle_t phy_handle;
    usb_new_phy(&phy_config, &phy_handle);
    tusb_init();

    // 5. Lanzar proceso USB en Núcleo 0
    xTaskCreatePinnedToCore(tusb_host_task, "tusb_host_task", 4096, NULL, 5, NULL, 0);

    // 6. Lanzar la transmisión de control a 50 Hz en Núcleo 1 (separada del host USB del 0)
    xTaskCreatePinnedToCore(control_tx_task, "control_tx_task", 4096, NULL, 5, NULL, 1);

    // 7. Lanzar la pantalla (telemetría) a baja frecuencia y prioridad, en el Núcleo 1
    xTaskCreatePinnedToCore(oled_task, "oled_task", 4096, NULL, 3, NULL, 1);
}