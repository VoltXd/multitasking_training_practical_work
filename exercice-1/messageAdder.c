#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h> 
#include <unistd.h>
#include <pthread.h>
#include "messageAdder.h"
#include "msg.h"
#include "iMessageAdder.h"
#include "multitaskingAccumulator.h"
#include "iAcquisitionManager.h"
#include "debug.h"

//consumer thread
pthread_t consumer;
//Message computed
volatile MSG_BLOCK out;
//Consumer count storage
volatile unsigned int consumeCount = 0;

/**
 * Increments the consume count.
 */
static void incrementConsumeCount(void);

/**
 * Consumer entry point.
 */
static void *sum( void *parameters );


MSG_BLOCK getCurrentSum(){
	waitSemSumReady();
	MSG_BLOCK currentSum = out;
	
	return currentSum;
}

unsigned int getConsumedCount(){
	return consumeCount;
}


void messageAdderInit(void){
	out.checksum = 0;
	for (size_t i = 0; i < DATA_SIZE; i++)
	{
		out.mData[i] = 0;
	}
	pthread_create(&consumer, NULL, sum, NULL);
}

void messageAdderJoin(void){
	pthread_join(consumer, NULL);
}

void incrementConsumeCount(void)
{
	consumeCount++;
}

static void *sum(void *parameters)
{
	D(printf("[messageAdder]Thread created for sum with id %d\n", gettid()));
	unsigned int i = 0;
	while(i<ADDER_LOOP_LIMIT){
		i++;
		sleep(ADDER_SLEEP_TIME);
		MSG_BLOCK newMsg = getMessage();
		if (!messageCheck(&newMsg))
		{
			printf("[messageAdder]ERROR::getInput: bad checksum");
			continue;
		}
		messageAdd(&out, &newMsg);
		incrementConsumeCount();

		if (getConsumedCount() % 4 == 0)
			setSemSumReady();
	}
	printf("[messageAdder] %d termination\n", gettid());
	pthread_exit(NULL);
}


