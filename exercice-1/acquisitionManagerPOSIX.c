#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include "acquisitionManager.h"
#include "msg.h"
#include "iSensor.h"
#include "multitaskingAccumulator.h"
#include "iAcquisitionManager.h"
#include "debug.h"

#define CHECK_SEMAPHORE(sem) \
if (sem != SEM_FAILED)\
		return ERROR_SUCCESS;\
\
	perror("[sem_open");\
	return ERROR_INIT;\

#define MSG_BUFFER_SIZE PRODUCER_COUNT

#define SEM_FULL_NAME "/full"
#define SEM_FULL_INITIAL_VALUE 0

#define SEM_FREE_NAME "/free"
#define SEM_FREE_INITIAL_VALUE MSG_BUFFER_SIZE

#define SEM_SUM_READY_NAME "/sum_ready"
#define SEM_SUM_READY_INITIAL_VALUE 0



//producer count storage
volatile unsigned int produceCount = 0;

pthread_t producers[PRODUCER_COUNT];

static void *produce(void *params);

// Buffers
MSG_BLOCK msgBuffer[MSG_BUFFER_SIZE] = { 0 };
unsigned int freeIndexArray[MSG_BUFFER_SIZE];
unsigned int fullIndexArray[MSG_BUFFER_SIZE];
u_int8_t freeIndex = 0;
u_int8_t fullIndex = 0;

/**
* Semaphores and Mutex
*/
pthread_mutex_t mutexIndexFree = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexIndexFull = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexProduceCount = PTHREAD_MUTEX_INITIALIZER;
sem_t* semFull;
sem_t* semFree;
sem_t* semSumReady;

/*
* Creates the synchronization elements.
* @return ERROR_SUCCESS if the init is ok, ERROR_INIT otherwise
*/
static unsigned int createSynchronizationObjects(void);

/*
* Increments the produce count.
*/
static void incrementProducedCount(void);

/// @brief Set the default values of the buffers
static void initialiseBuffers();

/// @brief Write a message in the buffer
/// @param msg Message to write
static void writeMessage(MSG_BLOCK* msg);

static unsigned int createSynchronizationObjects(void)
{
	// **************** SEMAPHORES **************** //
	// Unlink semaphores 
	sem_unlink(SEM_FULL_NAME);
	sem_unlink(SEM_FREE_NAME);
	sem_unlink(SEM_SUM_READY_NAME);

	// Open semaphores
	semFull 	= sem_open(SEM_FULL_NAME, 		O_CREAT, 0644, SEM_FULL_INITIAL_VALUE);
	semFree 	= sem_open(SEM_FREE_NAME, 		O_CREAT, 0644, SEM_FREE_INITIAL_VALUE);
	semSumReady = sem_open(SEM_SUM_READY_NAME, 	O_CREAT, 0644, SEM_SUM_READY_INITIAL_VALUE);

	// Check semaphores 
	CHECK_SEMAPHORE(semFull);
	CHECK_SEMAPHORE(semFree);
	CHECK_SEMAPHORE(semSumReady);

	printf("[acquisitionManager]Semaphore created\n");
	return ERROR_SUCCESS;
}

static void incrementProducedCount(void)
{
	pthread_mutex_lock(&mutexProduceCount);
	produceCount++;
	pthread_mutex_unlock(&mutexProduceCount);
}

static void initialiseBuffers()
{
	unsigned int i;
	for (i = 0; i < MSG_BUFFER_SIZE; i++)
	{
		freeIndexArray[i] = i;
		fullIndexArray[i] = i;
	}
}

static void writeMessage(MSG_BLOCK *msg)
{
	sem_wait(semFree);
	pthread_mutex_lock(&mutexIndexFree);
	unsigned int i = freeIndexArray[freeIndex];
	freeIndex = (freeIndex + 1) % MSG_BUFFER_SIZE;
	pthread_mutex_unlock(&mutexIndexFree);

	msgBuffer[i] = *msg;

	pthread_mutex_lock(&mutexIndexFull);
	fullIndexArray[fullIndex] = i;
	fullIndex = (fullIndex + 1) % MSG_BUFFER_SIZE;
	pthread_mutex_unlock(&mutexIndexFull);
	sem_post(semFull);
}

unsigned int getProducedCount(void)
{
	unsigned int p = 0;
	pthread_mutex_lock(&mutexProduceCount);
	p = produceCount;
	pthread_mutex_unlock(&mutexProduceCount);
	return p;
}

MSG_BLOCK getMessage(void){
	sem_wait(semFull);
	unsigned int i = fullIndexArray[fullIndex];
	fullIndex = (fullIndex + 1) % MSG_BUFFER_SIZE;

	MSG_BLOCK msg = msgBuffer[i];

	freeIndexArray[freeIndex] = i;
	freeIndex = (freeIndex + 1) % MSG_BUFFER_SIZE;
	sem_post(semFree);

	return msg;
}

void waitSemSumReady()
{
	sem_wait(semSumReady);
}

void setSemSumReady()
{
	sem_post(semSumReady);
}

unsigned int acquisitionManagerInit(void)
{
	unsigned int i;
	printf("[acquisitionManager]Synchronization initialization in progress...\n");
	fflush( stdout );
	if (createSynchronizationObjects() == ERROR_INIT)
		return ERROR_INIT;
	
	printf("[acquisitionManager]Synchronization initialization done.\n");

	initialiseBuffers();

	for (i = 0; i < PRODUCER_COUNT; i++)
	{
		pthread_create(&producers[i], NULL, produce, (void*)i);
	}

	return ERROR_SUCCESS;
}

void acquisitionManagerJoin(void)
{
	unsigned int i;
	for (i = 0; i < PRODUCER_COUNT; i++)
	{
		pthread_join(producers[i], NULL);
	}

	sem_destroy(semFull);
	sem_destroy(semFree);
	sem_destroy(semSumReady);
	printf("[acquisitionManager]Semaphore cleaned\n");
}

void *produce(void* params)
{
	D(printf("[acquisitionManager]Producer created with id %d\n", gettid()));
	unsigned int producerIndex = (unsigned int)params;
	unsigned int i = 0;
	while (i < PRODUCER_LOOP_LIMIT)
	{
		i++;
		sleep(PRODUCER_SLEEP_TIME+(rand() % 5));
		MSG_BLOCK inputMsg;
		getInput(producerIndex, &inputMsg);
		if (!messageCheck(&inputMsg))
		{
			printf("[acquisitionManager]ERROR::getInput: bad checksum\tProducer Index: %d", producerIndex);
			continue;
		}
		writeMessage(&inputMsg);
		incrementProducedCount();
	}
	printf("[acquisitionManager] %d termination\n", gettid());
	pthread_exit(NULL);
}