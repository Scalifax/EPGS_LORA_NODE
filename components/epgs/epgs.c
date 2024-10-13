/*
===============================================================================
 Name        : EPGS.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
 */

#include "epgs.h"
#include "epgs_wrapper.h"
#include "Common/ng_epgs_hash.h"
#include "Controller/epgs_controller.h"
#include "Network/PG.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "runepgs.h"
#include "string.h"
#include "rs485.h"
#include "driver/gpio.h"
#include "freertos/portmacro.h"
#include "OLED.h"

void display_state_simple(OLED* oled_, int n1, int n2, const char* message){
	
	OLED_clear(oled_);
    OLED_drawString(oled_, n1, n2, message, WHITE);
    OLED_sendData(oled_);
}

void display_state_complex(OLED* oled_, int n1, int n2, const char* message1, const char* message2, int space){
	
	OLED_clear(oled_);
    OLED_drawString(oled_, n1, n2, message1, WHITE);
	OLED_drawString(oled_, n1, n2+space, message2, WHITE);
    OLED_sendData(oled_);
}

int initEPGS(NgEPGS **ngEPGS) {

	if(*ngEPGS) {
		return NG_ERROR;
	}

	(*ngEPGS) = (NgEPGS*) ng_malloc(sizeof(NgEPGS));

	(*ngEPGS)->NetInfo = (NgNetInfo*) ng_malloc(sizeof(NgNetInfo));

	(*ngEPGS)->PGCSNetInfo = NULL;

	(*ngEPGS)->PGCSScnIDInfo = NULL;

	(*ngEPGS)->PSSScnIDInfo = NULL;

	(*ngEPGS)->APPScnIDInfo = NULL;

	(*ngEPGS)->ReceivedMsg = NULL;

	(*ngEPGS)->HwDescriptor = (NgHwDescriptor*) ng_malloc (sizeof(NgHwDescriptor));
	(*ngEPGS)->HwDescriptor->keyWords = NULL;
	(*ngEPGS)->HwDescriptor->keyWordsCounter = 0;

	(*ngEPGS)->HwDescriptor->sensorFeatureName = NULL;
	(*ngEPGS)->HwDescriptor->sensorFeatureValue = NULL;
	(*ngEPGS)->HwDescriptor->featureCounter = 0;

	addKeyWords(ngEPGS, "EPGS");
	addKeyWords(ngEPGS, "Embedded_Proxy_Gateway_Service");

	(*ngEPGS)->MessageCounter = 0;
	(*ngEPGS)->ngState = HELLO;
	(*ngEPGS)->HelloCounter = 0;

	(*ngEPGS)->pubDataFileName = NULL;
	(*ngEPGS)->pubDataSize = 0;
	(*ngEPGS)->pubData = NULL;

	(*ngEPGS)->key = NULL;

	return NG_OK;
}

int destroyEPGS(NgEPGS **ngEPGS) {
	destroy_NgEPGS(ngEPGS);
	return 1;
}

int setHwConfigurations(NgEPGS **ngEPGS, const char* hid, const char* soid, const char* Stack, const char* Interface, const char* Identifier) {
	if(!(*ngEPGS)->NetInfo) {
			return NG_ERROR;
		}

		ng_strcpy((*ngEPGS)->NetInfo->HID, hid);
		ng_strcpy((*ngEPGS)->NetInfo->SOID, soid);

		(*ngEPGS)->NetInfo->Stack = (char*) ng_malloc(sizeof(char) * (ng_strlen(Stack) + 1));
		ng_strcpy((*ngEPGS)->NetInfo->Stack, Stack);

		(*ngEPGS)->NetInfo->Interface = (char*) ng_malloc(sizeof(char) * (ng_strlen(Interface) + 1));
		ng_strcpy((*ngEPGS)->NetInfo->Interface, Interface);

		(*ngEPGS)->NetInfo->Identifier = (char*) ng_malloc(sizeof(char) * (ng_strlen(Identifier) + 1));
		ng_strcpy((*ngEPGS)->NetInfo->Identifier, Identifier);

		return NG_OK;
}

int processLoop(NgEPGS **ngEPGS) {

	char ucFile[256];
	char ucBufferName[200];
	switch((*ngEPGS)->ngState) {
		case HELLO:

			display_state_simple(oled, 0, 0, "HELLO");

			if((*ngEPGS)->HelloCounter >= 5 ) {
				(*ngEPGS)->ngState = WAIT_HELLO_PGCS;
				(*ngEPGS)->HelloCounter = 0;
			}
			(*ngEPGS)->HelloCounter++;

			RunHello((*ngEPGS));
			if((*ngEPGS)->PGCSNetInfo && (*ngEPGS)->PGCSScnIDInfo && (*ngEPGS)->PSSScnIDInfo) {
				(*ngEPGS)->ngState = EXPOSITION;
			}

			break;

		case WAIT_HELLO_PGCS:
			//Moved to the HELLO state, causing the exposition when reaches a PGCS hello

			display_state_simple(oled, 0, 0, "HELLO PGCS");

			if((*ngEPGS)->PGCSNetInfo && (*ngEPGS)->PGCSScnIDInfo && (*ngEPGS)->PSSScnIDInfo) {
				(*ngEPGS)->ngState = EXPOSITION;
			}

			break;

		case EXPOSITION:

			display_state_simple(oled, 0, 0, "EXPOSITION");

			if(RunExposition((*ngEPGS)) == NG_OK) {
				(*ngEPGS)->ngState = SERVICE_OFFER;
				cont = 0;
			}

			break;

		case SERVICE_OFFER:

			display_state_complex(oled, 0, 0, "SERVICE", " OFFER", 16);

			while((*ngEPGS)->ngState == SERVICE_OFFER){
				if(RunPubServiceOffer((*ngEPGS)) == NG_OK) {
					printf("\nWaiting Acceptance\n");
					vTaskDelay(15000/portTICK_PERIOD_MS);
					printf("\nSending another offer\n");
				}
			}

			break;

		case SUBSCRIBE_SERVICE_ACCEPTANCE:

			display_state_simple(oled, 0, 0, "SUBSCRIBE");

			if (cont < 10) {
				if(RunSubscribeServiceAcceptance((*ngEPGS)) == NG_OK) {
					(*ngEPGS)->ngState = WAIT_SERVICE_ACCEPTANCE_DELIVERY;
					cont++;
				}
			}

			break;

		case WAIT_SERVICE_ACCEPTANCE_DELIVERY:

			display_state_complex(oled, 0, 0, "WAIT SUB ", "ACCEPTANCE", 16);

			if (cont < 10) {
				(*ngEPGS)->ngState = SUBSCRIBE_SERVICE_ACCEPTANCE;
			}

			//NG ONLY LEAVE THIS STATE WHEN IT RECEIVES SERVICE ACCEPTANCE DELIVERY MESSAGE WITH NO ERRORS

			break;

		case PUB_DATA:

			if(cont == 10){
				display_state_simple(oled, 0, 0, "PUB DATA");
				OLED_destroy(oled);
				printf("Oled destuído com sucesso! \n");

				vTaskDelay(1000/portTICK_PERIOD_MS);

				init_uart_rs485(9600);
				cont++;
			}

			uint16_t arr[3] = {0,0,0};

			// Leitura dos sensores
			readSensorData(0x01, 0x0000, 0x0001, "Piranometro", arr);
			radiacao_solar = arr[0];
			printf("Radiação solar lida com valor: %d \n", radiacao_solar);

			readSensorData(0x02, 0x0001, 0x0003, "Anemometro", arr);
			inclinacao_vento = arr[0];
			velocidade_vento = arr[1];
			printf("Inclinação do vento lida com valor: %d \n", inclinacao_vento);
			printf("Velocidade do vento lida com valor: %d \n", velocidade_vento);

			readSensorData(0x03, 0x0000, 0x0003, "TUP", arr);
			temperatura = arr[0];
			umidade = arr[1];
			pressao = arr[2];
			printf("Temperatura lida com valor: %d \n", temperatura);
			printf("Umidade lida com valor: %d \n", umidade);
			printf("Pressão lida com valor: %d \n", pressao);

			readSensorData(0x0B, 0x0000, 0x0001, "Uv", arr);
			uv = arr[0];
			printf("Radiação UV lida com valor: %d \n", uv);

			int button_state = gpio_get_level(SWITCH_PIN);
			printf("Porta lida com valor [0-Fechada]: %d \n", button_state);
			
			int16_t pluv_count = 0;
        	pcnt_get_counter_value(PCNT_UNIT, &pluv_count);
        	printf("Pluviomentro lido com valor: %d \n", pluv_count);
			
			// Mensagem transmitida pelo EPGS
            sprintf(ucFile, "{ \"Temperatura\": %d, \"Umidade\": %d, \"Pressao\": %d, \"Rad_solar\": %d, \"Uv\": %d, \"Vel_vento\": %d, \"Inc_vento\": %d, \"Pluv\": %d, \"Porta\": %d}", 
			temperatura, umidade, pressao, radiacao_solar, uv, velocidade_vento, inclinacao_vento, pluv_count, button_state);
			
			pcnt_counter_clear(PCNT_UNIT);

            sprintf(ucBufferName, "Measures_%s_%d.json", (*ngEPGS)->NetInfo->HID, Count);
			
			if(umidade == 0 && pressao == 0){
				printf("Caso de valores inválidos");
			}
			else{
            	setDataToPub(ngEPGS, ucBufferName, ucFile, strlen(ucFile));
			}

            Count++;

			// Wait to make the next transmission
			//printf("\nWaiting... next read in 30\ns");
			//vTaskDelay(30000/portTICK_PERIOD_MS);

            break;
	}

	return NG_OK;
}

int newReceivedMessage(NgEPGS **ngEPGS, const char *message, int msgSize) {
	int result = newMessageReceived(ngEPGS, message, msgSize);

	if(result != NG_PROCESSING) {
		//Let the periodic processLoop in runEPGS handle that bro
		destroy_NgReceivedMsg(&((*ngEPGS)->ReceivedMsg));
	}

	return result;
}

int addKeyWords(NgEPGS **ngEPGS, const char* key) {
	(*ngEPGS)->HwDescriptor->keyWords = (char**) ng_realloc((*ngEPGS)->HwDescriptor->keyWords, ((*ngEPGS)->HwDescriptor->keyWordsCounter + 1) * sizeof(char*));
	(*ngEPGS)->HwDescriptor->keyWords[(*ngEPGS)->HwDescriptor->keyWordsCounter] = (char*)ng_malloc(sizeof(char) * (ng_strlen(key) + 1));
	ng_strcpy((*ngEPGS)->HwDescriptor->keyWords[(*ngEPGS)->HwDescriptor->keyWordsCounter], key);

	(*ngEPGS)->HwDescriptor->keyWordsCounter++;

	return NG_OK;
}


int addHwSensorFeature(NgEPGS **ngEPGS, const char* name, const char* value) {
	(*ngEPGS)->HwDescriptor->sensorFeatureName = (char**) ng_realloc((*ngEPGS)->HwDescriptor->sensorFeatureName, ((*ngEPGS)->HwDescriptor->featureCounter + 1) * sizeof(char*));
	(*ngEPGS)->HwDescriptor->sensorFeatureName[(*ngEPGS)->HwDescriptor->featureCounter] = (char*)ng_malloc(sizeof(char) * (ng_strlen(name) + 1));
	ng_strcpy((*ngEPGS)->HwDescriptor->sensorFeatureName[(*ngEPGS)->HwDescriptor->featureCounter], name);

	(*ngEPGS)->HwDescriptor->sensorFeatureValue = (char**) ng_realloc((*ngEPGS)->HwDescriptor->sensorFeatureValue, ((*ngEPGS)->HwDescriptor->featureCounter + 1) * sizeof(char*));
	(*ngEPGS)->HwDescriptor->sensorFeatureValue[(*ngEPGS)->HwDescriptor->featureCounter] = (char*)ng_malloc(sizeof(char) * (ng_strlen(value) + 1));
	ng_strcpy((*ngEPGS)->HwDescriptor->sensorFeatureValue[(*ngEPGS)->HwDescriptor->featureCounter], value);

	(*ngEPGS)->HwDescriptor->featureCounter++;

	return NG_OK;
}


int setDataToPub(NgEPGS **ngEPGS, const char* fileName, const char* data, int dataLength) {

	if(!fileName || !data || dataLength <= 0) {
		return NG_ERROR;
	}

	(*ngEPGS)->pubDataFileName = (char*) ng_realloc((*ngEPGS)->pubDataFileName, (ng_strlen(fileName) + 1) * sizeof(char));
	(*ngEPGS)->pubData = (char*) ng_realloc((*ngEPGS)->pubData, (dataLength) * sizeof(char));

	ng_strcpy((*ngEPGS)->pubDataFileName, fileName);

	int i = 0;
	for(i = 0; i<dataLength; i++) {
		(*ngEPGS)->pubData[i] = data[i];
	}

	(*ngEPGS)->pubDataSize = dataLength;

	if((*ngEPGS)->pubData) {
		RunPublishData((*ngEPGS));
	}

	return NG_OK;
}