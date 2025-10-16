idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES Libs_FireBase Libs_wifiSta Libs_BleGattClient Libs_softAP Libs_nvsHandle Configuration Libs_httpConfig
                    )
