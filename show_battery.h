#pragma once

#include <LiquidCrystal_I2C.h>//SCL - A5, SDA - A4

extern volatile unsigned long start_show_battery_time;//интервал замера и отрисовки батареи
extern bool start_show_battery; //старт таймера показа состояния батареи в loop()

//градация напряжений питания
extern long voltage[];

//long readVcc();
void createChars(LiquidCrystal_I2C lcd);
long readVcc();