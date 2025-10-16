idf_component_register(SRCS "uart_handle.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver
                    REQUIRES esp_timer)
