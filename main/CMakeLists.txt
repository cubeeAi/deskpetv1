idf_component_register(
    SRCS
        "app/app_main.cpp"
        "app/app_types.cpp"
        "display/oled_display.cpp"
        "display/text_render.cpp"
        "display/display_assets.cpp"
        "services/app_services.cpp"
        "ui/ui_runtime.cpp"
    INCLUDE_DIRS "." "include"
    EMBED_FILES "assets/cjk_3000_303f_12.bin" "assets/cjk_3400_9fff_12.bin" "assets/cjk_ff00_ffef_12.bin"
    REQUIRES driver esp_event esp_http_server esp_netif esp_timer esp_wifi json lwip mqtt nvs_flash
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=uninitialized)
