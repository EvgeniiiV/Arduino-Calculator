#include <Wire.h>
#include <LiquidCrystal_I2C.h>//SCL - A5, SDA - A4
#include <Keypad.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <EEPROM.h>
#include "buzzer.h"
#include "shift_register.h"
#include "show_battery.h"

#define MONITOR_WIDTH 16
#define MONITOR_ROWS 2
#define CLEAR_PIN 2//key input pin normally in LOW
#define LONG_TIME_PUSH 1400
#define TIME_TO_PRINT 200
#define CHARGE_GRADE 8 //количество пользовательских символов батареи 

//max. длина подмассивов чисел и знаков. 
#define MAX_HALF_STRING 100 //При значении в 300 не хватает SRAM, функция calculate result() перестает работать
#define MONITOR_POWER 13
#define EEPROM_ADDR 0x00

// Настройка дисплея I2C
LiquidCrystal_I2C lcd(0x27, MONITOR_WIDTH, MONITOR_ROWS);

// Настройка клавиатуры
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', '+'},
  {'4', '5', '6', '-'},
  {'7', '8', '9', 'x'},
  {'.', '0', '=', '/'}
};

byte rowPins[ROWS] = {5, 6, 7, 8};
byte colPins[COLS] = {9, 10, 11, 12};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//Время бездействия 
const unsigned long timeout = 1UL * 60UL * 1000UL; //1 минута

// Переменные для калькулятора
String input = "";
double result;

//флаги 
bool timer_print = false;//интервал вывода на экран
bool leading_zero = false;//set_string
bool dot = false;//десятичная точка установлена
bool math_sign = false; //последний введенный char - математическое действие.
bool long_push = false;//выбор аргумента r || R функции set_string()
 
//переменные
volatile unsigned long startPushTime = 0;//время нажатия С
volatile unsigned long startPrintTime = 0;//время вывода на экран
volatile unsigned long lastActivityTime = 0;//старт интервала бездействия перед засыпанием

//функции
void wait_to_read();//время на чтение приветствия
void calculate_result();//
void set_string(char new_char);
void print_string();
void printResult();
void saveStringToEEPROM(int addr, String data);
String readStringFromEEPROM(int addr);

void setup() {
  Serial.begin(9600);
  
  pinMode(latchPin, OUTPUT);//определены в shift_register.h
  pinMode(clockPin, OUTPUT);//определены в shift_register.h
  pinMode(dataPin, OUTPUT);//определены в shift_register.h
  pinMode(ninthLedPin, OUTPUT);//определены в shift_register.h
  digitalWrite(ninthLedPin, LOW);//определены в shift_register.h
  pinMode(MONITOR_POWER, OUTPUT);
  digitalWrite(MONITOR_POWER, HIGH);   
  pinMode(CLEAR_PIN, INPUT); 

  // Настройка Timer1 для генерации прерывания раз в 1 секунду
  cli(); // Отключение глобальных прерываний
  TCCR1A = 0; // Сброс регистра управления
  TCCR1B = 0;
  TCNT1 = 0; // Сброс счетчика
  OCR1A = 15624; // Установить значение для сравнения, 1 сек при частоте 16MHz и предделителе 1024
  TCCR1B |= (1 << WGM12); // Включить CTC режим
  TCCR1B |= (1 << CS12) | (1 << CS10); // Установить предделитель на 1024
  TIMSK1 |= (1 << OCIE1A); // Включить прерывание по совпадению
  sei(); // Включение глобальных прерываний
  
  //если EEPROM не пустой (т. е. контроллер уходил в сон), то загружаем input из него
  //первые 2 байта - длина имеющейся строки: младший, потом старший байты
  uint16_t len = EEPROM.read(EEPROM_ADDR) | (EEPROM.read(EEPROM_ADDR + 1) << 8);
  
  if(len){    
    for(uint16_t i = 2; i < len + 2 ; i++){
      input += (char)EEPROM.read(i);      
    }
    //обнуляем длину, на случай корректного выключения устройства
    EEPROM.write(EEPROM_ADDR, 0x00); // Младший байт длины
    EEPROM.write(EEPROM_ADDR + 1, 0x00); // Старший байт длины
  }
  updateShiftRegister(0);//стираем случайное значение в сдвиговом регистре
  // Инициализация LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Hi, daddy's");
  lcd.setCursor(0, 1);
  lcd.print("clever girl :-)");
  wait_to_read();  

  start_show_battery_time = millis();
  lcd.clear();
  if(len){
    lcd.print(input);
  }
  else
  lcd.print("0");
  lastActivityTime = millis();

  //стабилизируем питание монитора
  delay(100);  
  //передаем символы батареи🔋 
  createChars(lcd);  
  delay(50);  

}
///////////////////////////////////////////loop() START ///////////////////////////////////
void loop() {  
  //контроль времени замера напряжения питания
  if((millis() - start_show_battery_time) >= 3000){
    //контроль напряжения 
    long vcc = readVcc();
    int i = 0; 
    for(; i < CHARGE_GRADE - 1; i++){
      if(vcc < voltage[i])
      break;
    }  
    //вывод напряжения батареи     
    lcd.setCursor(15, 1);
    lcd.print(" ");
    lcd.setCursor(15, 1);
    lcd.write((uint8_t)(i));
    start_show_battery_time = millis(); 
  }   

  //устанавливаем таймер печати
  if(!timer_print){
  startPrintTime = millis();  
  timer_print = true;   
  }
 if(digitalRead(CLEAR_PIN)){
    startPushTime = millis(); 
    while(digitalRead(CLEAR_PIN)){
      if((millis() - startPushTime) >= LONG_TIME_PUSH) {
        long_push = true;
        break;
      }
    }
    if(long_push){
      long_push = false;
      set_string('R');  
      buzzer('R'); 
      updateShiftRegister(0);
    } 
    else {
      set_string('r');  
      buzzer('r'); 
      updateShiftRegister(0);
    } 
  }
  volatile char key = keypad.getKey();   
  if (key) {
    buzzer(key);   
    if(key >= '0' && key <= '9') {
      uint8_t led_number = key - '0';
      updateShiftRegister(led_number);
    }        
    lastActivityTime = millis();//обнуляем таймер отключения     
    if((millis() - startPrintTime) >= TIME_TO_PRINT) {
      set_string(key);
      print_string();       
      timer_print = false;
    }
  }
}
///////////////////////////////////////////loop() END ///////////////////////////////////

void wait_to_read(){  
  bool wait = true;
  while(wait){
    char key = keypad.getKey();           
    if(key || digitalRead(CLEAR_PIN)) wait = false;
  }
}

void calculate_result() {  
  // Получаем длину строки input
  int length = input.length();
  bool leading_negative = false;
  // строка для хранения строкового представления числа из input
  String temp_num;
  // установим флаг что первое число отрицательное
  if (input[0] == '-') {
    leading_negative = true;
  }
  // Начинаем с 1 если первое число отрицательное, иначе с 0
  int start = leading_negative ? 1 : 0;
  // создадим два массива: для чисел и математических действий
  double number[MAX_HALF_STRING];  
  char math_action[MAX_HALF_STRING];
  // индексы чисел и мат действий
  int num_index = 0, math_action_index = 0;

  // сортируем input в эти массивы
  for (int i = start; i < length && i < MAX_HALF_STRING; i++) {
    // Если текущий символ - цифра или точка, добавляем его в temp_num
    if ((input[i] >= '0' && input[i] <= '9') || input[i] == '.') {
      temp_num += input[i];
    }
    // Если текущий символ - математический знак, обрабатываем его
    else if (input[i] == '+' || input[i] == '-' || input[i] == 'x' || input[i] == '/') {
      // упаковываем число в number[]
      if (temp_num.length() > 0) {
        number[num_index++] = temp_num.toDouble();
        temp_num = "";
      }
      // упаковываем char в math_action[100]
      math_action[math_action_index++] = input[i];
    }
  }
  
  // упаковываем последнее число в number[]
  if (temp_num.length() > 0) {
    number[num_index++] = temp_num.toDouble();
  }

  // Создадим переменную для динамики сокращения элементов массива number[]
  // длина массива math_action будет на 1 меньше на протяжении всей программы
  // поэтому нет смысла вводить дополнительную переменную
  int number_length = num_index;
  // установим знак первого числа
  if (leading_negative) {
    number[0] *= -1;
  }

  // считаем результат для знаков х и /, итерируясь по массиву math_action
  for (int i = 0; i < (number_length - 1) && i < MAX_HALF_STRING; i++) {
    // итерируемся до индекса, содержащего х или / в math_action[]
    if (math_action[i] == 'x' || math_action[i] == '/') {
      // Создаем переменную для хранения результата математического действия, Выполняем действие
      double temp = (math_action[i] == 'x') ? number[i] * number[i + 1] : number[i] / number[i + 1];
      // Сохраняем число в массиве number[] по индексу i
      number[i] = temp;
      // сдвигаем оба массива на 1 элемент влево, i--, с учетом ее инкремента в цикле
      for (int y = i + 1; y < (number_length - 1); y++) {
        number[y] = number[y + 1];
        math_action[y - 1] = math_action[y];
      }
      // уменьшаем полезную длину массивов
      number_length--;
      i--;
    }
  }

  // положим первый элемент number[0] в result
  result = number[0];
  // Здесь уже имеем оба массива без х и /
  // Длина math_action[] = number_length - 1 
  for (int i = 0; i < (number_length - 1); i++) {
    // идем по math_action[]
    if (math_action[i] == '+') {
      result += number[i + 1];
    } else {
      result -= number[i + 1];
    }
  }  
}


void set_string(char new_char){
  if(new_char == '-' || new_char == '+' || new_char == 'x' || new_char == '/' || new_char == '.') {
    updateShiftRegister(0);
  }
  math_sign = false;   
  if(new_char){  
    char last = input[input.length() - 1];
    if(last ==  '-' || last == '+' || last == 'x' || last == '/'){
      math_sign = true;      
    }  
    if(!input.length() && (new_char == '+' || new_char == 'x' || new_char == '/')) return; 
    //замена знака любого на любой при наличие уже введенного знака 
    if(new_char == '-' || new_char == '+' || new_char == 'x' || new_char == '/'){
      dot = false;
      leading_zero = false; 
      if(math_sign){
        input[input.length() - 1] = new_char;
        print_string();
      }
      else{
        input += new_char;
        print_string();
      }
    }
    //выставляем флаг первого нуля или нуля после мат операции 
    else if((new_char == '0') && (!input.length() || math_sign || (input.length() == 1 && input[0] == '-'))){      
      leading_zero = true; 
      input += new_char;            
    }
    //конкатинация цифры 0 - 9
    else if(new_char >= '0' && new_char <= '9'){
      //нет ноля как первой цифры числа
      if(!leading_zero){
        math_sign = false;
        leading_zero = false; 
        input += new_char;
        print_string();     
      }
      //в числе 0 уже установлен как первая цифра
      else{
        input[input.length() - 1] = new_char;
        math_sign = false;
        leading_zero = false;
        print_string();  
      } 
            
    }
    //new_char = r - удаление последнего знака или числа, new_char = R - удаление всей строки
    else if((new_char == 'r' || new_char == 'R') && input.length()){
      if(new_char == 'r'){
        input.remove(input.length() - 1);
        print_string();
      }
      else {        
        input = "";
        print_string();        
      }
    }
    else if(new_char == '='){
      //удалим знак математического действия если он оказался перед знаком =
      if(last == '-' || last == '+' || last == 'x' || last == '/'){        
        input.remove(input.lastIndexOf(last));
        print_string();
      }      
      dot = false;
      calculate_result();
      updateShiftRegister(0);      
      while(!digitalRead(CLEAR_PIN)) {
        printResult(); delay(30);        
      } 
      buzzer('R');      
      input = "";     
    }
    else if (new_char == '.' && !dot){
      dot = true;
      leading_zero = false;
      if(!input.length() || math_sign){
        input += '0';
        input += new_char;
        print_string();
      }
      else{
        input += new_char;
        print_string();
      }
    }
    
  }  
}

void print_string(){   
  lcd.clear();
  lcd.setCursor(0, 0);
  short len = input.length();
  if (len){ 
    if(len <= 16) lcd.print(input);
    else{      
      short row_start = len - MONITOR_WIDTH;    
      lcd.print(input.substring(row_start));    
    }   
  }
  else{    
    lcd.print("0");
  }
  delay(10);  
}


void printResult() { 
  // Проверяем, если result равен 0, сразу выводим "0"
  if (result == 0) {
    lcd.setCursor(0, 1);
    lcd.print("=0");
    Serial.println("0");
    return;
  }

  // Преобразуем result в строку с двумя знаками после запятой
  String str_result = String(result, 2);

  // Удаляем все конечные нули
  while (str_result.endsWith("0")) {
    str_result.remove(str_result.length() - 1);
  }

  // Если строка заканчивается на ".", удаляем её
  if (str_result.endsWith(".")) {
    str_result.remove(str_result.length() - 1);
  }

  // Выводим результат на экран и в Serial
  lcd.setCursor(0, 1);
  lcd.print("=");
  lcd.print(str_result);   
}

ISR(TIMER1_COMPA_vect) {    
  // Проверяем, прошло ли время timeout с последнего действия
  if (millis() - lastActivityTime >= timeout) {
    saveStringToEEPROM();
    updateShiftRegister(0);    
    // Если прошло, переводим контроллер в режим сна    
    digitalWrite(MONITOR_POWER, LOW);
    // Настройка контроллера для сна
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_mode();
    // Пробуждение после reset
    sleep_disable();
    cli(); // Отключение глобальных прерываний
    sei(); // Включение глобальных прерываний
  }
}

//  ->>EEPROM
void saveStringToEEPROM() {
  uint16_t len = input.length(); // Длина строки, которая будет записана

  // Сохраняем длину строки (2 байта)
  EEPROM.write(EEPROM_ADDR, len & 0xFF); // Младший байт длины
  EEPROM.write(EEPROM_ADDR + 1, (len >> 8) & 0xFF); // Старший байт длины

  // Сохраняем саму строку
  for (uint16_t i = 0; i < len; i++) {
    EEPROM.write(EEPROM_ADDR + 2 + i, input[i]); // Записываем строку после длины
  }
}

//  EEPROM->>
// String readStringFromEEPROM() {
//   uint16_t len = EEPROM.read(EEPROM_ADDR) | (EEPROM.read(EEPROM_ADDR + 1) << 8); // Читаем длину строки (2 байта)
  
//   char input[len + 1]; // Создаем массив для хранения строки
//   for (uint16_t i = 0; i < len; i++) {
//     input[i] = EEPROM.read(EEPROM_ADDR + 2 + i); // Читаем символы строки
//   }
//   input[len] = '\0'; // Завершающий нуль

//   return String(input); // Возвращаем строку
// }


