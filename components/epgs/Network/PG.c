#include "PG.h"
#include "epgs_wrapper.h"
#include "ng_util.h"
#include "epgs_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include "runepgs.h"
#include <string.h>

//#include "vcom.h"
//#include "cmsis_os.h"

extern SemaphoreHandle_t xSemaphore;
/**
 * Fun��o para converter uma string MAC (11:22:33:44:55:66 com 17 caracteres) em
 * um vetor de bytes (6 bytes)
 */
void convertStrToMAC(char* macSTR, char** macBytes) {
	
	char *destinationMAC = (char*) ng_malloc(sizeof(char) * 6);
	
	char *end_str;
	char* stringDestMAC = (char*) ng_calloc(sizeof(char), ng_strlen(macSTR)+1);
	ng_strcpy(stringDestMAC, macSTR);
	
	char* token = strtok_r(stringDestMAC, ":", &end_str);			
	long value = ng_strtoul(token, NULL, 16);
	destinationMAC[0] = value;
	
	token = strtok_r(NULL, ":", &end_str);
	value = ng_strtoul(token, NULL, 16);
	destinationMAC[1] = value;
	
	token = strtok_r(NULL, ":", &end_str);
	value = ng_strtoul(token, NULL, 16);
	destinationMAC[2] = value;
	
	token = strtok_r(NULL, ":", &end_str);
	value = ng_strtoul(token, NULL, 16);
	destinationMAC[3] = value;
	
	token = strtok_r(NULL, ":", &end_str);
	value = ng_strtoul(token, NULL, 16);
	destinationMAC[4] = value;
	
	token = strtok_r(NULL, ":", &end_str);
	value = ng_strtoul(token, NULL, 16);
	destinationMAC[5] = value;
	
	ng_free(stringDestMAC);
	//ng_free(token);
	//ng_free(end_str);
	*macBytes = destinationMAC;
}

/**
 * Função de preparação e envio da mensagem NovaGenesis via Ethernet ou WiFi
 */
int sendNGMessage(NgEPGS* ngEPGS, NgMessage* message, bool isBroadcast) {


	if(!message || !ngEPGS || !ngEPGS->NetInfo || !ngEPGS->NetInfo->Identifier) {
		return NG_ERROR;
	}
	
	char *destinationMAC;
	if(!isBroadcast) {
		if(ngEPGS->PGCSNetInfo)
		{
			if(ngEPGS->PGCSNetInfo->Identifier) {
				convertStrToMAC(ngEPGS->PGCSNetInfo->Identifier, &destinationMAC);
			} 
			else {
				return NG_ERROR;
			}
		}
		else {
			return NG_ERROR;
		}
				
	} else {
		destinationMAC = (char*) ng_malloc(sizeof(char) * 6);
		destinationMAC[0] = 0xFF;
		destinationMAC[1] = 0xFF;
		destinationMAC[2] = 0xFF;
		destinationMAC[3] = 0xFF;
		destinationMAC[4] = 0xFF;
		destinationMAC[5] = 0xFF;
	}
	char *sourceMAC;
	convertStrToMAC(ngEPGS->NetInfo->Identifier, &sourceMAC);
	
	long long payloadSize = HEADER_SIZE_FIELD_SIZE + message->MessageSize;

	char* sdu = (char*) ng_malloc(sizeof(char) * payloadSize);

	int i = 0;
	for(i = 0; i < HEADER_SIZE_FIELD_SIZE; i++) {
		sdu[i]=(message->MessageSize >> (8*(7-i))) & 0xff;
	}

	for(i = 0; i < message->MessageSize; i++) {
		sdu[HEADER_SIZE_FIELD_SIZE + i] = message->Msg[i];
	}

	int numberOfMsgs = getNumberOfMessages(message->MessageSize);
	unsigned MessageNumber = ng_rand();
	MessageNumber = (MessageNumber << 16) | ng_rand();

	unsigned int SequenceNumber = 0;


//	printf("RX TASK DISABLED\n");
//	vTaskSuspend( task_rx_handle ); //The channel is busy, RX disabled
	int bytesSent = 0;
	TickType_t msg_tick_time = xTaskGetTickCount();
	int j = 0;
	for(j = 0; j < numberOfMsgs; j++) {
		char *mtu = (char*) ng_calloc(sizeof(char), DEFAULT_MTU);

		int index = 0;
		for(i = 0; i < ETHERNET_MAC_ADDR_FIELD_SIZE; i++) {
			mtu[index + i]=destinationMAC[i];
			mtu[index + i + ETHERNET_MAC_ADDR_FIELD_SIZE]=sourceMAC[i];
		}
		index += ETHERNET_MAC_ADDR_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE; // Destination + Source

		mtu[index] = NG_TYPE_MSB;
		mtu[index + 1] = NG_TYPE_LSB;
		index += ETHERNET_TYPE_FIELD_SIZE;

		unsigned char *HeaderSegmentationField = (unsigned char*)ng_malloc(sizeof(unsigned char)*8);
		HeaderSegmentationField[0]=(MessageNumber >> (8*3)) & 0xff;
		HeaderSegmentationField[1]=(MessageNumber >> (8*2)) & 0xff;
		HeaderSegmentationField[2]=(MessageNumber >> (8*1)) & 0xff;
		HeaderSegmentationField[3]=(MessageNumber >> (8*0)) & 0xff;
		HeaderSegmentationField[4]=(SequenceNumber >> (8*3)) & 0xff;
		HeaderSegmentationField[5]=(SequenceNumber >> (8*2)) & 0xff;
		HeaderSegmentationField[6]=(SequenceNumber >> (8*1)) & 0xff;
		HeaderSegmentationField[7]=(SequenceNumber >> (8*0)) & 0xff;

		for(i = 0; i< HEADER_SEGMENTATION_FIELD_SIZE; i++) {
			mtu[index + i]=HeaderSegmentationField[i];
		}
		ng_free(HeaderSegmentationField);
		SequenceNumber++;
		index += HEADER_SEGMENTATION_FIELD_SIZE;

		for(; index < DEFAULT_MTU && bytesSent < payloadSize; index++) {
			mtu[index]=sdu[bytesSent];
			bytesSent++;
		}
//		while(!(TXPermission == FREE || TXPermission == SENDING)) {	//Wait until the channel is free or txed recently
//			vTaskDelay(1);
//		}
		TXPermission = SENDING;
//		printf("TXPermission = NEEDTX\n");
//		TXPermission = NEEDTX;				//Waiting for a permission
//		while(TXPermission != TXGRANTED) {	//TXPermission == TXGRANTED -> Permission granted
//			vTaskDelay(1);
//		}
//		vTaskDelay((200 + (ng_rand()%300))/ portTICK_PERIOD_MS);
		vTaskDelayUntil( &msg_tick_time, 1000/portTICK_PERIOD_MS );
		printf("\n->->->->->->->->->->->->->->->->->->->->->->->->->->->->->->");
		printf("\nNG FRAG SENT / TICK->%lu / SIZE->%u / ID->%08X / SEQ->%u / NG Adaptation Header is:\n", xTaskGetTickCount()*portTICK_PERIOD_MS, index, MessageNumber, SequenceNumber-1);
		if(SequenceNumber-1==0)
		{
			for(int k = 0; k < 30; k++)
			{
				printf("%02X ", mtu[k]  & 0xFF );
			}
		}
		else
		{
			for(int k = 0; k < 22; k++)
			{
				printf("%02X ", mtu[k]  & 0xFF );
			}
		}
		printf("\n->->->->->->->->->->->->->->->->->->->->->->->->->->->->->->");
		ng_LoRaSendData(mtu, index);// NovaGenesis
		ng_free(mtu);
	}
	//printf("\n\n\n->->->->->->->->->->->->->->->->->->->->->->->->->->->->->->");
	//printf("\nNOVAGENESIS MSG SENT:\n");
	//for(int contagem = HEADER_SIZE_FIELD_SIZE; contagem<payloadSize; contagem++)
	//{
	//	printf("%c",sdu[contagem]);
	//}
	//printf("->->->->->->->->->->->->->->->->->->->->->->->->->->->->->->\n");
	TXPermission = FREE;				//Release the channel
	ng_free(sdu);
	ng_free(sourceMAC);
	ng_free(destinationMAC);
	
	return NG_OK;
}

int newMessageReceived(struct _ng_epgs **ngEPGS, const char* message, int rcvdMsgSize) {

    // Definição dos tamanhos dos cabeçalhos
    const int ethAddrHeaderSize = ETHERNET_MAC_ADDR_FIELD_SIZE * 2;
    const int totalHeaderSize = ethAddrHeaderSize + ETHERNET_TYPE_FIELD_SIZE + HEADER_SEGMENTATION_FIELD_SIZE;

    // Validação do tamanho da mensagem recebida
    if (rcvdMsgSize < totalHeaderSize) {
        printf("Tamanho da mensagem recebido (%d) é menor que o tamanho do cabeçalho requerido (%d).\n", rcvdMsgSize, totalHeaderSize);
        return NG_ERROR;
    }

    // Verificação do tipo Ethernet. NG é do tipo 0x1234
    if (!(message[ethAddrHeaderSize] == NG_TYPE_MSB && message[ethAddrHeaderSize + 1] == NG_TYPE_LSB)) {
        printf("Tipo de mensagem não é NG.\n");
        return NG_ERROR;
    }

    int index = ethAddrHeaderSize + ETHERNET_TYPE_FIELD_SIZE;
    int i = 0;

    // Extração do ID de sequência da mensagem
    const int segmentationIDSize = HEADER_SEGMENTATION_FIELD_SIZE / 2;
    unsigned int msgSeq = 0;
    for (i = 0; i < segmentationIDSize; i++) {
        msgSeq = (msgSeq << 8) | (unsigned char)message[index + i];
    }
    index += segmentationIDSize;

    // Extração do número do fragmento da mensagem
    const int segmentationCounterSize = HEADER_SEGMENTATION_FIELD_SIZE / 2;
    unsigned int msgNumber = 0;
    for (i = 0; i < segmentationCounterSize; i++) {
        msgNumber = (msgNumber << 8) | (unsigned char)message[index + i];
    }
    index += segmentationCounterSize;

    printf("\n<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-");
    printf("\nNG FRAG RECEIVED / TICK->%lu / SIZE->%d / ID->%08X / SEQ->%u\n", 
        xTaskGetTickCount() * portTICK_PERIOD_MS, rcvdMsgSize, msgSeq, msgNumber);

    // Impressão de bytes da mensagem para depuração
    int bytesToPrint = (message[21] == 0) ? 30 : 22;
    for(int k = 0; k < bytesToPrint && k < rcvdMsgSize; k++) {
        printf("%02X ", (unsigned char)message[k]);
    }
    printf("\n<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-\n");

    // Verificação se o fragmento recebido está na ordem esperada
    if ((*ngEPGS)->ReceivedMsg != NULL && msgNumber != (*ngEPGS)->ReceivedMsg->frames_read) {
        printf("Número de fragmento inesperado (%u). Esperado (%u). Deletando mensagem atual.\n", msgNumber, (*ngEPGS)->ReceivedMsg->frames_read);
        ng_free((*ngEPGS)->ReceivedMsg->buffer);
        ng_free((*ngEPGS)->ReceivedMsg);
        (*ngEPGS)->ReceivedMsg = NULL;
        return NG_ERROR;
    }

    if (msgNumber == 0) {
        // Início de uma nova mensagem
        if ((*ngEPGS)->ReceivedMsg == NULL) {
            // Extração do tamanho total da mensagem
            int msgSize = 0;
            for(i = 0; i < HEADER_SIZE_FIELD_SIZE; i++) {
                msgSize = (msgSize << 8) | (unsigned char)message[index + i];
            }
            index += HEADER_SIZE_FIELD_SIZE;

            // Alocação da estrutura ReceivedMsg
            (*ngEPGS)->ReceivedMsg = (NgReceivedMsg*) ng_malloc(sizeof(NgReceivedMsg));
            if (!(*ngEPGS)->ReceivedMsg) {
                printf("Falha ao alocar NgReceivedMsg.\n");
                return NG_ERROR;
            }

            // Inicialização dos campos de ReceivedMsg
            (*ngEPGS)->ReceivedMsg->msg_id = msgSeq;
            (*ngEPGS)->ReceivedMsg->frames_read = 0;
			(*ngEPGS)->ReceivedMsg->mgs_size = msgSize;
            (*ngEPGS)->ReceivedMsg->number_of_frames = getNumberOfMessages2(msgSize + HEADER_SIZE_FIELD_SIZE);
            (*ngEPGS)->ReceivedMsg->buffer = (char*) ng_malloc(sizeof(char) * msgSize);
            if (!(*ngEPGS)->ReceivedMsg->buffer) {
                printf("Falha ao alocar buffer para ReceivedMsg.\n");
                ng_free((*ngEPGS)->ReceivedMsg);
                (*ngEPGS)->ReceivedMsg = NULL;
                return NG_ERROR;
            }

            // Informações de depuração
            int headerSize = HEADER_SEGMENTATION_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE * 2 + ETHERNET_TYPE_FIELD_SIZE;
            int payloadMTU = DEFAULT_MTU2 - headerSize;

            printf("\nNovaGenesis message size = %d\n", msgSize);
            printf("\nNumber of bytes received = %d\n", payloadMTU);
        }
        else if ((*ngEPGS)->ReceivedMsg->msg_id != msgSeq) {
            // Recebido um ID de mensagem diferente enquanto outra está sendo recebida
            printf("\nID de mensagem diferente recebido. Deletando ReceivedMsg atual.\n");
            ng_free((*ngEPGS)->ReceivedMsg->buffer);
            ng_free((*ngEPGS)->ReceivedMsg);
            (*ngEPGS)->ReceivedMsg = NULL;
            return NG_ERROR;
        }
        // Caso contrário, continuação da mensagem atual
    }
    else {
        // Para fragmentos com msgNumber > 0, assegurar que ReceivedMsg existe
        if ((*ngEPGS)->ReceivedMsg == NULL) {
            printf("Recebido um fragmento com msgNumber > 0, mas ReceivedMsg está NULL.\n");
            return NG_ERROR;
        }
        // Verificar se o msgSeq corresponde ao msg_id de ReceivedMsg
        if ((*ngEPGS)->ReceivedMsg->msg_id != msgSeq) {
            printf("ID de mensagem do fragmento não corresponde ao msg_id de ReceivedMsg.\n");
            // Deletar mensagem atual
            ng_free((*ngEPGS)->ReceivedMsg->buffer);
            ng_free((*ngEPGS)->ReceivedMsg);
            (*ngEPGS)->ReceivedMsg = NULL;
            return NG_ERROR;
        }
    }

    // Cálculo do índice no buffer para inserir o fragmento
    int bufferIndex = 0;
    if(msgNumber == 0) {
        bufferIndex = 0;
    } else {
        int headerSize = HEADER_SEGMENTATION_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE * 2 + ETHERNET_TYPE_FIELD_SIZE + HEADER_SIZE_FIELD_SIZE;
        bufferIndex = DEFAULT_MTU2 - headerSize + (DEFAULT_MTU2 - (HEADER_SEGMENTATION_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE * 2 + ETHERNET_TYPE_FIELD_SIZE)) * (msgNumber - 1);
    }

    // Cópia do payload do fragmento para o buffer
    for(i = 0; index < rcvdMsgSize; i++, index++) {
        if(bufferIndex + i >= (*ngEPGS)->ReceivedMsg->mgs_size) {
            printf("Detectado overflow de buffer ao copiar o fragmento.\n");
            // Deletar a mensagem atual
            ng_free((*ngEPGS)->ReceivedMsg->buffer);
            ng_free((*ngEPGS)->ReceivedMsg);
            (*ngEPGS)->ReceivedMsg = NULL;
            return NG_ERROR;
        }
        (*ngEPGS)->ReceivedMsg->buffer[bufferIndex + i] = message[index];
    }

    // Incrementa o contador de fragmentos recebidos
    (*ngEPGS)->ReceivedMsg->frames_read++;

    // Verifica se todos os fragmentos foram recebidos
    if((*ngEPGS)->ReceivedMsg->frames_read == (*ngEPGS)->ReceivedMsg->number_of_frames) {
        // Todos os fragmentos recebidos, parsear a mensagem
        ParseReceivedMessage(ngEPGS);
        // Liberar a memória alocada após o parse
        ng_free((*ngEPGS)->ReceivedMsg->buffer);
        ng_free((*ngEPGS)->ReceivedMsg);
        (*ngEPGS)->ReceivedMsg = NULL;
        return NG_OK;
    }

    return NG_PROCESSING;
}


/*
int newMessageReceived(struct _ng_epgs **ngEPGS, const char* message, int rcvdMsgSize) {

	int ethAddrHeaderSize = ETHERNET_MAC_ADDR_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE;
	
	if(rcvdMsgSize < ethAddrHeaderSize + ETHERNET_TYPE_FIELD_SIZE + HEADER_SEGMENTATION_FIELD_SIZE) {
		return NG_ERROR;
	}	

	// Checking eth type. NG is type 0x1234
	if(!(message[ethAddrHeaderSize] == NG_TYPE_MSB) || !(message[ethAddrHeaderSize + 1] == NG_TYPE_LSB))
	{
		printf("Not a NG type");
		return NG_ERROR;
	}

	int index = ethAddrHeaderSize + ETHERNET_TYPE_FIELD_SIZE;
	int i = 0;

	int header_SeqmentationID_Size = HEADER_SEGMENTATION_FIELD_SIZE / 2;
	int header_SeqmentationCounter_Size = HEADER_SEGMENTATION_FIELD_SIZE / 2;

	unsigned msgSeq = 0;
	for(i = 0; i < header_SeqmentationID_Size; i++) {
		msgSeq |= (message[index + i] & 0x000000FF) << (8*(header_SeqmentationID_Size-1-i));
	}
	index += header_SeqmentationID_Size;

	unsigned msgNumber = 0;
	for(i = 0; i < header_SeqmentationCounter_Size; i++) {
		msgNumber |= (message[index + i] & 0x000000FF) << (8*(header_SeqmentationCounter_Size-1-i));
	}
	index += header_SeqmentationCounter_Size;

	bool b = false;

	printf("\n<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-");
	printf("\nNG FRAG RECEIVED / TICK->%lu / SIZE->%u / ID->%08X / SEQ->%u\n", xTaskGetTickCount()*portTICK_PERIOD_MS, rcvdMsgSize, msgSeq, msgNumber);
	if(message[21]==0)
	{
		for(int k = 0; k < 30; k++)
		{
			printf("%02X ", message[k]  & 0xFF );
		}
	}
	else
	{
		for(int k = 0; k < 22; k++)
		{
			printf("%02X ", message[k]  & 0xFF );
		}
	}
	printf("\n<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-\n");

	if (msgNumber == 0)
	{
		if ((*ngEPGS)->ReceivedMsg == NULL)
		{
			int msgSize = 0;
			for(i = 0; i < HEADER_SIZE_FIELD_SIZE; i++) {
				msgSize |= (message[index + i] & 0x000000FF)<< (8*(HEADER_SIZE_FIELD_SIZE-1-i));
			}
			index += HEADER_SIZE_FIELD_SIZE;

			(*ngEPGS)->ReceivedMsg = (NgReceivedMsg*) ng_malloc(sizeof(NgReceivedMsg)*1);
			(*ngEPGS)->ReceivedMsg->msg_id = msgSeq;
			(*ngEPGS)->ReceivedMsg->frames_read = 0;
			(*ngEPGS)->ReceivedMsg->mgs_size = msgSize;
			(*ngEPGS)->ReceivedMsg->number_of_frames = getNumberOfMessages2(msgSize + HEADER_SIZE_FIELD_SIZE);
			(*ngEPGS)->ReceivedMsg->buffer = (char*) ng_malloc(sizeof(char) * msgSize);

			int headerSize = HEADER_SEGMENTATION_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE +  ETHERNET_TYPE_FIELD_SIZE;
			int payloadMTU = DEFAULT_MTU2 - headerSize;

			printf("\nNovaGenesis message size = %d\n", msgSize);
			printf("\nNumber of bytes received = %d\n", payloadMTU);

		}
		else if((*ngEPGS)->ReceivedMsg->msg_id != msgSeq)
		{
			printf("\nDif id msg\n");
			ng_free((*ngEPGS)->ReceivedMsg->buffer);
			ng_free((*ngEPGS)->ReceivedMsg);
			(*ngEPGS)->ReceivedMsg->buffer = NULL;
			(*ngEPGS)->ReceivedMsg = NULL;
			return NG_ERROR;

			
		}
	else
	{
		if ((*ngEPGS)->ReceivedMsg == NULL)
		{
			return NG_ERROR;
		}
		else
		{
			b=false;
		}
	}

	int bufferIndex = 0;
	if(msgNumber == 0) {
		bufferIndex = 0;
	} else {
		bufferIndex = DEFAULT_MTU2 - (HEADER_SEGMENTATION_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE +  ETHERNET_TYPE_FIELD_SIZE + HEADER_SIZE_FIELD_SIZE) +
				(DEFAULT_MTU2 - (HEADER_SEGMENTATION_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE +  ETHERNET_TYPE_FIELD_SIZE)) * (msgNumber - 1);
	}


	for(i=0; index < rcvdMsgSize; i++, index++) {
		(*ngEPGS)->ReceivedMsg->buffer[bufferIndex + i] = message[index];
	}


	(*ngEPGS)->ReceivedMsg->frames_read++;


	if((*ngEPGS)->ReceivedMsg->frames_read == (*ngEPGS)->ReceivedMsg->number_of_frames && !b) {
		ParseReceivedMessage(ngEPGS);
		return NG_OK;
	}

	return NG_PROCESSING;
}*/

int getNumberOfMessages(int msgSize) {
	int headerSize = HEADER_SEGMENTATION_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE +  ETHERNET_TYPE_FIELD_SIZE;
	int payloadMTU = DEFAULT_MTU - headerSize;

	int payloadSize = HEADER_SIZE_FIELD_SIZE + msgSize;

	return ((int)(payloadSize/ payloadMTU)) + 1;
}

int getNumberOfMessages2(int msgSize) {
	int headerSize = HEADER_SEGMENTATION_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE + ETHERNET_MAC_ADDR_FIELD_SIZE +  ETHERNET_TYPE_FIELD_SIZE;
	int payloadMTU = DEFAULT_MTU2 - headerSize;

	int payloadSize = HEADER_SIZE_FIELD_SIZE + msgSize;

	return ((int)(payloadSize/ payloadMTU)) + 1;
}