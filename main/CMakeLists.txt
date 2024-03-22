idf_component_register(SRCS "./components/server/esp_http_interface.c" "esp_mpu_tracker.c" "./components/server/wifi_setup.c"  "./components/mpu/mpu.c"
INCLUDE_DIRS "."
REQUIRES "mpu6050"  esp-tls esp_http_server esp_timer esp_wifi esp_event nvs_flash esp_http_server)
