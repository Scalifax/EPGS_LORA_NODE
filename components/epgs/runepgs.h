/*
 * runepgs.h
 *
 *  Created on: May 19, 2018
 *      Author: Unknown
 */

#ifndef RUNEPGS_H_
#define RUNEPGS_H_

#include <stdio.h>
#include "epgs.h"
#include "epgs_wrapper.h"
#include "DataStructures/epgs_structures.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lora.h"
#include "OLED.h"
#include "driver/pcnt.h"
#include "driver/gpio.h"

extern SemaphoreHandle_t xSemaphore;

enum CHPERMISSIONS
{
	FREE = 0,
	SENDING,
	RECEIVING,
};

extern NgEPGS *tagNgEPGS;

extern int dataPSize;

extern TaskHandle_t task_tx_handle, task_rx_handle;

extern int Count;

extern char *dataP;

void startepgs(void);

void runepgs(void);

void lora_rx_mode(void);

int lora_rxed(void);

int lora_rx_msg(uint8_t *buf, int size);

void lora_tx_msg(uint8_t *buf, int size);

extern pcnt_unit_t PCNT_UNIT;

extern uint16_t temperatura;
extern uint16_t umidade;
extern uint16_t pressao;
extern uint16_t uv;
extern uint16_t velocidade_vento;
extern uint16_t inclinacao_vento;
extern uint16_t radiacao_solar;

extern const gpio_num_t PULSE_INPUT_PIN;
extern const gpio_num_t SWITCH_PIN;

//extern volatile uint16_t pulse_count;

extern uint8_t seq;

extern uint8_t flagstart;

extern uint8_t TXPermission;

extern uint8_t cont;

extern OLED* oled;
extern const gpio_num_t SDA_PIN;
extern const gpio_num_t SCL_PIN;
extern const gpio_num_t RESET_PIN;

#endif /* RUNEPGS_H_ */