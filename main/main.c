#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h" 

// --- LIBRERÍAS USB Y LED ---
#include "esp_private/usb_phy.h" 
#include "led_strip.h"
#include "tusb.h"
#include "xinput_host.h"

// --- LIBRERÍAS ESP-NOW ---
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "nvs_flash.h"

// --- CONFIGURACIÓN ---
#define PIN_LED 48 

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
#define GAS_ECO 120                  
#define GAS_NORMAL 150               
#define GAS_SPORT 180                 

#define FRENO_ECO 60
#define FRENO_NORMAL 30
#define FRENO_SPORT 0

// Configuración de modos de conducción
typedef enum {
  Eco,
  Normal,
  Sport
} ModoConduccion;

// --- ESTRUCTURA MENSAJES ---
typedef struct __attribute__((packed)) MensajeRadio {
  uint8_t modo_conduccion;  
  uint8_t pwm_motor;        
  uint8_t angulo_servo;     
} MensajeRadio;

// --- VARIABLES GLOBALES ---
MensajeRadio instrucciones;          
ModoConduccion modoSeleccionado = Eco;

// Enlace y red
uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t client_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

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
    
    tuh_xinput_receive_report(dev_addr, instance); 
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance) {
    // Apagamos LED si se desconecta
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);

    // Reseteamos estados para no arrastrar una pulsación/vibración de un mando que ya no está
    comboLBRBPulsado = false;
    vibracionActiva = false;
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

    // 4. TRANSMISIÓN
    instrucciones.angulo_servo = (uint8_t)angulo;
    instrucciones.pwm_motor = (uint8_t)valorMotor;
    instrucciones.modo_conduccion = (uint8_t)modoSeleccionado;

    esp_now_send(broadcast_mac, (uint8_t *) &instrucciones, sizeof(instrucciones));

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

void app_main(void) {
    // Pequeño respiro al arrancar para que el voltaje se estabilice
    vTaskDelay(pdMS_TO_TICKS(500));

    // 1. Inicializar LED 
    led_strip_config_t strip_config = { .strip_gpio_num = PIN_LED, .max_leds = 1, };
    led_strip_rmt_config_t rmt_config = { .resolution_hz = 10 * 1000 * 1000, };
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    led_strip_clear(led_strip); 

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

    esp_now_init();
    
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcast_mac, 6);
    peerInfo.channel = 0;     
    peerInfo.encrypt = false; 
    esp_now_add_peer(&peerInfo);

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
}