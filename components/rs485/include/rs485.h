#ifndef __RS485__
#define __RS485__

#include <stdio.h>

uint16_t calculateCRC(uint8_t *data, uint8_t length);
void sendReadCommand(uint8_t device_id, uint16_t start_register, uint8_t num_registers);
void readSensorData(uint8_t device_id, uint16_t register_start, uint8_t num_registers, const char *parameter_name, uint16_t *output_values);
void init_uart_rs485(int rate);

#endif