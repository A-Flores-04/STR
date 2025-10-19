#include "my_interaction_functions.h"

extern "C" {
    #include <FreeRTOS.h>
    #include <task.h>
    #include <timers.h>
    #include <semphr.h>
    #include <interface.h>	
    #include <interrupts.h>
}

int getBitValue(uInt8 value, uInt8 bit_n) // given a byte value, returns the value of its bit n
{
    return(value & (1 << bit_n));
}
void setBitValue(uInt8* variable, int n_bit, int new_value_bit) // given a byte value, set the n bit to value
{
    uInt8 mask_on = (uInt8)(1 << n_bit);
    uInt8 mask_off = ~mask_on;
    if (new_value_bit) *variable |= mask_on;
    else *variable &= mask_off;
}
// CylinderStart

void moveCylinderStartFront()
{
    taskENTER_CRITICAL();
    uInt8 p = readDigitalU8(2); // read port 2
    setBitValue(&p, 0, 0);      // set bit 0 to low level
    setBitValue(&p, 1, 1);      // set bit 1 to high level
    writeDigitalU8(2, p);       // update port 2
    taskEXIT_CRITICAL();
}
void moveCylinderStartBack()
{
    taskENTER_CRITICAL();
    uInt8 p = readDigitalU8(2); // read port 2
    setBitValue(&p, 0, 1);      // set bit 0 to high level
    setBitValue(&p, 1, 0);      // set bit 1 to low level
    writeDigitalU8(2, p);     
    taskEXIT_CRITICAL();
}
void stopCylinderStart()
{
    taskENTER_CRITICAL();
    uInt8 p = readDigitalU8(2); // read port 2
    setBitValue(&p, 0, 0);      // set bit 0 to low level
    setBitValue(&p, 1, 0);      // set bit 1 to low level
    writeDigitalU8(2, p);       // update port 2
    taskEXIT_CRITICAL();
}
int getCylinderStartPos()
{
    uInt8 p0 = readDigitalU8(0);
    if (getBitValue(p0, 6))
        return 0;
    if (getBitValue(p0, 5))
        return 1;
    return(-1);
}
void gotoCylinderStart(int pos)
{
    if (getCylinderStartPos() != pos) {
        if (pos)
            moveCylinderStartFront();
        if (!pos)
            moveCylinderStartBack();
        while (getCylinderStartPos() != pos) {
            vTaskDelay(10);
        }
        stopCylinderStart();
    }
    else 
        return;
}

// Cylinder1

void moveCylinder1Front()
{
    taskENTER_CRITICAL();
    uInt8 p = readDigitalU8(2); // read port 2
    setBitValue(&p, 3, 0);      // set bit 3 to low level
    setBitValue(&p, 4, 1);      // set bit 4 to high level
    writeDigitalU8(2, p);       // update port 2
    taskEXIT_CRITICAL();
}
void moveCylinder1Back()
{
    taskENTER_CRITICAL();
    uInt8 p = readDigitalU8(2); // read port 2
    setBitValue(&p, 4, 0);      // set bit 4 to low level
    setBitValue(&p, 3, 1);      // set bit 3 to high level
    writeDigitalU8(2, p);       // update port 2
    taskEXIT_CRITICAL();
}
void stopCylinder1()
{
    taskENTER_CRITICAL();
    uInt8 p = readDigitalU8(2); // read port 2
    setBitValue(&p, 3, 0);      // set bit 3 to low level
    setBitValue(&p, 4, 0);      // set bit 4 to low level
    writeDigitalU8(2, p);       // update port 2
    taskEXIT_CRITICAL();
}
int getCylinder1Pos()
{
    uInt8 p0 = readDigitalU8(0);
    if (!getBitValue(p0, 4))
        return 0;
    if (!getBitValue(p0, 3))
        return 1;
    return(-1);
}
void gotoCylinder1(int pos)
{
    if (getCylinder1Pos() != pos) {
        if (pos)
            moveCylinder1Front();
        if (!pos)
            moveCylinder1Back();
        while (getCylinder1Pos() != pos) {
            vTaskDelay(10);
        }
        stopCylinder1();
    }
    else
        return;
}

// Cylinder2

void moveCylinder2Front()
{
    taskENTER_CRITICAL();
    uInt8 p = readDigitalU8(2); // read port 2
    setBitValue(&p, 5, 0);      // set bit 5 to low level
    setBitValue(&p, 6, 1);      // set bit 6 to high level
    writeDigitalU8(2, p);       // update port 2
    taskEXIT_CRITICAL();
}
void moveCylinder2Back()
{
    taskENTER_CRITICAL();
    uInt8 p = readDigitalU8(2); // read port 2
    setBitValue(&p, 6, 0);      // set bit 6 to low level
    setBitValue(&p, 5, 1);      // set bit 5 to high level
    writeDigitalU8(2, p);       // update port 2
    taskEXIT_CRITICAL();
}
void stopCylinder2()
{
    taskENTER_CRITICAL();
    uInt8 p = readDigitalU8(2); // read port 2
    setBitValue(&p, 5, 0);      // set bit 5 to low level
    setBitValue(&p, 6, 0);      // set bit 6 to low level
    writeDigitalU8(2, p);       // update port 2
    taskEXIT_CRITICAL();
}
int getCylinder2Pos()
{
    uInt8 p0 = readDigitalU8(0);
    if (!getBitValue(p0, 2))
        return 0;
    if (!getBitValue(p0, 1))
        return 1;
    return(-1);
}
void gotoCylinder2(int pos)
{
    if (getCylinder2Pos() != pos) {
        if (pos)
            moveCylinder2Front();
        if (!pos)
            moveCylinder2Back();
        while (getCylinder2Pos() != pos) {
            vTaskDelay(10);
        }
        stopCylinder2();
    }
    else
        return;
}

// Conveyor

void moveConveyor() 
{
    taskENTER_CRITICAL();
    uInt8 p = readDigitalU8(2); // read port 2
    setBitValue(&p, 2, 1);      // set bit 2 to high level
    writeDigitalU8(2, p);       // update port 2
    taskEXIT_CRITICAL();
}
void stopConveyor() 
{
    taskENTER_CRITICAL();
    uInt8 p = readDigitalU8(2); // read port 2
    setBitValue(&p, 2, 0);      // set bit 2 to low level
    writeDigitalU8(2, p);       // update port 2
    taskEXIT_CRITICAL();
}

// LED

void setLed(int on) 
{
    taskENTER_CRITICAL();
    uInt8 p = readDigitalU8(2); // read port 2
    setBitValue(&p, 7, on);         // set bit 7 to high or low level
    writeDigitalU8(2, p);       // update port 2
    taskEXIT_CRITICAL();
}

