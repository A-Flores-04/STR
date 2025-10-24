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
	xSemaphoreHandle show_stats;
	xSemaphoreHandle show_stats_done;
	xSemaphoreHandle emergency_sem_on;
	xSemaphoreHandle emergency_sem_off;
} vTask_signals_struct;

typedef struct enter_brick_task_struct {
	xSemaphoreHandle sem_start;
	xSemaphoreHandle sem_cylinder_start_start;
	xSemaphoreHandle sem_check_brick_start;
	xSemaphoreHandle sem_cylinder_start_finished;
	xQueueHandle mbx_check_brick;
	xQueueHandle brick_type;
	xQueueHandle brick_type_to_statistic;
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

typedef struct cylinder1_struct {
	xQueueHandle brick_type;
	xQueueHandle docks;
	xSemaphoreHandle start_led;
	xQueueHandle one_to_two;
	xQueueHandle overrides;
	xQueueHandle bricksDock1;
	xSemaphoreHandle complete_sequences1;
} cylinder1_struct;

typedef struct cylinder2_struct {
	xQueueHandle brick_type;
	xQueueHandle docks;
	xSemaphoreHandle start_led;
	xQueueHandle one_to_two;
	xQueueHandle overrides;
	xQueueHandle bricksDock2;
	xQueueHandle brickDockEnd;
	xSemaphoreHandle complete_sequences2;
} cylinder2_struct;	

typedef struct StatisticTask_struct {
	xQueueHandle brick_type_to_statistic;
	xQueueHandle overrides;
	xQueueHandle bricksDock1;
	xQueueHandle bricksDock2;
	xQueueHandle brickDockEnd;
	xQueueHandle in_emergency;
	xSemaphoreHandle show_stats;
	xSemaphoreHandle show_stats_done;
	xSemaphoreHandle complete_sequences1;
	xSemaphoreHandle complete_sequences2;
} StatisticTask_struct;

typedef struct LedTask_struct {
	xSemaphoreHandle start_led;
} LedTask_struct;

typedef struct emergency_struct {
	xQueueHandle ports;
	xSemaphoreHandle end_emerg;
	xTaskHandle enter_brick_task_handle;
	xTaskHandle cylinder_start_task_handle;
	xTaskHandle check_brick_task_handle;
	xTaskHandle Deliver_Brick_handle;
	xTaskHandle cylinder1_handle;
	xTaskHandle cylinder2_handle;
	xTaskHandle LedTask_handle;
	xQueueHandle in_emergency;
	xSemaphoreHandle emergency_sem_on;
} emergency_struct;

typedef struct resume_struct {
	xQueueHandle ports;
	xSemaphoreHandle end_emerg;
	xTaskHandle enter_brick_task_handle;
	xTaskHandle cylinder_start_task_handle;
	xTaskHandle check_brick_task_handle;
	xTaskHandle Deliver_Brick_handle;
	xTaskHandle cylinder1_handle;
	xTaskHandle cylinder2_handle;
	xTaskHandle LedTask_handle;
	xQueueHandle in_emergency;
	xSemaphoreHandle emergency_sem_off;
} resume_struct;


xQueueHandle docks;
xTaskHandle emergencyTask;
xTaskHandle resumeTask;
bool emergency_stop = FALSE;
bool resume_var = FALSE;


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

void CalibracaoInicial() {
	while (TRUE) {
		printf("\n\n--- Calibration Menu ---\n");
		printf("Type 'q' to calibrate Cylinder 1 Front\nType 'w' to calibrate Cylinder 1 Back\nType 'e' to calibrate Cylinder 2 Front\nType 'r' to calibrate Cylinder 2 Back\nType 't' to calibrate Cylinder Start Front\nType 'y' to calibrate Cylinder Start Back\nType 'f' to finish calibration\n");
		char ch[101];
		gets_s(ch, 100);
		if (strcmp(ch, "q") == 0) {
			while(getCylinder1Pos() != 0) 
				moveCylinder1Front();
			stopCylinder1();
		}
		if (strcmp(ch, "w") == 0) {
			while (getCylinder1Pos() != 0)
				moveCylinder1Back();
			stopCylinder1();
		}
		if (strcmp(ch, "e") == 0) {
			while (getCylinder2Pos() != 0)
				moveCylinder2Front();
			stopCylinder2();
		}
		if (strcmp(ch, "r") == 0) {
			while (getCylinder2Pos() != 0)
				moveCylinder2Back();
			stopCylinder2();
		}
		if (strcmp(ch, "t") == 0) {
			while (getCylinderStartPos() != 0)
				moveCylinderStartFront();
			stopCylinderStart();
		}
		if (strcmp(ch, "y") == 0) {
			while (getCylinderStartPos() != 0)
				moveCylinderStartBack();
			stopCylinderStart();
		}
		if (strcmp(ch, "f") == 0) {
			return;
		}	
	}
}

void vTask_signals(void* pvParameters){
	vTask_signals_struct* my_params = (vTask_signals_struct*)pvParameters;
	xSemaphoreHandle sem_press_p = my_params->sem_press_p;
	xSemaphoreHandle show_stats = my_params->show_stats;
	xSemaphoreHandle show_stats_done = my_params->show_stats_done;
	xSemaphoreHandle emergency_sem_on = my_params->emergency_sem_on;
	xSemaphoreHandle emergency_sem_off = my_params->emergency_sem_off;
	

	bool inEmerg = FALSE;

	while (1) {
		printf("\n\n-----------MENU-----------\n");
		printf("Type 's' to move conveyor\nType 'p' to deliver a brick\nType 'e' to show statistics\nType 'c' to calibrate\nType 'f' to finish\n");
		printf("--------------------------\n");
		char ch[101];
		gets_s(ch, 100);  // input from keyboard
		printf("\n vTask_signals got string '%s' from keyboard\n", ch);
		if(xSemaphoreTake(emergency_sem_on, 0) == pdTRUE){
			inEmerg = TRUE;
		}
		if(xSemaphoreTake(emergency_sem_off, 0) == pdTRUE){
			inEmerg = FALSE;
		}
		if (strcmp(ch, "s") == 0) {
			if (!inEmerg) 
				moveConveyor();
			else
				printf("\nCan not move conveyor in emergency mode\n");
		}
		if (strcmp(ch, "p") == 0) {
			if (!inEmerg) 
				xSemaphoreGive(sem_press_p);
			else
				printf("\nCan not deliver a brick in emergency mode\n");
		}
		if (strcmp(ch, "e") == 0) {
			xSemaphoreGive(show_stats);
			xSemaphoreTake(show_stats_done, portMAX_DELAY);
		}	
		if (strcmp(ch, "c") == 0) {
			if(!inEmerg)
				CalibracaoInicial();
			else
				printf("\nCan not calibrate in emergency mode\n");
		}
		if (strcmp(ch, "f") == 0)
			exit(0); // terminate program...
	}
}
int ID_Brick() {
	int type = 1;
	bool controls1 = FALSE;
	bool controls2 = FALSE;
	ULONGLONG  time = GetTickCount64();
	TickType_t maxtime_permited = pdMS_TO_TICKS(5000);
	while (!getBitValue(readDigitalU8(0), 0)) {
		vTaskDelay(10);
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
		if (resume_var) {
			time = GetTickCount64();
			resume_var = FALSE;
		}

		if ((GetTickCount64() - time) > maxtime_permited) 
			return 0;
			
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
	xQueueHandle brick_type_to_statistic = my_params->brick_type_to_statistic;
	int brickType;
	while (TRUE)
	{
		xQueueSemaphoreTake(sem_start, portMAX_DELAY);
		xSemaphoreGive(sem_cylinder_start_start);
		xSemaphoreGive(sem_check_brick_start);
		xQueueSemaphoreTake(sem_cylinder_start_finished, portMAX_DELAY);
		xQueueReceive(mbx_check_brick, &brickType, portMAX_DELAY);
		if (brickType == 0)
			continue;

		xQueueSend(brick_type, &brickType, portMAX_DELAY);
		xQueueSend(brick_type_to_statistic, &brickType, portMAX_DELAY);
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



void cylinder1(void* pvParameters) {
	cylinder1_struct* my_params = (cylinder1_struct*)pvParameters;
	xQueueHandle brick_type = my_params->brick_type;
	xQueueHandle docks = my_params->docks;
	xSemaphoreHandle start_led = my_params->start_led;
	xQueueHandle one_to_two = my_params->one_to_two;
	xQueueHandle overrides = my_params->overrides;
	xQueueHandle bricksDock1 = my_params->bricksDock1;
	xSemaphoreHandle complete_sequences1 = my_params->complete_sequences1;

	int brickType = 0;
	int dock = 0;
	int numberCorrectBricks = 0;

	while (TRUE) {
		vTaskDelay(10);
		if (getBitValue(readDigitalU8(0), 0)) {
			xQueueReceive(brick_type, &brickType, portMAX_DELAY);
			xQueuePeek(docks, &dock, 10);
			
			if (dock == 1) {
				stopConveyor();
				gotoCylinder1(1);
				gotoCylinder1(0);
				moveConveyor();
				if (brickType == 1) {
					numberCorrectBricks++;
					if (numberCorrectBricks == 3) {
						xSemaphoreGive(start_led);
						xSemaphoreGive(complete_sequences1);
						numberCorrectBricks = 0;
					}
				}
				xQueueSend(bricksDock1, &brickType, 10);
				brickType = 0;
				xQueueReceive(docks, &dock, 10);
				xQueueSend(overrides, &dock, 10);
				dock = 0;
			}
			else if (dock == 2) {
				xQueueSend(one_to_two, &brickType, 10);
				brickType = 0;
				dock = 0;
			}
			if (brickType == 1) {
				numberCorrectBricks++;
				stopConveyor();
				gotoCylinder1(1);
				gotoCylinder1(0);
				moveConveyor();
				if (numberCorrectBricks == 3) {
					xSemaphoreGive(start_led);
					xSemaphoreGive(complete_sequences1);
					numberCorrectBricks = 0;
				}
				xQueueSend(bricksDock1, &brickType, 10);
				brickType = 0;
			}
			if (brickType > 1) {
				xQueueSend(one_to_two, &brickType, 10);
				brickType = 0;
			}
		}
	}
}

void cylinder2(void* pvParameters) {
	cylinder2_struct* my_params = (cylinder2_struct*)pvParameters;
	xQueueHandle brick_type = my_params->brick_type;
	xQueueHandle docks = my_params->docks;
	xSemaphoreHandle start_led = my_params->start_led;
	xQueueHandle one_to_two = my_params->one_to_two;
	xQueueHandle overrides = my_params->overrides;
	xQueueHandle bricksDock2 = my_params->bricksDock2;
	xQueueHandle brickDockEnd = my_params->brickDockEnd;
	xSemaphoreHandle complete_sequences2 = my_params->complete_sequences2;

	int brickType;
	int dock;
	int numberCorrectBricks = 0;

	while (TRUE) {
		vTaskDelay(10);
		if (getBitValue(readDigitalU8(1), 7)) {
			xQueueReceive(one_to_two, &brickType, 10);
			xQueuePeek(docks, &dock, 10);

			if ((brickType == 2) || (dock == 2)) {
				stopConveyor();
				gotoCylinder2(1);
				gotoCylinder2(0);
				moveConveyor();
				if (brickType == 2) {
					numberCorrectBricks++;
					if (numberCorrectBricks == 3) {
						xSemaphoreGive(start_led);
						xSemaphoreGive(complete_sequences2);
						numberCorrectBricks = 0;
					}
				}
				xQueueSend(bricksDock2, &brickType, 10);
				if (dock == 2) {
					xQueueReceive(docks, &dock, 10);
					xQueueSend(overrides, &dock, 10);
				}
				brickType = 0;
				dock = 0;
			}
			else if (brickType == 3) {
				while (getBitValue(readDigitalU8(1), 7)) {
					vTaskDelay(100);
				}
				for (int i = 0; i < 3; i++) {
					xSemaphoreGive(start_led);
				}
				xQueueSend(brickDockEnd, &brickType, 10);
				brickType = 0;
			}

		}
	}
}

void StatisticTask(void* pvParameters) {
	StatisticTask_struct* my_params = (StatisticTask_struct*)pvParameters;
	xQueueHandle brick_type_to_statistic = my_params->brick_type_to_statistic;
	xQueueHandle overrides = my_params->overrides;
	xQueueHandle bricksDock1 = my_params->bricksDock1;
	xQueueHandle bricksDock2 = my_params->bricksDock2;
	xQueueHandle brickDockEnd = my_params->brickDockEnd;
	xQueueHandle in_emergency = my_params->in_emergency;
	xSemaphoreHandle show_stats = my_params->show_stats;
	xSemaphoreHandle show_stats_done = my_params->show_stats_done;
	xSemaphoreHandle complete_sequences1 = my_params->complete_sequences1;
	xSemaphoreHandle complete_sequences2 = my_params->complete_sequences2;

	int begining_brickType[3] = { 0 };
	int number_overrides[2] = { 0 };
	int dock1[3] = { 0 };
	int dock2[3] = { 0 };
	int dockEnd[3] = { 0 };
	int complete_sequences[2] = { 0 };
	int brickType;
	int dock;
	bool inEmerg = FALSE;

	while (TRUE) {
		if (xQueueReceive(brick_type_to_statistic, &brickType, 10) == pdTRUE) {
			begining_brickType[brickType-1]++;
		}
		if (xQueueReceive(overrides, &dock, 10) == pdTRUE) {
			number_overrides[dock-1]++;
		}
		if(xQueueReceive(bricksDock1, &brickType, 10) == pdTRUE){
			dock1[brickType - 1]++;
		}
		if (xQueueReceive(bricksDock2, &brickType, 10) == pdTRUE) {
			dock2[brickType - 1]++;
		}
		if (xQueueReceive(brickDockEnd, &brickType, 10) == pdTRUE) {
			dockEnd[brickType - 1]++;
		}
		if (xSemaphoreTake(complete_sequences1, 10) == pdTRUE) {
			complete_sequences[0]++;
		}
		if (xSemaphoreTake(complete_sequences2, 10) == pdTRUE) {
			complete_sequences[1]++;
		}
		xQueueReceive(in_emergency, &inEmerg, 10);

		if (xSemaphoreTake(show_stats, 10) == pdTRUE) {
			printf("\n\n--- Statistics ---\n\n");
			if (inEmerg)
				printf("System in EMERGENCY state!\n");
			else
				printf("System in NORMAL state!\n");
			printf("Bricks entered: Type 1: %d, Type 2: %d, Type 3: %d\n", begining_brickType[0], begining_brickType[1], begining_brickType[2]);
			printf("Number of overrides: Dock 1: %d, Dock 2: %d\n", number_overrides[0], number_overrides[1]);
			printf("Bricks at Dock 1: Type 1: %d, Type 2: %d, Type 3: %d\n", dock1[0], dock1[1], dock1[2]);
			printf("Bricks at Dock 2: Type 1: %d, Type 2: %d, Type 3: %d\n", dock2[0], dock2[1], dock2[2]);
			printf("Bricks at Dock End: Type 1: %d, Type 2: %d, Type 3: %d\n", dockEnd[0], dockEnd[1], dockEnd[2]);
			printf("Complete sequences at Dock 1: %d\n", complete_sequences[0]);
			printf("Complete sequences at Dock 2: %d\n", complete_sequences[1]);
			printf("Total complete sequences: %d\n", complete_sequences[0] + complete_sequences[1]);
			xSemaphoreGive(show_stats_done);
		}
		
		
		vTaskDelay(100);

	}
}

void LedTask(void* pvParameters) {
	LedTask_struct* my_params = (LedTask_struct*)pvParameters;
	xSemaphoreHandle start_led = my_params->start_led;

	while (TRUE) {
		xQueueSemaphoreTake(start_led, portMAX_DELAY);
		setLed(1);
		vTaskDelay(200);
		setLed(0);
		vTaskDelay(200);
	}

}

void emergency(void* pvParameters) {
	emergency_struct* my_params = (emergency_struct*)pvParameters;
	xQueueHandle ports = my_params->ports;
	xSemaphoreHandle end_emerg = my_params->end_emerg;
	xQueueHandle in_emergency = my_params->in_emergency;
	xSemaphoreHandle emergency_sem_on = my_params->emergency_sem_on;
	
	bool inEmerg = TRUE;

	while (TRUE) {

		vTaskSuspend(NULL);
		
		xQueueSend(in_emergency, &inEmerg, 0);
		xSemaphoreGive(emergency_sem_on);

			
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

		vTaskSuspend(my_params->enter_brick_task_handle);
		vTaskSuspend(my_params->cylinder_start_task_handle);
		vTaskSuspend(my_params->check_brick_task_handle);
		vTaskSuspend(my_params->Deliver_Brick_handle);
		vTaskSuspend(my_params->cylinder1_handle);
		vTaskSuspend(my_params->cylinder2_handle);
		vTaskSuspend(my_params->LedTask_handle);
		
		while (xSemaphoreTake(end_emerg, 10) != pdTRUE) {
			setLed(1);
			vTaskDelay(pdMS_TO_TICKS(500));
			setLed(0);
			vTaskDelay(pdMS_TO_TICKS(500));
		}
	}
}
	
void resume(void* pvParameters) {
	resume_struct* my_params = (resume_struct*)pvParameters;
	xQueueHandle ports = my_params->ports;
	xSemaphoreHandle end_emerg = my_params->end_emerg;
	xQueueHandle in_emergency = my_params->in_emergency;
	xSemaphoreHandle emergency_sem_off = my_params->emergency_sem_off;

	bool inEmerg = FALSE;

	while (TRUE) {
		vTaskSuspend(NULL);

		xSemaphoreGive(end_emerg);
		xQueueSend(in_emergency, &inEmerg, 0);
		xSemaphoreGive(emergency_sem_off);

		taskENTER_CRITICAL();
		uInt8 p;
		xQueueReceive(ports, &p, portMAX_DELAY);
		writeDigitalU8(0, p);
		xQueueReceive(ports, &p, portMAX_DELAY);
		writeDigitalU8(1, p);
		xQueueReceive(ports, &p, portMAX_DELAY);
		writeDigitalU8(2, p);
		taskEXIT_CRITICAL();

		vTaskResume(my_params->enter_brick_task_handle);
		vTaskResume(my_params->cylinder_start_task_handle);
		vTaskResume(my_params->check_brick_task_handle);
		vTaskResume(my_params->Deliver_Brick_handle);
		vTaskResume(my_params->cylinder1_handle);
		vTaskResume(my_params->cylinder2_handle);
		vTaskResume(my_params->LedTask_handle);

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
		resume_var = TRUE;
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
	xSemaphoreHandle start_led = xSemaphoreCreateCounting(10, 0);
	xSemaphoreHandle show_stats = xSemaphoreCreateCounting(10, 0);
	xSemaphoreHandle show_stats_done = xSemaphoreCreateCounting(10, 0);
	xSemaphoreHandle emergency_sem_on = xSemaphoreCreateBinary();
	xSemaphoreHandle emergency_sem_off = xSemaphoreCreateBinary();
	xSemaphoreHandle complete_sequences1 = xSemaphoreCreateBinary();
	xSemaphoreHandle complete_sequences2 = xSemaphoreCreateBinary();
	
	xQueueHandle mbx_check_brick = xQueueCreate(10, sizeof(int));
	xQueueHandle brick_type = xQueueCreate(10, sizeof(int));
	xQueueHandle brick_Type_not1 = xQueueCreate(10, sizeof(int));
	docks = xQueueCreate(10, sizeof(int));
	xQueueHandle ports = xQueueCreate(10, sizeof(uInt8));
	xQueueHandle one_to_two = xQueueCreate(10, sizeof(int));
	xQueueHandle brick_type_to_statistic = xQueueCreate(10, sizeof(int));
	xQueueHandle overrides = xQueueCreate(10, sizeof(int));
	xQueueHandle bricksDock1 = xQueueCreate(10, sizeof(int));
	xQueueHandle bricksDock2 = xQueueCreate(10, sizeof(int));
	xQueueHandle brickDockEnd = xQueueCreate(10, sizeof(int));
	xQueueHandle in_emergency = xQueueCreate(10, sizeof(bool));

	xTaskHandle vTask_signals_handle;
	xTaskHandle enter_brick_task_handle;
	xTaskHandle cylinder_start_task_handle;
	xTaskHandle check_brick_task_handle;
	xTaskHandle Deliver_Brick_handle;
	xTaskHandle cylinder1_handle;
	xTaskHandle cylinder2_handle;
	xTaskHandle LedTask_handle;
	


	vTask_signals_struct *vTask_signals_Params = (vTask_signals_struct*)pvPortMalloc(sizeof(vTask_signals_struct));
	enter_brick_task_struct *enter_brick_task_Params = (enter_brick_task_struct*)pvPortMalloc(sizeof(enter_brick_task_struct));
	cylinder_start_task_struct *cylinder_start_task_Params = (cylinder_start_task_struct*)pvPortMalloc(sizeof(cylinder_start_task_struct));
	check_brick_task_struct *check_brick_task_Params = (check_brick_task_struct*)pvPortMalloc(sizeof(check_brick_task_struct));
	Deliver_Brick_struct * Deliver_Brick_Params = (Deliver_Brick_struct*)pvPortMalloc(sizeof(Deliver_Brick_struct));
	cylinder1_struct* cylinder1_Params = (cylinder1_struct*)pvPortMalloc(sizeof(cylinder1_struct));
	cylinder2_struct* cylinder2_Params = (cylinder2_struct*)pvPortMalloc(sizeof(cylinder2_struct));
	LedTask_struct* LedTask_Params = (LedTask_struct*)pvPortMalloc(sizeof(LedTask_struct));
	StatisticTask_struct* StatisticTask_Params = (StatisticTask_struct*)pvPortMalloc(sizeof(StatisticTask_struct));

	emergency_struct* emergency_Params = (emergency_struct*)pvPortMalloc(sizeof(emergency_struct));
	resume_struct* resume_Params = (resume_struct*)pvPortMalloc(sizeof(resume_struct));

	vTask_signals_Params->sem_press_p = sem_press_p;
	vTask_signals_Params->show_stats = show_stats;
	vTask_signals_Params->show_stats_done = show_stats_done;
	vTask_signals_Params->emergency_sem_on = emergency_sem_on;
	vTask_signals_Params->emergency_sem_off = emergency_sem_off;
	enter_brick_task_Params->sem_start = sem_start;
	enter_brick_task_Params->sem_cylinder_start_start = sem_cylinder_start_start;
	enter_brick_task_Params->sem_check_brick_start = sem_check_brick_start;
	enter_brick_task_Params->sem_cylinder_start_finished = sem_cylinder_start_finished;
	enter_brick_task_Params->mbx_check_brick = mbx_check_brick;
	enter_brick_task_Params->brick_type = brick_type;
	enter_brick_task_Params->brick_type_to_statistic = brick_type_to_statistic;
	cylinder_start_task_Params->sem_cylinder_start_start = sem_cylinder_start_start;
	cylinder_start_task_Params->sem_cylinder_start_finished = sem_cylinder_start_finished;
	check_brick_task_Params->sem_check_brick_start = sem_check_brick_start;
	check_brick_task_Params->mbx_check_brick = mbx_check_brick;
	Deliver_Brick_Params->sem_press_p = sem_press_p;
	Deliver_Brick_Params->sem_start = sem_start;
	cylinder1_Params->brick_type = brick_type;
	cylinder1_Params->docks = docks;
	cylinder1_Params->start_led = start_led;
	cylinder1_Params->one_to_two = one_to_two;
	cylinder1_Params->overrides = overrides;
	cylinder1_Params->bricksDock1 = bricksDock1;
	cylinder1_Params->complete_sequences1 = complete_sequences1;
	cylinder2_Params->brick_type = brick_type;
	cylinder2_Params->docks = docks;
	cylinder2_Params->start_led = start_led;
	cylinder2_Params->one_to_two = one_to_two;
	cylinder2_Params->overrides = overrides;
	cylinder2_Params->bricksDock2 = bricksDock2;
	cylinder2_Params->brickDockEnd = brickDockEnd;
	cylinder2_Params->complete_sequences2 = complete_sequences2;
	LedTask_Params->start_led = start_led;
	StatisticTask_Params->brick_type_to_statistic = brick_type_to_statistic;
	StatisticTask_Params->overrides = overrides;
	StatisticTask_Params->bricksDock1 = bricksDock1;
	StatisticTask_Params->bricksDock2 = bricksDock2;
	StatisticTask_Params->brickDockEnd = brickDockEnd;
	StatisticTask_Params->in_emergency = in_emergency;
	StatisticTask_Params->show_stats = show_stats;
	StatisticTask_Params->show_stats_done = show_stats_done;
	StatisticTask_Params->complete_sequences1 = complete_sequences1;
	StatisticTask_Params->complete_sequences2 = complete_sequences2;

	emergency_Params->ports = ports;
	emergency_Params->end_emerg = end_emerg;
	emergency_Params->in_emergency = in_emergency;
	emergency_Params->emergency_sem_on = emergency_sem_on;
	resume_Params->ports = ports;
	resume_Params->end_emerg = end_emerg;
	resume_Params->in_emergency = in_emergency;
	resume_Params->emergency_sem_off = emergency_sem_off;
	


	xTaskCreate(vTask_signals, "vTask_signals", 100, vTask_signals_Params, 0, &vTask_signals_handle);
	xTaskCreate(enter_brick_task, "Enter brick Task", 100, enter_brick_task_Params, 0, &enter_brick_task_handle);
	xTaskCreate(cylinder_start_task, "Cylinder Start Task", 100, cylinder_start_task_Params, 0, &cylinder_start_task_handle);
	xTaskCreate(check_brick_task, "Check brick Task", 100, check_brick_task_Params, 0, &check_brick_task_handle);

	xTaskCreate(Deliver_Brick, "Deliver_Brick", 100, Deliver_Brick_Params, 0, &Deliver_Brick_handle);
	xTaskCreate(cylinder1, "cylinder1", 100, cylinder1_Params, 0, &cylinder1_handle);
	xTaskCreate(cylinder2, "cylinder2", 100, cylinder2_Params, 0, &cylinder2_handle);
	xTaskCreate(LedTask, "LedTask", 100, LedTask_Params, 0, &LedTask_handle);
	xTaskCreate(StatisticTask, "StatisticTask", 100, StatisticTask_Params, 0, NULL);

	emergency_Params->enter_brick_task_handle = enter_brick_task_handle;
	emergency_Params->cylinder_start_task_handle = cylinder_start_task_handle;
	emergency_Params->check_brick_task_handle = check_brick_task_handle;
	emergency_Params->Deliver_Brick_handle = Deliver_Brick_handle;
	emergency_Params->cylinder1_handle = cylinder1_handle;
	emergency_Params->cylinder2_handle = cylinder2_handle;
	emergency_Params->LedTask_handle = LedTask_handle;
	
	resume_Params->enter_brick_task_handle = enter_brick_task_handle;
	resume_Params->cylinder_start_task_handle = cylinder_start_task_handle;
	resume_Params->check_brick_task_handle = check_brick_task_handle;
	resume_Params->Deliver_Brick_handle = Deliver_Brick_handle;
	resume_Params->cylinder1_handle = cylinder1_handle;
	resume_Params->cylinder2_handle = cylinder2_handle;
	resume_Params->LedTask_handle = LedTask_handle;

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
