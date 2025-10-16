idf_component_register(SRCS "Libs_FireBase.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_flash json esp_event esp_netif esp_wifi esp_http_client mbedtls
                    )
