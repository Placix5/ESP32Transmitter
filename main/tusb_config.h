#pragma once

// --- CONFIGURACIÓN DEL PUERTO FÍSICO ---
// Le decimos a TinyUSB que el puerto 0 operará como Host a máxima velocidad (Full Speed)
#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_HOST | OPT_MODE_FULL_SPEED)

// --- MODO DE OPERACIÓN ---
// Desactivar Device, Activar Host
#define CFG_TUD_ENABLED 0
#define CFG_TUH_ENABLED 1

// --- CLASES ACTIVAS ---
// Habilitar la clase XInput (requerido por Ryzee119)
#define CFG_TUH_XINPUT  1

// --- CONFIGURACIÓN DEL SISTEMA ---
#define CFG_TUSB_MCU    OPT_MCU_ESP32S3
#define CFG_TUSB_OS     OPT_OS_FREERTOS

// --- MEMORIA EXTRA PARA EL MANDO DE XBOX ---
// Aumentamos el buffer de enumeración (El mando de Xbox Series tiene un descriptor enorme)
#define CFG_TUH_ENUMERATION_BUFSIZE 512

// Aumentamos el número máximo de Endpoints permitidos (El mando usa varios para audio y datos)
#define CFG_TUH_ENDPOINT_MAX 16

// (Opcional pero recomendado para ver qué falla) Activar los logs internos de TinyUSB
#define CFG_TUSB_DEBUG 1