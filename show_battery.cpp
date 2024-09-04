#include<Arduino.h>
#include "show_battery.h"


volatile unsigned long start_show_battery_time = 0;//интервал замера и отрисовки батареи
bool start_show_battery = true; //для старта таймера показа состояния батареи в loop()
long voltage[] = {4000, 4167, 4334, 4501, 4668, 4835, 5002};
//контроль напряжения
// Создание пользовательских символов для уровней заряда батареи
uint8_t battery0[8] = { // 0% заряда
  0b01110,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b11111
};
uint8_t battery1[8] = { 
  0b01110,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b11001,
  0b11111
};
uint8_t battery2[8] = { 
  0b01110,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b11001,
  0b11101,
  0b11111
};
uint8_t battery3[8] = { 
  0b01110,
  0b10001,
  0b10001,
  0b10001,
  0b11001,
  0b11101,
  0b11111,
  0b11111
};
uint8_t battery4[8] = { 
  0b01110,
  0b10001,
  0b10001,
  0b11001,
  0b11101,
  0b11111,
  0b11111,
  0b11111
};
uint8_t battery5[8] = { 
  0b01110,
  0b10001,
  0b11001,
  0b11101,
  0b11111,
  0b11111,
  0b11111,
  0b11111
};
uint8_t battery6[8] = { 
  0b01110,
  0b11001,
  0b11101,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111  
};

uint8_t battery7[8] = { //100% 
  0b01110,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111
};


void createChars(LiquidCrystal_I2C lcd) {
  lcd.createChar(0, battery0);
  lcd.createChar(1, battery1);
  lcd.createChar(2, battery2);
  lcd.createChar(3, battery3);
  lcd.createChar(4, battery4);
  lcd.createChar(5, battery5);
  lcd.createChar(6, battery6);
  lcd.createChar(7, battery7);  
}

long readVcc() {
  // Устанавливаем опорное напряжение на 1.1 В
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Даем время для стабилизации

  // Запускаем преобразование
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC));

  // Считываем значение АЦП и вычисляем напряжение
  long vcc = ADCL;
  vcc |= ADCH << 8;
  vcc = 1126400L / vcc; // 1.1V * 1024 * 1000 / result

  return vcc; // Возвращаем напряжение в мВ
}