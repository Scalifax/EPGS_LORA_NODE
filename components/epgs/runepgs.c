/**
 * Programa de Testes do EPGS
 */

#include "epgs_wrapper.h"
#include "Common/ng_hash_table.h"
#include <stdio.h>
#include <stdlib.h>
#include "runepgs.h"
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/pcnt.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "OLED.h"

// Definição dos pinos do Oled
const gpio_num_t SDA_PIN = GPIO_NUM_4;
const gpio_num_t SCL_PIN = GPIO_NUM_15;
const gpio_num_t RESET_PIN = GPIO_NUM_16;

pcnt_unit_t PCNT_UNIT = PCNT_UNIT_0; // Unidade PCNT (counter) a ser usada

SemaphoreHandle_t xSemaphore = NULL;
NgEPGS *tagNgEPGS = NULL;
int dataPSize = 0;
TaskHandle_t task_tx_handle = NULL;
TaskHandle_t task_rx_handle = NULL;
int Count = 0;
char *dataP = NULL;

uint16_t temperatura = 9999;
uint16_t umidade = 9999;
uint16_t pressao = 9999;
uint16_t uv = 9999;
uint16_t velocidade_vento = 9999;
uint16_t inclinacao_vento = 9999;
uint16_t radiacao_solar = 9999;

const gpio_num_t PULSE_INPUT_PIN = GPIO_NUM_23;  // Pino do sensor de chuva
const gpio_num_t SWITCH_PIN = GPIO_NUM_22;       // Pino do botão

uint8_t seq = 0;
uint8_t flagstart = 0;
uint8_t TXPermission = 0;
uint8_t cont = 0;

OLED* oled;

const char ucIdentify[] = {"e0:e2:e6:00:71:0c"}; // MAC address from the GW_Station
const char ucHID[] = {"12345"};
const char ucSOID[] = {"NG_SO"};
const char ucName[] = {"Measures_B_"};
const char ucPID[] = {"4321"};
const char cTemp[] = {"{ Temperature_B_"};

const char ucStack[] = {"Wi-Fi"};
const char ucInterface[] = {"eth1"};
const char cSensorType[] = {"Temperature"};

static void IRAM_ATTR pcnt_intr_handler(void *arg)
{
    uint32_t status = 0;
    pcnt_get_event_status(PCNT_UNIT, &status);

    if (status & PCNT_EVT_H_LIM) {
        // Evento de limite superior alcançado
        int16_t count = 0;
        pcnt_get_counter_value(PCNT_UNIT, &count);
        arg += count;
		
			//pulse_count += count;

        // Limpa o contador após atingir o limite
        pcnt_counter_clear(PCNT_UNIT);
    }
}

void startepgs()
{

    /*Init epgs*/
    initEPGS(&tagNgEPGS);

    setHwConfigurations(&tagNgEPGS, &ucHID, &ucSOID, &ucStack, &ucInterface, &ucIdentify);


   	// Configuração do pino de entrada de pulso com pull-down
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PULSE_INPUT_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);


	// Configuração do PCNT
    pcnt_config_t pcnt_config = {
        .pulse_gpio_num = PULSE_INPUT_PIN,
        .ctrl_gpio_num = PCNT_PIN_NOT_USED,
        .channel = PCNT_CHANNEL_0,
        .unit = PCNT_UNIT,
        .pos_mode = PCNT_COUNT_INC,   // Incrementa no flanco de subida
        .neg_mode = PCNT_COUNT_DIS,   // Não conta no flanco de descida
        .lctrl_mode = PCNT_MODE_KEEP, // Não altera o modo de contagem
        .hctrl_mode = PCNT_MODE_KEEP, // Não altera o modo de contagem
        .counter_h_lim = 10000,
        .counter_l_lim = 0,
    };

    // Inicializa a unidade PCNT com as configurações
    pcnt_unit_config(&pcnt_config);

    // Configura o filtro para eliminar ruídos (opcional)
    pcnt_set_filter_value(PCNT_UNIT, 100);
    pcnt_filter_enable(PCNT_UNIT);

    // Habilita o evento de limite superior
    pcnt_event_enable(PCNT_UNIT, PCNT_EVT_H_LIM);

    // Instala o serviço de interrupção do PCNT
    pcnt_isr_service_install(0);
    pcnt_isr_handler_add(PCNT_UNIT, pcnt_intr_handler, NULL);

    // Inicia o contador
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);
    pcnt_counter_resume(PCNT_UNIT);


    // Configuração do pino do botão
    gpio_config_t button_conf = {};
    button_conf.intr_type = GPIO_INTR_DISABLE;  // Sem interrupção para o botão (leitura por polling)
    button_conf.mode = GPIO_MODE_INPUT;
    button_conf.pin_bit_mask = (1ULL << SWITCH_PIN);
    button_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    button_conf.pull_up_en = GPIO_PULLUP_ENABLE; // Ativa o resistor de pull-up interno
    gpio_config(&button_conf);

	//Inicia o OLED
	oled = OLED_create(128, 64, SDA_PIN, SCL_PIN, RESET_PIN);
	
	if (oled != NULL) {
        printf("Oled iniciado com sucesso! \n");
		OLED_setFont(oled, ArialMT_Plain_16); //Define o tamanho da fonte
		OLED_clear(oled); //Limpa o buffer do display
    }
}

void runepgs(void)
{
	processLoop(&tagNgEPGS);
}

void lora_rx_mode(void)
{                            

	if( xSemaphore != NULL )
	{
		// See if we can obtain the semaphore.  If the semaphore is not available
		// wait 10 ticks to see if it becomes free.
		if( xSemaphoreTake( xSemaphore, portMAX_DELAY ) == pdTRUE )
		{
			// We were able to obtain the semaphore and can now access the
			// shared resource.
			lora_receive();    // put into receive mode
			// We have finished accessing the shared resource.  Release the
			// semaphore.
			xSemaphoreGive( xSemaphore );
		}
		else
		{
			// We could not obtain the semaphore and can therefore not access
			// the shared resource safely.
		}
	}
}

int lora_rxed(void)
{
	int ans=0;
	if( xSemaphore != NULL )
	{
		// See if we can obtain the semaphore.  If the semaphore is not available
		// wait 10 ticks to see if it becomes free.
		if( xSemaphoreTake( xSemaphore, portMAX_DELAY ) == pdTRUE )
		{
			// We were able to obtain the semaphore and can now access the
			// shared resource.
			ans = lora_received();	//verify if has received msg
			// We have finished accessing the shared resource.  Release the
			// semaphore.
			xSemaphoreGive( xSemaphore );
		}
		else
		{
			// We could not obtain the semaphore and can therefore not access
			// the shared resource safely.
		}
	}
	return ans;
}

int lora_rx_msg(uint8_t *buf, int size)
{
	int ans=0;
	if( xSemaphore != NULL )
	{
		// See if we can obtain the semaphore.  If the semaphore is not available
		// wait 10 ticks to see if it becomes free.
		if( xSemaphoreTake( xSemaphore, portMAX_DELAY ) == pdTRUE )
		{
			// We were able to obtain the semaphore and can now access the
			// shared resource.
			ans = lora_receive_packet(buf, size);	//	Receive the msg
			// We have finished accessing the shared resource.  Release the
			// semaphore.
			xSemaphoreGive( xSemaphore );
		}
		else
		{
			// We could not obtain the semaphore and can therefore not access
			// the shared resource safely.
		}
	}
	return ans;
}

void lora_tx_msg(uint8_t *buf, int size)
{
	if( xSemaphore != NULL )
	{
		// See if we can obtain the semaphore.  If the semaphore is not available
		// wait 10 ticks to see if it becomes free.
		if( xSemaphoreTake( xSemaphore, portMAX_DELAY ) == pdTRUE )
		{
			// We were able to obtain the semaphore and can now access the
			// shared resource.
			lora_send_packet(buf, size);
			// We have finished accessing the shared resource.  Release the
			// semaphore.
			xSemaphoreGive( xSemaphore );
		}
		else
		{
			// We could not obtain the semaphore and can therefore not access
			// the shared resource safely.
		}
	}
}