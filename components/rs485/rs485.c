#include "driver/uart.h"
#include "driver/gpio.h"
#include <stdio.h>

// Configuração dos pinos da UART e controle RS485
#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)
#define RTS_GPIO (GPIO_NUM_2)  // Pino RTS para controle de direção RS485

#define UART_PORT UART_NUM_2  // Utilizando a UART2 do ESP32

#define INFO_LED GPIO_NUM_25 // Led para informar atividade de leitura

// Função para calcular CRC-16 (Modbus)
uint16_t calculateCRC(uint8_t *data, uint8_t length) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// Função para enviar o comando de leitura
void sendReadCommand(uint8_t device_id, uint16_t start_register, uint8_t num_registers) {
    uint8_t command[8];
    command[0] = device_id;                     // ID do escravo
    command[1] = 0x03;                          // Função de leitura (0x03)
    command[2] = (start_register >> 8) & 0xFF;  // Byte alto do registrador inicial
    command[3] = start_register & 0xFF;         // Byte baixo do registrador inicial
    command[4] = 0x00;                          // Byte alto do número de registradores
    command[5] = num_registers;                 // Byte baixo do número de registradores
    uint16_t crc = calculateCRC(command, 6);    // Calcula o CRC-16
    command[6] = crc & 0xFF;                    // CRC byte baixo
    command[7] = (crc >> 8) & 0xFF;             // CRC byte alto

    // Enviar o comando pela UART
    uart_write_bytes(UART_PORT, (const char *)command, sizeof(command));
    uart_wait_tx_done(UART_PORT, 100);  // Aguarda envio completo
}

// Função para ler dados do sensor escravo
void readSensorData(uint8_t device_id, uint16_t register_start, uint8_t num_registers, const char *parameter_name, uint16_t *output_values) {

    sendReadCommand(device_id, register_start, num_registers);

    // Definir o tempo máximo de espera (35 segundos)
    const int timeout_ms = 1500; // 1000ms = 1s
    const int poll_interval_ms = 100; // Intervalo de verificação de 100 ms
    int waited_ms = 0;
    
    uint8_t data[256];
    int length = 0;
    int read_bytes = 0;

    while (waited_ms < timeout_ms) {
    
    gpio_set_level(INFO_LED, 1);

        // Verificar se há dados disponíveis no buffer UART
        ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, (size_t *)&length));

        if (length > 0) {

            // Ler os dados recebidos
            read_bytes = uart_read_bytes(UART_PORT, data, length, pdMS_TO_TICKS(100));

            // printf("%s (Registrador 0x%04X) - ID do Escravo: 0x%02X\n", parameter_name, register_start, device_id);

            /* Exibir todos os bytes recebidos
            printf("Dados recebidos (%d bytes): ", read_bytes);
            for (int i = 0; i < read_bytes; i++) {
                printf("0x%02X ", data[i]);
            }
            printf("\n");
            */

            // Verificar se a resposta é válida
            if (read_bytes >= 5) { // mínimo de bytes para uma resposta válida
                // Verificar endereço do escravo e código de função
                if (data[0] == device_id && data[1] == 0x03) {
                    // Verificar CRC
                    uint16_t received_crc = (data[read_bytes - 1] << 8) | data[read_bytes - 2];
                    uint16_t calculated_crc = calculateCRC(data, read_bytes - 2);
                    if (received_crc == calculated_crc) {
                        
                        // Extrair os dados
                        uint8_t byte_count = data[2];
                        if (byte_count == (num_registers * 2)) {
                            // Processar os dados recebidos
                            for (int i = 0; i < num_registers; i++) {
                                int data_index = 3 + i * 2;
                                output_values[i] = (data[data_index] << 8) | data[data_index + 1];
                            }

                            // Exibir os valores lidos
                            /*printf("Valores lidos:\n");
                            for (int i = 0; i < num_registers; i++) {
                                printf("Registrador %d: %d\n", register_start + i, output_values[i]);
                            }
                            */

                            printf("\nLeitura do sensor bem sucedida: \n");
                            gpio_set_level(INFO_LED, 0);
                        }
                        else {
                            printf("Contagem de bytes não corresponde ao número de registradores solicitados.\n");
                        }
                    }
                    else {
                        printf("CRC inválido. Recebido: 0x%04X, Calculado: 0x%04X\n", received_crc, calculated_crc);
                    }
                }
                else {
                    printf("Resposta inválida. Endereço ou código de função incorretos.\n");
                }
            }
            else {
                printf("Resposta muito curta para ser válida.\n");
            }

            // Saia do loop após processar a resposta
            break;
        }

        // Aguardar antes de verificar novamente
        vTaskDelay(pdMS_TO_TICKS(poll_interval_ms));
        waited_ms += poll_interval_ms;
    }

    if (read_bytes == 0) {
        //printf("Nenhuma resposta recebida para %s - ID do Escravo: 0x%02X após %d ms\n", parameter_name, device_id, timeout_ms);
        printf("Nenhuma resposta recebida para %s", parameter_name);
    }
}

// Configuração inicial da UART e modo RS485
void init_uart_rs485(int rate) {
    uart_config_t uart_config = {
        .baud_rate = rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    gpio_pad_select_gpio(INFO_LED);
    gpio_set_direction(INFO_LED, GPIO_MODE_OUTPUT);

    // Configura a UART com os pinos TX, RX, RTS e modo Half-Duplex RS485
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, RTS_GPIO, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 1024 * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_set_mode(UART_PORT, UART_MODE_RS485_HALF_DUPLEX));  // Ativa RS485 Half-Duplex
}