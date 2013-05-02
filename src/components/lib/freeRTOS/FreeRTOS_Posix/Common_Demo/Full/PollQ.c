/*
    FreeRTOS V6.0.4 - Copyright (C) 2010 Real Time Engineers Ltd.

    ***************************************************************************
    *                                                                         *
    * If you are:                                                             *
    *                                                                         *
    *    + New to FreeRTOS,                                                   *
    *    + Wanting to learn FreeRTOS or multitasking in general quickly       *
    *    + Looking for basic training,                                        *
    *    + Wanting to improve your FreeRTOS skills and productivity           *
    *                                                                         *
    * then take a look at the FreeRTOS eBook                                  *
    *                                                                         *
    *        "Using the FreeRTOS Real Time Kernel - a Practical Guide"        *
    *                  http://www.FreeRTOS.org/Documentation                  *
    *                                                                         *
    * A pdf reference manual is also available.  Both are usually delivered   *
    * to your inbox within 20 minutes to two hours when purchased between 8am *
    * and 8pm GMT (although please allow up to 24 hours in case of            *
    * exceptional circumstances).  Thank you for your support!                *
    *                                                                         *
    ***************************************************************************

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    ***NOTE*** The exception to the GPL is included to allow you to distribute
    a combined work that includes FreeRTOS without being obliged to provide the
    source code for proprietary components outside of the FreeRTOS kernel.
    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public 
    License and the FreeRTOS license exception along with FreeRTOS; if not it 
    can be viewed here: http://www.freertos.org/a00114.html and also obtained 
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!

    http://www.FreeRTOS.org - Documentation, latest information, license and
    contact details.

    http://www.SafeRTOS.com - A version that is certified for use in safety
    critical systems.

    http://www.OpenRTOS.com - Commercial support, development, porting,
    licensing and training services.
*/


/**
 * This is a very simple queue test.  See the BlockQ. c documentation for a more 
 * comprehensive version.
 *
 * Creates two tasks that communicate over a single queue.  One task acts as a 
 * producer, the other a consumer.  
 *
 * The producer loops for three iteration, posting an incrementing number onto the 
 * queue each cycle.  It then delays for a fixed period before doing exactly the 
 * same again.
 *
 * The consumer loops emptying the queue.  Each item removed from the queue is 
 * checked to ensure it contains the expected value.  When the queue is empty it 
 * blocks for a fixed period, then does the same again.
 *
 * All queue access is performed without blocking.  The consumer completely empties 
 * the queue each time it runs so the producer should never find the queue full.  
 *
 * An error is flagged if the consumer obtains an unexpected value or the producer 
 * find the queue is full.
 *
 * \page PollQC pollQ.c
 * \ingroup DemoFiles
 * <HR>
 */

/*
Changes from V2.0.0

	+ Delay periods are now specified using variables and constants of
	  portTickType rather than unsigned long.
*/

#include <stdlib.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "print.h"
#include <jw_freertos.h>

/* Demo program include files. */
#include "PollQ.h"

#define pollqSTACK_SIZE		( ( unsigned short ) configMINIMAL_STACK_SIZE )

#ifdef FREERTOS_INTERRUPT_LATENCY_TEST
/* The task that posts the incrementing number onto the queue. */
static void vPolledQueueProducer( void *pvParameters );

static void lowPrioRdtscTask( void *pvParameters );
#endif

#ifdef FREERTOS_MQ_TEST
/* The task that posts the incrementing number onto the queue. */
static void vPolledQueueProducerMQTest ( void *pvParameters );
/* The task that empties the queue. */
static void vPolledQueueConsumer( void *pvParameters );
static u64_t total_latency = 0, latency_measurements = 0;
#endif

/* Variables that are used to check that the tasks are still running with no errors. */
static volatile short sPollingConsumerCount = 0, sPollingProducerCount = 0;

#ifdef FREERTOS_INTERRUPT_LATENCY_TEST
/* variables to keep track of total latency for a test by JW on MQ performance.*/
static u64_t total_interrupt_latency = 0, interrupt_measurements = 0;
static volatile u64_t lastTSCval = 0;
/* Low prio task to constantly check tsc */
#endif

/*-----------------------------------------------------------*/

void vStartPolledQueueTasks( unsigned portBASE_TYPE uxPriority )
{
static xQueueHandle xPolledQueue;
const unsigned portBASE_TYPE uxQueueSize = 10;

	/* Create the queue used by the producer and consumer. */
	xPolledQueue = xQueueCreate( uxQueueSize, ( unsigned portBASE_TYPE ) sizeof( u64_t ) );

	/* Spawn the producer and consumer. */
#ifdef FREERTOS_INTERRUPT_LATENCY_TEST
	xTaskCreate( vPolledQueueProducer, "QProdNB", pollqSTACK_SIZE, ( void * ) &xPolledQueue, uxPriority, NULL );
	xTaskCreate( lowPrioRdtscTask, "RdTSC", pollqSTACK_SIZE, (void *) 0, uxPriority - 1, NULL);
#ifdef FREERTOS_MQ_TEST
#error Interrupt latency and mq tests are mutually exclusive. Please define only one.
#endif // FREERTOS_MQ_TEST
#endif // FREERTOS_INTERRUPT_LATENCY_TEST

#ifdef FREERTOS_MQ_TEST
	xTaskCreate( vPolledQueueProducerMQTest, "QProdNB", pollqSTACK_SIZE, ( void * ) &xPolledQueue, uxPriority, NULL );
	xTaskCreate( vPolledQueueConsumer, "QConsNB", pollqSTACK_SIZE, ( void * ) &xPolledQueue, uxPriority, NULL );
#ifdef FREERTOS_INTERRUPT_LATENCY_TEST
#error Interrupt latency and mq tests are mutually exclusive. Please define only one.
#endif // FREERTOS_INTERRUPT_LATENCY_TEST
#endif // FREERTOS_MQ_TEST

}
/*-----------------------------------------------------------*/
#ifdef FREERTOS_INTERRUPT_LATENCY_TEST
static void lowPrioRdtscTask( void *pvParameters )
{
	// I HATE THEIR PROGRAMMING STYLE.
	freertos_print("Started tsc task\n");
	for( ;; ) 
	{
		//		freertos_print("Running tsc task\n");
		rdtscll(lastTSCval);
	}
}

static void vPolledQueueProducer( void *pvParameters )
{
	//	u64_t usValue = 0, usLoop;
	xQueueHandle *pxQueue;
	const portTickType xDelay = ( portTickType ) 200 / portTICK_RATE_MS;
	//const unsigned short usNumToProduce = 1;
	const char * const pcTaskStartMsg = "Polled queue producer started.\r\n";
	//const char * const pcTaskErrorMsg = "Could not post on polled queue.\r\n";
	//short sError = pdFALSE;
	u64_t tsc;

	/* Queue a message for printing to say the task has started. */
	vPrintDisplayMessage( &pcTaskStartMsg );

	/* The queue being used is passed in as the parameter. */
	pxQueue = ( xQueueHandle * ) pvParameters;

	for( ;; )
	{		
		rdtscll(tsc);
		if (tsc > lastTSCval && lastTSCval > 0) {
			total_interrupt_latency += (tsc - lastTSCval);
			freertos_print("interrupt_latency: %llu\n", tsc - lastTSCval);
		}
		interrupt_measurements++;
 		if (interrupt_measurements % 10 == 0 && interrupt_measurements > 0) {
 			freertos_print("Average interrupt latency: %llu\n", total_interrupt_latency / interrupt_measurements);
 		}

		/* Wait before we start posting again to ensure the consumer runs and
		empties the queue. */
		vTaskDelay( xDelay );
	}
}
#endif

/*-----------------------------------------------------------*/
#ifdef FREERTOS_MQ_TEST
static void vPolledQueueProducerMQTest ( void *pvParameters )
{
	
	xQueueHandle *pxQueue;
	const portTickType xDelay = ( portTickType ) 200 / portTICK_RATE_MS;
	const char * const pcTaskStartMsg = "Polled queue producer started.\r\n";
	const char * const pcTaskErrorMsg = "Could not post on polled queue.\r\n";
	u64_t tsc, usValue = 0, usLoop, usNumToProduce = 1;

	/* Queue a message for printing to say the task has started. */
	vPrintDisplayMessage( &pcTaskStartMsg );

	/* The queue being used is passed in as the parameter. */
	pxQueue = ( xQueueHandle * ) pvParameters;

	for( ;; )
	{		

		for( usLoop = 0; usLoop < usNumToProduce; ++usLoop )
		{
			rdtscll(tsc);
			/* Send an incrementing number on the queue without blocking. */
			if( xQueueSendToBack( *pxQueue, ( void * ) &tsc, ( portTickType ) 0 ) != pdPASS )
			{
				/* We should never find the queue full - this is an error. */
				vPrintDisplayMessage( &pcTaskErrorMsg );
			}
			else
			{
				/* if( sError == pdFALSE ) */
				/* { */
				/* 	/\* If an error has ever been recorded we stop incrementing the */
				/* 	check variable. *\/ */
				/* 	++sPollingProducerCount; */
				/* } */

				/* Update the value we are going to post next time around. */
				++usValue;
				/* freertos_print("Produced message\n"); */
			}
		}

		/* Wait before we start posting again to ensure the consumer runs and 
		empties the queue. */
		vTaskDelay( xDelay );
	}
}


static void vPolledQueueConsumer( void *pvParameters )
{
	u64_t usData;
	xQueueHandle *pxQueue;
	const portTickType xDelay = ( portTickType ) 200 / portTICK_RATE_MS;
	const char * const pcTaskStartMsg = "Polled queue consumer started.\r\n";
	u64_t cur_tsc, latency;
	/* Queue a message for printing to say the task has started. */
	vPrintDisplayMessage( &pcTaskStartMsg );

	/* The queue being used is passed in as the parameter. */
	pxQueue = ( xQueueHandle * ) pvParameters;

	for( ;; )
	{		
		/* Loop until the queue is empty. */
		while( uxQueueMessagesWaiting( *pxQueue ) )
		{
			if( xQueueReceive( *pxQueue, &usData, ( portTickType ) 0 ) == pdPASS )
			{
				rdtscll(cur_tsc);
				if (cur_tsc < usData) continue;

				latency = cur_tsc - usData;
				
				total_latency += latency;
				latency_measurements++;
				++sPollingConsumerCount;
				

				if (latency_measurements % 50 == 0 && latency_measurements != 0) {
					freertos_print("Average latency for producer consumer queue: %llu\n", total_latency / latency_measurements);
				}
				//				freertos_print("Got the expected value in the queue.\n");
			}
		}

		/* Now the queue is empty we block, allowing the producer to place more 
		items in the queue. */
		vTaskDelay( xDelay );
	}
}
#endif

/*-----------------------------------------------------------*/

/* This is called to check that all the created tasks are still running with no errors. */
portBASE_TYPE xArePollingQueuesStillRunning( void )
{
static short sLastPollingConsumerCount = 0, sLastPollingProducerCount = 0;
portBASE_TYPE xReturn;

	if( ( sLastPollingConsumerCount == sPollingConsumerCount ) ||
		( sLastPollingProducerCount == sPollingProducerCount ) 
	  )
	{
		xReturn = pdFALSE;
	}
	else
	{
		xReturn = pdTRUE;
	}

	sLastPollingConsumerCount = sPollingConsumerCount;
	sLastPollingProducerCount = sPollingProducerCount;

	return xReturn;
}
