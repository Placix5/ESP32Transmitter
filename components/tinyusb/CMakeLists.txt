# 1. Definimos las rutas relativas
set(TINYUSB_DIR ${CMAKE_CURRENT_LIST_DIR})
# Subimos un nivel (..) y entramos a donde se clonó el submódulo de xinput
set(XINPUT_DIR "${CMAKE_CURRENT_LIST_DIR}/../src/lib/tusb_xinput")

# 2. Elegimos a dedo los archivos .c que queremos compilar (Solo los de Host)
set(SRCS
    # Core de TinyUSB
    "src/tusb.c"
    "src/common/tusb_fifo.c"

    # Core del Host
    "src/host/usbh.c"
    "src/host/hub.c"

    # El driver físico para el ESP32-S3 (Usa el controlador DWC2)
    "src/portable/synopsys/dwc2/hcd_dwc2.c"
    "src/portable/synopsys/dwc2/dwc2_common.c" # (Si el compilador no lo encuentra, puedes borrar esta línea)

    # El driver de Ryzee119
    "${XINPUT_DIR}/xinput_host.c"
)

# 3. Le decimos dónde están las carpetas con los archivos .h
set(INCLUDE_DIRS
    "src"
    "${XINPUT_DIR}"
)

# 4. Registramos todo como un componente nativo de ESP-IDF
idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES freertos
)

# 5. MAGIA: Le decimos al componente que busque tu "tusb_config.h" en la carpeta main
target_include_directories(${COMPONENT_LIB} PUBLIC "${CMAKE_SOURCE_DIR}/main")

# 6. TinyUSB es codigo vendorizado de terceros: no tratamos sus warnings como errores.
# El GCC mas reciente (ESP-IDF nuevo) emite -Wtype-limits en get_driver() de usbh.c
# porque BUILTIN_DRIVER_COUNT == 0 (todas las clases host built-in estan desactivadas;
# aqui solo usamos XInput como driver de aplicacion), y "drv_id < 0" con drv_id sin signo
# es siempre falso. En el sobremesa el GCC antiguo no lo emitia; asi el build es portable.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=type-limits)