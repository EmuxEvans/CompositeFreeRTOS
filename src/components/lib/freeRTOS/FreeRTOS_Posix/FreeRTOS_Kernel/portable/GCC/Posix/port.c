/*
    FreeRTOS V7.3.0 - Copyright (C) 2012 Real Time Engineers Ltd.

    FEATURES AND PORTS ARE ADDED TO FREERTOS ALL THE TIME.  PLEASE VISIT 
    http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS tutorial books are available in pdf and paperback.        *
     *    Complete, revised, and edited pdf reference manuals are also       *
     *    available.                                                         *
     *                                                                       *
     *    Purchasing FreeRTOS documentation will not only help you, by       *
     *    ensuring you get running as quickly as possible and with an        *
     *    in-depth knowledge of how to use FreeRTOS, it will also help       *
     *    the FreeRTOS project to continue with its mission of providing     *
     *    professional grade, cross platform, de facto standard solutions    *
     *    for microcontrollers - completely free of charge!                  *
     *                                                                       *
     *    >>> See http://www.FreeRTOS.org/Documentation for details. <<<     *
     *                                                                       *
     *    Thank you for using FreeRTOS, and thank you for your support!      *
     *                                                                       *
    ***************************************************************************


    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    >>>NOTE<<< The modification to the GPL is included to allow you to
    distribute a combined work that includes FreeRTOS without being obliged to
    provide the source code for proprietary components outside of the FreeRTOS
    kernel.  FreeRTOS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public
    License and the FreeRTOS license exception along with FreeRTOS; if not it
    can be viewed here: http://www.freertos.org/a00114.html and also obtained
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!
    
    ***************************************************************************
     *                                                                       *
     *    Having a problem?  Start by reading the FAQ "My application does   *
     *    not run, what could be wrong?"                                     *
     *                                                                       *
     *    http://www.FreeRTOS.org/FAQHelp.html                               *
     *                                                                       *
    ***************************************************************************

    
    http://www.FreeRTOS.org - Documentation, training, latest versions, license 
    and contact details.  
    
    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool.

    Real Time Engineers ltd license FreeRTOS to High Integrity Systems, who sell 
    the code with commercial support, indemnification, and middleware, under 
    the OpenRTOS brand: http://www.OpenRTOS.com.  High Integrity Systems also
    provide a safety engineered and independently SIL3 certified version under 
    the SafeRTOS brand: http://www.SafeRTOS.com.
*/

#include "FreeRTOS.h"
#include "task.h"
#include <jw_freertos.h>

/*-----------------------------------------------------------
 * Implementation of functions defined in portable.h for the AVR port.
 *----------------------------------------------------------*/

/* Start tasks with interrupts enables. */
/* #define portFLAGS_INT_ENABLED					( ( portSTACK_TYPE ) 0x80 ) */

/* /\* Hardware constants for timer 1. *\/ */
/* #define portCLEAR_COUNTER_ON_MATCH				( ( unsigned char ) 0x08 ) */
/* #define portPRESCALE_64							( ( unsigned char ) 0x03 ) */
/* #define portCLOCK_PRESCALER						( ( unsigned long ) 64 ) */
/* #define portCOMPARE_MATCH_A_INTERRUPT_ENABLE	( ( unsigned char ) 0x10 ) */

/*-----------------------------------------------------------*/

/* We require the address of the pxCurrentTCB variable, but don't want to know
any details of its type. */
/* typedef void tskTCB; */
/* extern volatile tskTCB * volatile pxCurrentTCB; */

/*-----------------------------------------------------------*/

/* 
 * Macro to save all the general purpose registers, the save the stack pointer
 * into the TCB.  
 * 
 * The interrupts will have been disabled during the call to portSAVE_CONTEXT()
 * so we need not worry about reading/writing to the stack pointer. 
 */

#define MAX_NUMBER_OF_TASKS 256

#define portSAVE_CONTEXT()

/* #define portDISABLE_INTERRUPTS() */

/* #define portENABLE_INTERRUPTS() */

typedef struct taskMapping
{
	int thd_id;
	xTaskHandle task;
} taskMapping_t;

taskMapping_t taskMappings[MAX_NUMBER_OF_TASKS];

typedef struct XPARAMS 
{
	pdTASK_CODE pxCode;
	void *pvParams;
} xParams;

int lastCreatedTask;

/* 
 * Opposite to portSAVE_CONTEXT().  Interrupts will have been disabled during
 * the context save so we can write to the stack pointer. 
 */

#define portRESTORE_CONTEXT()

/*-----------------------------------------------------------*/

/*
 * Perform hardware setup to enable ticks from timer 1, compare match A.
 */
static void prvSetupTimerInterrupt( void );
/*-----------------------------------------------------------*/

void *prvWaitForStart( void *pvParams) {
	xParams *pxParams = (xParams *) pvParams;
	pdTASK_CODE pvCode = pxParams->pxCode;
	void *pParams = pxParams->pvParams;
	
	//	vPortFree(pvParams);
	
	// pthread_cleanup_push?

	jw_print("Running task's function!\n");
	pvCode (pParams);
	
	// other pthread crap here.

	return (void *) NULL;
}

/* 
 * See header file for description. 
 */
portSTACK_TYPE *pxPortInitialiseStack( portSTACK_TYPE *pxTopOfStack, pdTASK_CODE pxCode, void *pvParameters )
{
	
	xParams *pxThisThreadParams = pvPortMalloc( sizeof(xParams));
	pxThisThreadParams->pxCode = pxCode;
	pxThisThreadParams->pvParams = pvParameters;

	// enter a critical section here.
	//	jw_lock();
	int thd_id = jw_create_thread((int) prvWaitForStart, (int) pxThisThreadParams, 0);
	lastCreatedTask = thd_id;

	jw_print("FreeRTOS started thd %d\n", thd_id);//.. switching to it...\n");
	//	jw_switch_thread(thd_id, 0);
	
	//	jw_unlock();

	return pxTopOfStack;
}
/*-----------------------------------------------------------*/

void pvAddTaskMapping (int thread_id, xTaskHandle task_handle) {
	// FIXME: should make this mapping more dynamic.
	taskMappings[thread_id].thd_id = thread_id;
	taskMappings[thread_id].task = task_handle;
}

void pvAddTaskHandle (void *pxTaskHandle) {
	pvAddTaskMapping((int) lastCreatedTask, (xTaskHandle) pxTaskHandle);
}

/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
	/* Probably don't need this. */
}
/*-----------------------------------------------------------*/

int prvGetThreadHandle( xTaskHandle task_handle) {
	int i;
	for (i = 0; i < MAX_NUMBER_OF_TASKS; i++) {
		if (taskMappings[i].task == task_handle) {
			return taskMappings[i].thd_id;
		}
	}
	jw_print("NO TASK FOUND FOR THE TASK HANDLE\n");
	return 0;
}

/*
 * Manual context switch.  
 */
void vPortYield( void )
{
	jw_print("Yielding from the port.\n");

	//	jw_lock();

	jw_print("Acquired lock.\n");

	int cur_thd_id = prvGetThreadHandle(xTaskGetCurrentTaskHandle());
	
	vTaskSwitchContext();

	jw_print("Switched context\n");
	
	int next_thd = prvGetThreadHandle(xTaskGetCurrentTaskHandle());
	
	jw_print("Yielding from thd %d to thd %d\n", cur_thd_id, next_thd);
	
	jw_switch_thread(next_thd, 0);
	
	jw_unlock();
	
	return;
}
/*-----------------------------------------------------------*/

/*
 * Context switch function used by the tick.  This must be identical to 
 * vPortYield() from the call to vTaskSwitchContext() onwards.  The only
 * difference from vPortYield() is the tick count is incremented as the
 * call comes from the tick ISR.
 */
//void vPortYieldFromTick( void ) __attribute__ ( ( naked ) );
void vPortYieldFromTick( void )
{
	/* Save context, increment tick (vtaskincrementtick()), switch context, restore context */
	return;
}
/*-----------------------------------------------------------*/

/*
 * Setup timer 1 compare match A to generate a tick interrupt.
 */
static void prvSetupTimerInterrupt( void )
{
	return;
}


portBASE_TYPE xPortStartScheduler( void )
{
	// need to call prvSetupTimerInterrupt()
	// and then restore the context of the first task that is going to run
	// and start it.
	jw_print("Starting freeRTOS scheduler\n");

	//vPortEnableInterrupts();
	int first_thread = prvGetThreadHandle(xTaskGetCurrentTaskHandle()); 
	jw_print("First thread: %d\n", first_thread);
	jw_switch_thread(first_thread, 0);
	
	return 0;
}
	
