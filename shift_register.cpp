#include "shift_register.h"

void updateShiftRegister(int numLeds) {
  
  byte leds = 0;

  for (int i = 0; i < numLeds && i < 8; i++) {
    leds |= (1 << i);
  }

  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, leds);
  digitalWrite(latchPin, HIGH);

  // Управляем 9-м светодиодом отдельно
  if (numLeds == 9) {
    digitalWrite(ninthLedPin, HIGH);
  } else {
    digitalWrite(ninthLedPin, LOW);
  }
}