# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pratica_02_leitura_sensor)

# --- Correcao de compatibilidade: espressif/mpu6050 v1.2.1 + ESP-IDF v6.1 ---
#
# O componente mpu6050 inclui o driver I2C legado (driver/i2c.h), que a partir
# do ESP-IDF 5.3 mora no componente "driver". Mas o CMakeLists do componente
# declara apenas REQUIRES esp_driver_gpio esp_driver_i2c, entao o header nao
# fica visivel e o build falha com:
#     mpu6050.c:12:10: fatal error: driver/i2c.h: No such file or directory
#
# A correcao "manual" seria editar managed_components/espressif__mpu6050/
# CMakeLists.txt -- mas essa pasta esta no .gitignore e e regerada a cada
# fullclean, entao o conserto se perderia no clone. Ligar as duas bibliotecas
# aqui resolve de forma versionada: os include dirs publicos de "driver"
# passam a ser visiveis para o componente mpu6050.
idf_component_get_property(mpu6050_lib espressif__mpu6050 COMPONENT_LIB)
idf_component_get_property(driver_lib  driver            COMPONENT_LIB)
target_link_libraries(${mpu6050_lib} PRIVATE ${driver_lib})
