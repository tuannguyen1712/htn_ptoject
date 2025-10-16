idf_component_register(SRCS "Libs_httpConfig.c"
                    EMBED_FILES "web_server.html" "favicon.ico"
                    INCLUDE_DIRS "include"
                    REQUIRES Configuration
                    REQUIRES "esp_wifi" "nvs_flash" "esp-tls" "esp_http_server" "json")
