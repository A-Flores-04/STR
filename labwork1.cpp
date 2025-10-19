#include <iostream>
#include<conio.h>
#include<stdlib.h>
#include<windows.h>    // for the Sleep function

//define _USE_REAL_KIT_

#include "my_interaction_functions.h"

extern "C" {
	#include <FreeRTOS.h>
	#include <task.h>
	#include <timers.h>
	#include <semphr.h>
	#include <interface.h>	
	#include <interrupts.h>
}

#define mainREGION_1_SIZE   8201
#define mainREGION_2_SIZE   29905
#define mainREGION_3_SIZE   7607

typedef struct vTask_signals_struct {
	xSemaphoreHandle sem_press_p;
} vTask_signals_struct;

typedef struct enter_brick_task_struct {
	xSemaphoreHandle sem_start;
	xSemaphoreHandle sem_cylinder_start_start;
	xSemaphoreHandle sem_check_brick_start;
	xSemaphoreHandle sem_cylinder_start_finished;
	xQueueHandle mbx_check_brick;
	xQueueHandle brick_type;
} enter_brick_task_struct;

typedef struct cylinder_start_task_struct {
	xSemaphoreHandle sem_cylinder_start_start;
	xSemaphoreHandle sem_cylinder_start_finished;
} cylinder_start_task_struct;

typedef struct check_brick_task_struct {
	xSemaphoreHandle sem_check_brick_start;
	xQueueHandle mbx_check_brick;
} check_brick_task_struct;

typedef struct Deliver_Brick_struct {
	xSemaphoreHandle sem_press_p;
	xSemaphoreHandle sem_start;
} Deliver_Brick_struct;

typedef struct push_Cylinder1_struct {
	xQueueHandle brick_type;
	xSemaphoreHandle docks;
	xSemaphoreHandle dock2_end;
	xQueueHandle brick_Type_not1;
} push_Cylinder1_struct;

typedef struct push_Cylinder2_struct {
	xSemaphoreHandle dock2_end;
	xQueueHandle brick_Type_not1;
} push_Cylinder2_struct;

typedef struct emergency_struct {
	xQueueHandle ports;
	xSemaphoreHandle end_emerg;
	xTaskHandle vTask_signals_handle;
	xTaskHandle enter_brick_task_handle;
	xTaskHandle cylinder_start_task_handle;
	xTaskHandle check_brick_task_handle;
	xTaskHandle Deliver_Brick_handle;
	xTaskHandle push_Cylinder1_handle;
	xTaskHandle push_Cylinder2_handle;
} emergency_struct;

typedef struct resume_struct {
	xQueueHandle ports;
	xSemaphoreHandle end_emerg;
	xTaskHandle vTask_signals_handle;
	xTaskHandle enter_brick_task_handle;
	xTaskHandle cylinder_start_task_handle;
	xTaskHandle check_brick_task_handle;
	xTaskHandle Deliver_Brick_handle;
	xTaskHandle push_Cylinder1_handle;
	xTaskHandle push_Cylinder2_handle;
} resume_struct;


xQueueHandle docks;
xTaskHandle emergencyTask;
xTaskHandle resumeTask;
bool emergency_stop = FALSE;


void vAssertCalled(unsigned long ulLine, const char* const pcFileName)
{
	static BaseType_t xPrinted = pdFALSE;
	volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;
	/* Called if an assertion passed to configASSERT() fails.  See
	http://www.freertos.org/a00110.html#configASSERT for more information. */
	/* Parameters are not used. */
	(void)ulLine;
	(void)pcFileName;
	printf("ASSERT! Line %ld, file %s, GetLastError() %ld\r\n", ulLine, pcFileName, GetLastError());

	taskENTER_CRITICAL();
	{
		/* Cause debugger break point if being debugged. */
		__debugbreak();
		/* You can step out of this function to debug the assertion by using
		   the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
		   value. */
		while (ulSetToNonZeroInDebuggerToContinue == 0)
		{
			__asm { NOP };
			__asm { NOP };
		}
	}
	taskEXIT_CRITICAL();
}
static void  initialiseHeap(void)
{
	static uint8_t ucHeap[configTOTAL_HEAP_SIZE];
	/* Just to prevent 'condition is always true' warnings in configASSERT(). */
	volatile uint32_t ulAdditionalOffset = 19;
	const HeapRegion_t xHeapRegions[] =
	{
		/* Start address with dummy offsetsSize */
		{ ucHeap + 1,mainREGION_1_SIZE },
		{ ucHeap + 15 + mainREGION_1_SIZE,mainREGION_2_SIZE },
		{ ucHeap + 19 + mainREGION_1_SIZE +
				mainREGION_2_SIZE,mainREGION_3_SIZE },
		{ NULL, 0 }
	};


	configASSERT((ulAdditionalOffset +
		mainREGION_1_SIZE +
		mainREGION_2_SIZE +
		mainREGION_3_SIZE) < configTOTAL_HEAP_SIZE);
	/* Prevent compiler warnings when configASSERT() is not defined. */
	(void)ulAdditionalOffset;
	vPortDefineHeapRegions(xHeapRegions);
}
void inicializarPortos() {
	createDigitalInput(0);
	createDigitalInput(1);
	createDigitalOutput(2);
	writeDigitalU8(2, 0);
}

void vTask_signals(void* pvParameters){
	vTask_signals_struct* my_params = (vTask_signals_struct*)pvParameters;
	xSemaphoreHandle sem_press_p = my_params->sem_press_p;

	printf("write something\n");
	while (1) {
		char ch[101];
		gets_s(ch, 100);  // input from keyboard
		printf("\n vTask_signals got string '%s' from keyboard\n", ch);
		if (strcmp(ch, "s") == 0)
			moveConveyor();
		if (strcmp(ch, "p") == 0) 
			xSemaphoreGive(sem_press_p);
		if (strcmp(ch, "f") == 0)
			exit(0); // terminate program...
	}
}
int ID_Brick() {
	int type = 1;
	bool controls1 = FALSE;
	bool controls2 = FALSE;
	while (!getBitValue(readDigitalU8(0), 0)) {
		uInt8 p1 = readDigitalU8(1);
		if (getBitValue(p1, 6)) {

			if (!controls1) {
				type++;
				controls1 = TRUE;
			}	
		}
		if (getBitValue(p1, 5)) {

			if (!controls2) {
				type++;
				controls2 = TRUE;
			}
		}
		vTaskDelay(10);
	}
	return type;
}
void enter_brick_task(void* pvParameters) {
	enter_brick_task_struct* my_params = (enter_brick_task_struct*)pvParameters;
	xSemaphoreHandle sem_start = my_params->sem_start;
	xSemaphoreHandle sem_cylinder_start_start = my_params->sem_cylinder_start_start;
	xSemaphoreHandle sem_check_brick_start = my_params->sem_check_brick_start;
	xSemaphoreHandle sem_cylinder_start_finished = my_params->sem_cylinder_start_finished;
	xQueueHandle mbx_check_brick = my_params->mbx_check_brick;
	xQueueHandle brick_type = my_params->brick_type;
	int brickType;
	while (TRUE)
	{
		xQueueSemaphoreTake(sem_start, portMAX_DELAY);
		xSemaphoreGive(sem_cylinder_start_start);
		xSemaphoreGive(sem_check_brick_start);
		xQueueSemaphoreTake(sem_cylinder_start_finished, portMAX_DELAY);
		xQueueReceive(mbx_check_brick, &brickType, portMAX_DELAY);

		printf("Brick received, Brick Type : % d\n", brickType);
		xQueueSend(brick_type, &brickType, portMAX_DELAY);
	}
}
void cylinder_start_task(void* pvParameters) {
	cylinder_start_task_struct* my_params = (cylinder_start_task_struct*)pvParameters;
	xSemaphoreHandle sem_cylinder_start_start = my_params->sem_cylinder_start_start;
	xSemaphoreHandle sem_cylinder_start_finished = my_params->sem_cylinder_start_finished;
	while (TRUE) {
		xQueueSemaphoreTake(sem_cylinder_start_start,portMAX_DELAY);
		gotoCylinderStart(1);
		xSemaphoreGive(sem_cylinder_start_finished);
		gotoCylinderStart(0);
	}
}
void check_brick_task(void* pvParameters) {
	check_brick_task_struct* my_params = (check_brick_task_struct*)pvParameters;
	xSemaphoreHandle sem_check_brick_start = my_params->sem_check_brick_start;
	xQueueHandle mbx_check_brick = my_params->mbx_check_brick;
	int brickType;
	while (TRUE) {
		xQueueSemaphoreTake(sem_check_brick_start, portMAX_DELAY);
		brickType = ID_Brick();
		xQueueSend(mbx_check_brick, &brickType, portMAX_DELAY);
	}
}

void Deliver_Brick(void* pvParameters) {
	Deliver_Brick_struct* my_params = (Deliver_Brick_struct*)pvParameters;
	xSemaphoreHandle sem_press_p = my_params->sem_press_p;
	xSemaphoreHandle sem_start = my_params->sem_start;
	while (TRUE) {
		xQueueSemaphoreTake(sem_press_p, portMAX_DELAY);
		xSemaphoreGive(sem_start);
	}
}


void push_Cylinder1(void* pvParameters) {
	push_Cylinder1_struct* my_params = (push_Cylinder1_struct*)pvParameters;
	xQueueHandle brick_type = my_params->brick_type;
	xQueueHandle docks = my_params->docks;
	xSemaphoreHandle dock2_end = my_params->dock2_end;
	xQueueHandle brick_Type_not1 = my_params->brick_Type_not1;
	int dock;
	int brickType;
	int numberCorrectBricks = 0;


	while (TRUE) {
		xQueueReceive(brick_type, &brickType, portMAX_DELAY);

		if (getBitValue(readDigitalU8(0), 0)) {
			
			xQueueReceive(docks, &dock, 10);
			if (dock == 1) {
				stopConveyor();
				gotoCylinder1(1);
				gotoCylinder1(0);
				moveConveyor();
				dock = 0;
				continue;
			}
			else if (dock == 2) {
				xSemaphoreGive(dock2_end);
				dock = 0;
				continue;
			}
			

			if (brickType == 1) {
				stopConveyor();
				gotoCylinder1(1);
				numberCorrectBricks++;
				gotoCylinder1(0);
				moveConveyor();
				
				if (numberCorrectBricks == 3) {
					setLed(1);
					vTaskDelay(500);
					setLed(0);
					numberCorrectBricks = 0;
				}
			}
			else if (brickType != 1) {
				xQueueSend(brick_Type_not1, &brickType, portMAX_DELAY);
			}
				
		}
		vTaskDelay(10);
	}
}

void push_Cylinder2(void* pvParameters) {
	push_Cylinder2_struct* my_params = (push_Cylinder2_struct*)pvParameters;
	xSemaphoreHandle dock2_end = my_params->dock2_end;
	xQueueHandle brick_Type_not1 = my_params->brick_Type_not1;
	int brickType;	
	int numberCorrectBricks = 0;


	while (TRUE) {
		if (xSemaphoreTake(dock2_end, 10) == pdTRUE) {
			while (TRUE) {
				uInt8 p1 = readDigitalU8(1);
				vTaskDelay(10);
				if (getBitValue(p1, 7)) {
					stopConveyor();
					gotoCylinder2(1);
					gotoCylinder2(0);
					moveConveyor();
					break;
				}
			}
		}
		vTaskDelay(10);
		xQueueReceive(brick_Type_not1, &brickType, 10);
		if (brickType == 2) {
			while (TRUE) {
				uInt8 p1 = readDigitalU8(1);
				vTaskDelay(10);
				if (getBitValue(p1, 7)) {
					stopConveyor();
					gotoCylinder2(1);
					numberCorrectBricks++;
					gotoCylinder2(0);
					moveConveyor();
					if (numberCorrectBricks == 3) {
						setLed(1);
						vTaskDelay(500);
						setLed(0);
						numberCorrectBricks = 0;
					}
					break;
				}
			}
			brickType = 0;
			
		}
		else if (brickType == 3) {
			while (TRUE) {
				vTaskDelay(100);
				if (getBitValue(readDigitalU8(1) , 7)) {
					while (getBitValue(readDigitalU8(1), 7)) {
						vTaskDelay(100);
					}
					break;
				}
			}
			brickType = 0;
		}
	}
}

void emergency(void* pvParameters) {
	emergency_struct* my_params = (emergency_struct*)pvParameters;
	xQueueHandle ports = my_params->ports;
	xSemaphoreHandle end_emerg = my_params->end_emerg;

	while (TRUE) {

		vTaskSuspend(NULL);
		printf("EMERGENCY STOP ACTIVATED\n");
			
		uInt8 p = readDigitalU8(0);
		xQueueSend(ports, &p, 0);
		p = readDigitalU8(1);
		xQueueSend(ports, &p, 0);
		p = readDigitalU8(2);
		xQueueSend(ports, &p, 0);

		stopConveyor();
		stopCylinderStart();
		stopCylinder1();
		stopCylinder2();

		vTaskSuspend(my_params->vTask_signals_handle);
		vTaskSuspend(my_params->enter_brick_task_handle);
		vTaskSuspend(my_params->cylinder_start_task_handle);
		vTaskSuspend(my_params->check_brick_task_handle);
		vTaskSuspend(my_params->Deliver_Brick_handle);
		vTaskSuspend(my_params->push_Cylinder1_handle);
		vTaskSuspend(my_params->push_Cylinder2_handle);

		while (xSemaphoreTake(end_emerg, 10) != pdTRUE) {
			setLed(1);
			vTaskDelay(500);
			setLed(0);
			vTaskDelay(500);
		}
	}
}
	
void resume(void* pvParameters) {
	resume_struct* my_params = (resume_struct*)pvParameters;
	xQueueHandle ports = my_params->ports;
	xSemaphoreHandle end_emerg = my_params->end_emerg;

	while (TRUE) {
		vTaskSuspend(NULL);
		printf("EMERGENCY STOP DEACTIVATED\n");
		xSemaphoreGive(end_emerg);

		taskENTER_CRITICAL();
		uInt8 p;
		xQueueReceive(ports, &p, portMAX_DELAY);
		writeDigitalU8(0, p);
		xQueueReceive(ports, &p, portMAX_DELAY);
		writeDigitalU8(1, p);
		xQueueReceive(ports, &p, portMAX_DELAY);
		writeDigitalU8(2, p);
		taskEXIT_CRITICAL();

		vTaskResume(my_params->vTask_signals_handle);
		vTaskResume(my_params->enter_brick_task_handle);
		vTaskResume(my_params->cylinder_start_task_handle);
		vTaskResume(my_params->check_brick_task_handle);
		vTaskResume(my_params->Deliver_Brick_handle);
		vTaskResume(my_params->push_Cylinder1_handle);
		vTaskResume(my_params->push_Cylinder2_handle);

	}
}



void switch1_rising_isr(ULONGLONG lastTime) {
	int dock = 1;
	xQueueSend(docks, &dock, 10);
}

void switch2_rising_isr(ULONGLONG lastTime) {
	int dock = 2;
	xQueueSend(docks, &dock, 10);
}

void switch3_rising_isr(ULONGLONG lastTime){
	if (!emergency_stop) {
		emergency_stop = TRUE;
		BaseType_t xYieldRequired;
		xYieldRequired = xTaskResumeFromISR(emergencyTask);
	}
	else {
		emergency_stop = FALSE;
		BaseType_t xYieldRequired;
		xYieldRequired = xTaskResumeFromISR(resumeTask);
	}	
}

void myDaemonTaskStartupHook(void) {
	inicializarPortos();

	xSemaphoreHandle sem_press_p = xSemaphoreCreateCounting(10, 0);
	xSemaphoreHandle sem_start = xSemaphoreCreateCounting(10, 0);
	xSemaphoreHandle sem_cylinder_start_start = xSemaphoreCreateCounting(10, 0);
	xSemaphoreHandle sem_check_brick_start = xSemaphoreCreateCounting(10, 0);
	xSemaphoreHandle sem_cylinder_start_finished = xSemaphoreCreateCounting(10, 0);
	xSemaphoreHandle dock2_end = xSemaphoreCreateBinary();
	xSemaphoreHandle end_emerg = xSemaphoreCreateBinary();


	xQueueHandle mbx_check_brick = xQueueCreate(10, sizeof(int));
	xQueueHandle brick_type = xQueueCreate(10, sizeof(int));
	xQueueHandle brick_Type_not1 = xQueueCreate(10, sizeof(int));
	docks = xQueueCreate(10, sizeof(int));
	xQueueHandle ports = xQueueCreate(10, sizeof(uInt8));

	xTaskHandle vTask_signals_handle;
	xTaskHandle enter_brick_task_handle;
	xTaskHandle cylinder_start_task_handle;
	xTaskHandle check_brick_task_handle;
	xTaskHandle Deliver_Brick_handle;
	xTaskHandle push_Cylinder1_handle;
	xTaskHandle push_Cylinder2_handle;


	vTask_signals_struct *vTask_signals_Params = (vTask_signals_struct*)pvPortMalloc(sizeof(vTask_signals_struct));
	enter_brick_task_struct *enter_brick_task_Params = (enter_brick_task_struct*)pvPortMalloc(sizeof(enter_brick_task_struct));
	cylinder_start_task_struct *cylinder_start_task_Params = (cylinder_start_task_struct*)pvPortMalloc(sizeof(cylinder_start_task_struct));
	check_brick_task_struct *check_brick_task_Params = (check_brick_task_struct*)pvPortMalloc(sizeof(check_brick_task_struct));
	Deliver_Brick_struct * Deliver_Brick_Params = (Deliver_Brick_struct*)pvPortMalloc(sizeof(Deliver_Brick_struct));
	push_Cylinder1_struct* push_Cylinder1_Params = (push_Cylinder1_struct*)pvPortMalloc(sizeof(push_Cylinder1_struct));
	push_Cylinder2_struct* push_Cylinder2_Params = (push_Cylinder2_struct*)pvPortMalloc(sizeof(push_Cylinder2_struct));
	emergency_struct* emergency_Params = (emergency_struct*)pvPortMalloc(sizeof(emergency_struct));
	resume_struct* resume_Params = (resume_struct*)pvPortMalloc(sizeof(resume_struct));

	vTask_signals_Params->sem_press_p = sem_press_p;
	enter_brick_task_Params->sem_start = sem_start;
	enter_brick_task_Params->sem_cylinder_start_start = sem_cylinder_start_start;
	enter_brick_task_Params->sem_check_brick_start = sem_check_brick_start;
	enter_brick_task_Params->sem_cylinder_start_finished = sem_cylinder_start_finished;
	enter_brick_task_Params->mbx_check_brick = mbx_check_brick;
	enter_brick_task_Params->brick_type = brick_type;
	cylinder_start_task_Params->sem_cylinder_start_start = sem_cylinder_start_start;
	cylinder_start_task_Params->sem_cylinder_start_finished = sem_cylinder_start_finished;
	check_brick_task_Params->sem_check_brick_start = sem_check_brick_start;
	check_brick_task_Params->mbx_check_brick = mbx_check_brick;
	Deliver_Brick_Params->sem_press_p = sem_press_p;
	Deliver_Brick_Params->sem_start = sem_start;
	push_Cylinder1_Params->docks = docks;
	push_Cylinder1_Params->dock2_end = dock2_end;
	push_Cylinder1_Params->brick_type = brick_type;
	push_Cylinder1_Params->brick_Type_not1 = brick_Type_not1;
	push_Cylinder2_Params->dock2_end = dock2_end;
	push_Cylinder2_Params->brick_Type_not1 = brick_Type_not1;
	emergency_Params->ports = ports;
	emergency_Params->end_emerg = end_emerg;
	resume_Params->ports = ports;
	resume_Params->end_emerg = end_emerg;
	


	xTaskCreate(vTask_signals, "vTask_signals", 100, vTask_signals_Params, 0, &vTask_signals_handle);
	xTaskCreate(enter_brick_task, "Enter brick Task", 100, enter_brick_task_Params, 0, &enter_brick_task_handle);
	xTaskCreate(cylinder_start_task, "Cylinder Start Task", 100, cylinder_start_task_Params, 0, &cylinder_start_task_handle);
	xTaskCreate(check_brick_task, "Check brick Task", 100, check_brick_task_Params, 0, &check_brick_task_handle);

	xTaskCreate(Deliver_Brick, "Deliver_Brick", 100, Deliver_Brick_Params, 0, &Deliver_Brick_handle);
	xTaskCreate(push_Cylinder1, "push_Cylinder1", 100, push_Cylinder1_Params, 0, &push_Cylinder1_handle);
	xTaskCreate(push_Cylinder2, "push_Cylinder2", 100, push_Cylinder2_Params, 0, &push_Cylinder2_handle);

	emergency_Params->vTask_signals_handle = vTask_signals_handle;
	emergency_Params->enter_brick_task_handle = enter_brick_task_handle;
	emergency_Params->cylinder_start_task_handle = cylinder_start_task_handle;
	emergency_Params->check_brick_task_handle = check_brick_task_handle;
	emergency_Params->Deliver_Brick_handle = Deliver_Brick_handle;
	emergency_Params->push_Cylinder1_handle = push_Cylinder1_handle;
	emergency_Params->push_Cylinder2_handle = push_Cylinder2_handle;
	resume_Params->vTask_signals_handle = vTask_signals_handle;
	resume_Params->enter_brick_task_handle = enter_brick_task_handle;
	resume_Params->cylinder_start_task_handle = cylinder_start_task_handle;
	resume_Params->check_brick_task_handle = check_brick_task_handle;
	resume_Params->Deliver_Brick_handle = Deliver_Brick_handle;
	resume_Params->push_Cylinder1_handle = push_Cylinder1_handle;
	resume_Params->push_Cylinder2_handle = push_Cylinder2_handle;

	xTaskCreate(emergency, "emergency", 100, emergency_Params, 0, &emergencyTask);
	xTaskCreate(resume, "resume", 100, resume_Params, 0, &resumeTask);

	attachInterrupt(1, 4, switch1_rising_isr, RISING);
	attachInterrupt(1, 3, switch2_rising_isr, RISING);
	attachInterrupt(1, 2, switch3_rising_isr, RISING);
	
}

int main(int argc, char** argv) {
	initialiseHeap();
	
	vApplicationDaemonTaskStartupHook = &myDaemonTaskStartupHook;

	vTaskStartScheduler();

	closeChannels();
	return 0;
}
