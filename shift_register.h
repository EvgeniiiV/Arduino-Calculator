#pragma once

#include <Keypad.h>


const int latchPin = A1;  //RCLK
const int clockPin = A2;  //SRCLK
const int dataPin = A3;   //SER
const int ninthLedPin = 3;  // Пин для 9-го светодиода

void updateShiftRegister(int numLeds);
