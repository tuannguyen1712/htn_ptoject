idf_component_register(SRCS "gatts_demo.c"
                    REQUIRES uart_handle nvs_flash bt Libs_GattServer
                    INCLUDE_DIRS ".")
