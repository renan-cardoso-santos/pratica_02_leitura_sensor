/*
 * Pratica 2/6 - Leitura de sensor
 * UC: IA Embarcada e Modelos Compactos - UniSENAI
 *
 * Le aceleracao, giroscopio e temperatura de um MPU6050 via I2C
 * e imprime os valores no monitor serial a cada 500 ms.
 *
 * Alvo: ESP32-S3-DevKitC-1 (simulado no Wokwi)
 * Sensor: MPU6050 (biblioteca espressif/mpu6050 v1.2.1 do ESP Component Registry)
 *
 * Ligacao (ver diagram.json):
 *   MPU6050 VCC -> ESP32-S3 3V3
 *   MPU6050 GND -> ESP32-S3 GND
 *   MPU6050 SDA -> ESP32-S3 GPIO8
 *   MPU6050 SCL -> ESP32-S3 GPIO9
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "driver/gpio.h"

#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "mpu6050.h"

/* TAG identifica a origem da mensagem no monitor serial.
 * Equivale ao "name" de um logger do Python. */
static const char *TAG = "pratica02_mpu6050";

/* Pinos e frequencias em macros: nenhum numero magico espalhado pelo codigo.
 * Trocar de pino vira uma edicao em um lugar so. */
#define I2C_MASTER_SDA_IO    GPIO_NUM_8
#define I2C_MASTER_SCL_IO    GPIO_NUM_9
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000      /* 100 kHz - modo standard do I2C */

#define PERIODO_LEITURA_MS   500

/*
 * Configura o ESP32-S3 como mestre do barramento I2C.
 *
 * O I2C e um barramento de dreno aberto: os dispositivos so conseguem
 * puxar a linha para 0 V. Quem devolve a linha para 3,3 V sao os resistores
 * de pull-up - por isso os dois `*_pullup_en` abaixo. Sem pull-up, o
 * barramento nunca sobe e nenhuma leitura funciona.
 */
static esp_err_t i2c_bus_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(I2C_MASTER_NUM, &conf), TAG,
                        "i2c_param_config falhou");

    /* rx_buf e tx_buf sao 0 porque o mestre nao precisa de buffer de slave */
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

/*
 * app_main e o ponto de entrada da aplicacao no ESP-IDF.
 * Equivale ao `if __name__ == "__main__":` do Python - o FreeRTOS cria
 * uma task e chama esta funcao dentro dela.
 */
void app_main(void)
{
    /* ESP_ERROR_CHECK aborta o boot se falhar. Correto no setup:
     * sem barramento I2C nao ha nada a fazer. */
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_LOGI(TAG, "Barramento I2C inicializado (SDA=GPIO%d, SCL=GPIO%d, %d Hz)",
             I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);

    /* 0x68 = endereco do MPU6050 com o pino AD0 em nivel baixo (padrao).
     * Com AD0 em nivel alto seria 0x69 (MPU6050_I2C_ADDRESS_1). */
    mpu6050_handle_t mpu = mpu6050_create(I2C_MASTER_NUM, MPU6050_I2C_ADDRESS);
    if (mpu == NULL) {
        ESP_LOGE(TAG, "Falha ao criar o handle do MPU6050");
        return;
    }

    /* Fundo de escala: +/-4 g no acelerometro, +/-500 graus/s no giroscopio.
     * Fundo de escala menor = mais resolucao, porem satura antes. */
    ESP_ERROR_CHECK(mpu6050_config(mpu, ACCE_FS_4G, GYRO_FS_500DPS));

    /* O MPU6050 acorda em modo sleep: sem isso, todas as leituras vem zeradas. */
    ESP_ERROR_CHECK(mpu6050_wake_up(mpu));

    /* WHO_AM_I: registrador de identidade do chip. Se responder 0x68,
     * o barramento, o endereco e a fiacao estao corretos. E o primeiro
     * teste a fazer em qualquer sensor I2C. */
    uint8_t who_am_i = 0;
    ESP_ERROR_CHECK(mpu6050_get_deviceid(mpu, &who_am_i));
    ESP_LOGI(TAG, "MPU6050 WHO_AM_I: 0x%02x (esperado: 0x68)", who_am_i);

    ESP_LOGI(TAG, "Iniciando leituras a cada %d ms...", PERIODO_LEITURA_MS);

    while (1) {
        mpu6050_acce_value_t acce = {0};
        mpu6050_gyro_value_t gyro = {0};
        mpu6050_temp_value_t temp = {0};

        /* No loop a falha e tratada com aviso, nao com abort:
         * uma leitura ruim nao pode derrubar o dispositivo. */
        if (mpu6050_get_acce(mpu, &acce) != ESP_OK) {
            ESP_LOGW(TAG, "Falha ao ler o acelerometro");
        } else if (mpu6050_get_gyro(mpu, &gyro) != ESP_OK) {
            ESP_LOGW(TAG, "Falha ao ler o giroscopio");
        } else if (mpu6050_get_temp(mpu, &temp) != ESP_OK) {
            ESP_LOGW(TAG, "Falha ao ler a temperatura");
        } else {
            ESP_LOGI(TAG, "ACC[g]: x=%6.2f y=%6.2f z=%6.2f | "
                          "GYRO[dps]: x=%7.2f y=%7.2f z=%7.2f | "
                          "TEMP[C]: %5.2f",
                     acce.acce_x, acce.acce_y, acce.acce_z,
                     gyro.gyro_x, gyro.gyro_y, gyro.gyro_z,
                     temp.temp);
        }

        /* vTaskDelay devolve a CPU ao scheduler do FreeRTOS durante a espera.
         * Um busy-wait aqui travaria as outras tasks e acionaria o watchdog. */
        vTaskDelay(pdMS_TO_TICKS(PERIODO_LEITURA_MS));
    }

    /* Inalcancavel neste exemplo, mas e a forma correta de liberar o handle: */
    /* mpu6050_delete(mpu); */
}
