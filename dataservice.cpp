#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <cstring>
#include <fstream>
#include <iostream>

#include "service.h"

/*
 * Initialize the instance flag parameter to false, to indicate that an instance does not currently exist.
 * Initialize the init flag as false, until the data structure has been initialized.
 * Set the shared instance to NULL, because the instance does not currently exist.
 */
bool DataService::instanceFlag = false;
bool DataService::initFlag = false;

DataService* DataService::sharedInstance = NULL;

/*
 * Initialize the transmit and receive buffers to NULL.
 * These buffers are the thread safe interface for communication between the data object and external threads
 *
 * TX -> This buffer will hold messages that are waiting to be retrieved from external threads
 *
 * RX -> This buffer will hold messages that are waiting to be handled internally by the data service
 */
MessageQueue<PMESSAGE>* DataService::rxBuffer = NULL;
MessageQueue<PMESSAGE>* DataService::txBuffer = NULL;

MessageQueue<PMESSAGE>* DataService::rxThreadBuffer = NULL;
MessageQueue<PMESSAGE>* DataService::txThreadBuffer = NULL;

MessageQueue<PMESSAGE>* DataService::rxTempBuffer = NULL;
MessageQueue<PMESSAGE>* DataService::txTempBuffer = NULL;

/* DataService::getInstance()
 *
 * This function returns a pointer of type DataService, to the singleton instance of the class.
 */
DataService* DataService::getInstance()
{
	if( !instanceFlag )
	{
		sharedInstance = new DataService();
		instanceFlag = true;
		printf("Instance Acquired\n");
		return sharedInstance;
	}
	else
	{
		return sharedInstance;
	}
}

/* DataService::Init()
 *
 * The purpose of this function is to initialize both the memory space for the data object, and
 * the token for the thread running inside the data object.
 *
 * Returns true upon success, false if any errors occur during allocation
 */
bool DataService::Init()
{	
	bool result = false;
	int errorCount = 0;

	if( !DataService::initFlag )
	{
		//Initialize TX and RX buffers
		rxBuffer = new MessageQueue<PMESSAGE>();
		txBuffer = new MessageQueue<PMESSAGE>();

		//Initialize thread buffers
		rxThreadBuffer = new MessageQueue<PMESSAGE>();
		txThreadBuffer = new MessageQueue<PMESSAGE>();

		//Initialize temp thread buffers
		rxTempBuffer = new MessageQueue<PMESSAGE>();
		txTempBuffer = new MessageQueue<PMESSAGE>();
		
		//Initialize memory space
		ms = (BYTE*)malloc(sizeof(BYTE) * ds_size);
		
		//Initialize ID record portion of data object
		id_record = (PRECORD)malloc(sizeof(RECORD));
		id_record->ptr = ms + id_offset;
		if( NULL == id_record )
		{
			printf("Error allocating ID record\n");
			errorCount++;
		}

		//Initialize device record portion of the data object
		device_record = (PRECORD)malloc(sizeof(RECORD));
		device_record->ptr = ms + device_offset;
		device_record->size = 0;
		if( NULL == device_record )
		{
			printf("Error allocating device record\n");
			errorCount++;
		}

		//Initialize the data record portion of the data object
		data_record = (PRECORD)malloc(sizeof(RECORD));
		data_record->ptr = ms + data_offset;
		data_record->size = 0;
		if( NULL == data_record )
		{
			printf("Error allocating data record\n");
			errorCount++;
		}

		//Check for errors
		if( errorCount == 0 )
		{
			result = true;
			DataService::initFlag = true;
		}
		
		// ============ TEST ============
		printf("\nDataService::Init()\n");
		printf("ID_record address: %p\n", id_record->ptr);
		printf("DV_record address: %p\n", device_record->ptr);
		printf("DA_record address: %p\n", data_record->ptr);	
		// ============ TEST ============
	}

	return result;
}

/* DataService::Request(BYTE* pMsg)
 *
 * Handle the buffer pointed to by pMsg, performing either a GET or PUT on the data object
 */
void DataService::Request(BYTE* pMsg, BYTE count)
{
	int size = (int)count;
	
	PMESSAGE msg = (PMESSAGE)malloc(sizeof(MESSAGE));
	msg->count = size;
	memcpy(msg->buf, pMsg, size);

	DataService::rxBuffer->add(msg);
	
	// ============ TEST ============
	printf("\nDataService::Request()\n");
	printf("Message: ");
	int i;
	for(i = 0; i < count; i++)
	{
		printf(" <%02x> ", msg->buf[i]);
	}
	printf("\n");
	// ============ TEST ============
}

/* DataService::AckRequest()
 *
 * The purpose of this function is to acknowledge the receipt of a message
 * 
 * The data object places an ack message into the TX buffer
 */
void DataService::AckRequest(BYTE* pMsg, BYTE count)
{
	PMESSAGE ack = (PMESSAGE)malloc(sizeof(MESSAGE));
	memcpy(ack->buf, pMsg, count);
	ack->buf[count] = ACK;
	ack->count++;
	
	// ============ TEST ============
	printf("\nDataService::AckRequest()\n");
	printf("Message: ");
	int i;
	for(i = 0; i <= count; i++)
	{
		printf(" <%02x> ", ack->buf[i]);
	}
	printf("\n");
	// ============ TEST ============
}

/* DataService::HandleRX()
 *
 * This function is called internally by the data service thread to handle the buffer requests
 */
void DataService::HandleRX()
{
	PMESSAGE msg = (PMESSAGE)malloc(sizeof(MESSAGE));
	msg = DataService::rxBuffer->remove();
	BYTE cmd = msg->buf[4];

	BYTE* pMsg = (BYTE*)malloc(sizeof(BYTE) * msg->count);
	memcpy(pMsg, msg->buf, msg->count);
	
	// ============ TEST ============
	printf("\nDataService::HandleRX()\n");
	printf("Message: ");
	int i;
	for(i = 0; i < msg->count; i++)
	{
		printf(" <%02x> ", pMsg[i]);
	}
	printf("\n");
	// ============ TEST ============
	
	switch(cmd)
	{
		//INIT
		case 0x01:
			break;
			
		//WRITE
		case 0x02:
			DataService::Put(pMsg);
			break;
			
		//READ
		case 0x03:
			DataService::Get(pMsg[2]);
			break;
	}
}

/* DataService::HandleTX()
 *
 * This is a public method for a client service to connect to in order to handle the delivery of TX buffer items
 */
void DataService::HandleTX()
{
	PMESSAGE msg = (PMESSAGE)malloc(sizeof(MESSAGE));
	msg = DataService::txBuffer->remove();
	
	BYTE dst = msg->buf[3];
	BYTE* pMsg = (BYTE*)malloc(sizeof(BYTE) * msg->count);
	memcpy(pMsg, msg->buf, msg->count);

	// ============ TEST ============
	printf("\nDataService::HandleTX()\n");
	printf("Size: %d\n", msg->count);
	printf("Message: ");
	int i;
	for(i = 0; i < msg->count; i++)
	{
		printf(" <%02x> ", pMsg[i]);
	}
	printf("\n");
	// ============ TEST ============
}

/* DataService::Put(BYTE* pMsg)
 *
 * The purpose of this function is to write a BYTE array to the appropriate space in memory
 */
void DataService::Put(BYTE *pMsg)
{
	int i;
	bool terminate = false;
	
	//Check to see if device is registered, if not, it will be registered
	if( DataService::Validate(pMsg[2], PUT) )
	{
		//Reset device temp pointer
		device_record->temp = device_record->ptr;
		printf("\nDataService::Put()\n");
		
		//Assign temp pointer to the first free byte in the data record
		data_record->temp = data_record->ptr + data_record->size;
		printf("First free byte: %p\n", data_record->temp);
		
		//Write the number of bytes contained in the message to the first byte of the free space
		*(data_record->temp) = pMsg[5];
		printf("Count of message: %d\n", (int)*(data_record->temp));
		
		//Set device index pointer to first byte of message based on ID of message
		for(i = 0; i < device_record->size && !terminate; i++)
		{
			printf("Record Address: <%02x> | Source: <%02x>\n", *(device_record->temp), pMsg[2]);

			if( *(device_record->temp) == pMsg[2] )
			{
				device_record->indexes[i] = data_record->temp;

				printf("Index: %d\n", i);
				printf("Value: %x\n", *device_record->temp);
				printf("Device index pointer address: %p\n", device_record->indexes[i]);

				terminate = true;
				break;
			}
			
			device_record->temp++;
		}
		
		//Increment the pointer and write the message to memory
		data_record->temp++;
		memcpy(data_record->temp, pMsg, pMsg[5]);
		
		printf("Message: ");

		int j;
		for(j = 0; j < pMsg[5]; j++)
		{
			printf(" <%02x> ", data_record->temp[j]);
		}
		printf("\n");
		
		//Increase the size field of the data record to indicate the total number of used bytes in the record
		data_record->size += (int)pMsg[5];
		printf("Data record size: %d\n", data_record->size);	
	}
	
}

/* DataService::Get(BYTE id)
 *
 * The purpose of this function is to return the most recent message in memory for a given device
 */
void DataService::Get(BYTE id)
{
	int i;
	bool terminate = false;
	
	//Check to see if device exists
	if( DataService::Validate(id, GET) )
	{
		printf("\nDataService::Get()\n");
		
		device_record->temp = device_record->ptr;
		
		for(i = 0; i < device_record->size && !terminate; i++)
		{
			if( *(device_record->temp) == id)
			{
				printf("Device: <%02x>\n", *device_record->temp);
				printf("ID: <%02x> | Index: %d\n", id, i);

				PMESSAGE msg = (PMESSAGE)malloc(sizeof(MESSAGE));
				
				data_record->temp = device_record->indexes[i];
				printf("Device record index pointer: %p\n", data_record->temp);
				
				int size = *(data_record->temp);
				data_record->temp++;
				
				memcpy(msg->buf, data_record->temp, size);
				msg->count = size;
				
				DataService::txBuffer->add(msg);

				printf("Message: ");
				int j;
				for(j = 0; j < size; j++)
				{
					printf(" <%02x> ", data_record->temp[j]);
				}
				printf("\n");

				terminate = true;
				break;
			}
			
			device_record->temp++;
		}
	}
}

/* DataService::GetDevices()
 * 
 * Print out a list of registered devices, displaying their identifier BYTE
 */
void DataService::GetDevices()
{
	device_record->temp = device_record->ptr;
	int i;
	
	for( i = 0; i < device_record->size; i++)
	{
		printf("Device ID: <%02x>\n", *device_record->temp);
		device_record->temp++;
	}
}

/* DataService::Validate(BYTE id)
 *
 * The purpose of this function is to check the validity of the requesting device
 *
 * PUT: If device exists, return TRUE. If device doesn't exist, register the device and return TRUE.
 *
 * GET: If the device exists, return TRUE. If the device doesn't exist, return FALSE.
 *
 */
bool DataService::Validate(BYTE id, BYTE cmd)
{
	device_record->temp = device_record->ptr;

	bool result = 		false;
	bool found = 		false;

	int i;
	for(i = 0; i < device_record->size && !found; i++)
	{
		if( *(device_record->temp) == id )
		{
			result = true;
			found = true;
			break;
		}

		device_record->temp++;
	}
	
	//Device is not registered
	if(!found)
	{
		//If a read is requested and the device is not registered, return false
		if(cmd == 0x03)
		{
			result = false;
		}
		else
		{
			//Move to empty location in device record
			device_record->temp = device_record->ptr + device_record->size;			
			
			//Write new device ID to device record
			*(device_record->temp) = id;
			
			//Increment of the size of the device record
			device_record->size++;
			
			//Indicate successful registration
			result = true;
			
			// ============ TEST ============
			printf("\nDataService::Validate()\n");
			printf("Device record address: %p\n", device_record->temp);
			printf("Device record value: <%02x>\n", *(device_record->temp));
			// ============ TEST ============

		}
	}
	
	return result;
}

void DataService::rxCopy()
{
	DataService::rxBuffer->exportContainer(rxTempBuffer);
	DataService::rxThreadBuffer->importContainer(rxTempBuffer);
}

void DataService::txCopy()
{
	DataService::txBuffer->exportContainer(txTempBuffer);
	DataService::txThreadBuffer->importContainer(txTempBuffer);
}

void DataService::rxTest()
{
	int i;
	for(i = 0; i < 10; i++)
	{
		PMESSAGE msg = (PMESSAGE)malloc(sizeof(MESSAGE));
		msg->count = i;
		DataService::rxBuffer->add(msg);
		printf("RX Buffer: %d\n", msg->count);
	}

	DataService::rxCopy();

	int j;
	for(j = 0; j < 10; j++)
	{
		PMESSAGE ret = (PMESSAGE)malloc(sizeof(MESSAGE));
		ret = DataService::rxThreadBuffer->remove();
		printf("RX TMP Buffer: %d\n", ret->count);
	}
}

void DataService::txTest()
{
	int i;
	for(i = 0; i < 10; i++)
	{
		PMESSAGE msg = (PMESSAGE)malloc(sizeof(MESSAGE));
		msg->count = i;
		DataService::txBuffer->add(msg);
		printf("TX Buffer: %d\n", msg->count);
	}

	DataService::txCopy();

	int j;
	for(j = 0; j < 10; j++)
	{
		PMESSAGE ret = (PMESSAGE)malloc(sizeof(MESSAGE));
		ret = DataService::txThreadBuffer->remove();
		printf("TX TMP Buffer: %d\n", ret->count);
	}
}
