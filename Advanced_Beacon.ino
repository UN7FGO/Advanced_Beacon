#include <TM1638.h>
#include <OneWire.h>
#include <stdio.h>
#include <DS1302.h>

// контакт вывода на "ключевание"
#define CW_PIN 13
// контакт вывода звука 
#define CW_AUDIO_KEYER_PIN 10
// частота звука для контроля
#define CW_AUDIO_FREQUENCY 880
// базова длина "точки", определяет скорость передачи сообщения
#define CW_TICK_TIME_MILLISECONDS 70
// контакт PTT
#define CW_PTT_PIN 9
// контакт для подключения датчика температуры DS18B20
#define TMP_PIN A1
// контакт для подключения реле управления вентилятором обдува радиостанции
#define FAN_PIN 5
// контакт для подключения реле управления питанием радиостанции
#define TRX_PIN 6
// температура включения вентилятора обдува
#define TMP_FAN_ON 30
// предельная температура передатчика, при которой прекращается передача
#define TMP_MAX 70
// температура передатчика, при которой разрешается работа на передачу, после отключения
#define TMP_MIN 50
// время повтора сообщения, секунды
unsigned long TIME_RPT = 180;
// время работы в начале каждого часа, для 2 режима работы
#define TIME_BEG 10


OneWire  ds(TMP_PIN);  

// контакты используемые для модуля LED & KEY
#define LK_STB 2
#define LK_CLK 3
#define LK_DIO 4

#define RTC_SCL 7
#define RTC_IO  8
#define RTC_RST 11

#define RELAY_ON  0
#define RELAY_OFF 1



// Create a DS1302 object.
DS1302 rtc(RTC_RST, RTC_IO, RTC_SCL);

// define a modules
TM1638 sk_module = { TM1638(LK_DIO, LK_CLK, LK_STB, true , 0) };


  int t_tick = CW_TICK_TIME_MILLISECONDS;

  // Length of dot, dash, between characters, between words
  // Динна точки, тире, между символами, между словами
  int t_dot = t_tick * 1;
  int t_dash = t_tick * 3;
  int t_between_char = t_tick * 3;
  int t_between_words = t_tick * 7;
  
  String st_beacon, time_st, t_st;
  unsigned long last_time, wait_time;
  boolean fan_on, trx_off, beacon_on;
  int current_temp;
// переменная для текущего режима работы  
  int current_mode;
// возможные режимы работы
// 0 - работа на передачу отключена
// 1 - работа с указанной в настройках паузой между циклами передачи (переменная TIME_RPT)
// 2 - работа в течении TIME_BEG минут в начале каждого часа
// 3 - непрерывная работа, до отмены данного режима


  
void setup() {
// определяем режим работы некоторых контактов
  pinMode(CW_PIN, OUTPUT);  
  pinMode(CW_AUDIO_KEYER_PIN, OUTPUT);  
  pinMode(CW_PTT_PIN, OUTPUT);  
  pinMode(TRX_PIN, OUTPUT);  
  digitalWrite(TRX_PIN, LOW);
  pinMode(FAN_PIN, OUTPUT);  
  digitalWrite(FAN_PIN, LOW);

// раздел начальных "динамических" установок
  last_time = 0;
  current_temp = 22;
// переменные по умолчанию переводим в "безопасное" состояние  
  fan_on = true;
  trx_off = true;
  beacon_on = false;
// режим работы "по умолчанию"
  current_mode = 0;  
// строка, которую будет выдавать маяк
  st_beacon = "CQ CQ CQ DE BEACON UN8FF LOC MO82JM86OG BAND 4M K";
//  st_beacon = "CQ DE BEACON";
  
}

void loop() {

// вся обработка ограничений по температуре вначале
// так как возможно выключение контроллера и включение при горячем передатчике

// опрашиваем таймер
  Time t = rtc.time();
  
// считываем показания датчика температуры
  current_temp = getTempDS18b20();

// опрашиваем кнопки
  byte buttons = sk_module.getButtons();
// нажатием кнопок переключаем режим работы маяка
  if (buttons != 0) {
    switch (buttons) {
      case 1 : 
// выбор режима 0 - работа на передачу отключена
        current_mode = 0;
        break;
      case 2 : 
// выбор режима 1 - работа с указанной в настройках паузой между циклами передачи (переменная TIME_RPT)
        current_mode = 1;
        break;
      case 4 : 
// выбор режима 2 - работа в течении TIME_BEG минут в начале каждого часа
        current_mode = 2;
        break;
      case 8 : 
// выбор режима 3 - непрерывная работа, до отмены данного режима
        current_mode = 3;
        break;
      case 64 : 
// переход в режим настройки времени
        SetTime();
        break;
      case 128 : 
// показываем текущую температуру
        time_st = " t-" + String(current_temp) + " C ";
        sk_module.setDisplayToString(time_st);
        delay(4000);        
        break;
    }
  }
   
// зажигаем соответствующий режиму работы светодиод
  for (int i=0; i <= 4; i++) {
     if ( i == current_mode ) {
        sk_module.setLED(1, i);
     } else {
        sk_module.setLED(0, i);
     }
     
  }

// проверяем условие для включения вентилятора обдува
// и обозначаем переменную, в дальнейшем для индикации и прочего
// управление выводом включения вентилятора обдува
  if (current_temp > TMP_FAN_ON) {
    fan_on = true;
    digitalWrite(FAN_PIN, RELAY_ON);
    sk_module.setLED(1, 5);
  } else {
    fan_on = false;
    digitalWrite(FAN_PIN, RELAY_OFF);
    sk_module.setLED(0, 5);
  }

// проверяем условие для отключения передатчика
// если перегрелся, то отключаем и запрещаем работу на передачу
  if (current_temp > TMP_MAX) {
    trx_off = true;
    digitalWrite(TRX_PIN, RELAY_ON);
  }
// если температура передатчика ниже разрешенной после перегрева, то разрешаем включать передатчик
  if (current_temp < TMP_MIN) {
    trx_off = false;
    digitalWrite(TRX_PIN, RELAY_OFF);
  }
  
// выключаем разрешение работы маяка, будем это проверять позже
  beacon_on = false;

// в зависимости от режима, пробуем включить маяк
    switch (current_mode) {
      case 0 : 
// 0 - работа на передачу отключена
        beacon_on = false;
        break;
      case 1 : 
// 1 - работа с указанной в настройках паузой между циклами передачи (переменная TIME_RPT)
        if ((millis() - last_time) > (wait_time*1000) || last_time == 0 )
        {
          beacon_on = true;
        }          
        break;
      case 2 : 
// 2 - работа в течении TIME_BEG минут в начале каждого часа
        if ( t.min <= TIME_BEG ) {
          beacon_on = true;
        }
        break;
      case 3 : 
// 3 - непрерывная работа, до отмены данного режима
        beacon_on = true;
        break;
    }
  
  wait_time = TIME_RPT;
// если прошло достаточно времени, то начинаем передавать сообщение

// если передатчик еще не остыл, после перегрева, то не разрешаем передачу
  if (trx_off) 
  {
    beacon_on = false;
// показываем перегрев и текущую температуру
    time_st = "hot " + String(current_temp) + " C";
    sk_module.setDisplayToString(time_st);
    delay(500);        
  }  

// если мы в режиме ожидания, то выводим на индикаторы текущее время
    if (!beacon_on && !trx_off) {
// формируем строку текущего времени
      t = rtc.time();
      time_st = "";
      t_st = String(t.hr);
      if (t.hr < 10 ){
        t_st = "0"+t_st;
      }
      time_st = time_st + t_st + "-";
      t_st = String(t.min);
      if (t.min < 10 ){
        t_st = "0"+t_st;
      }
      time_st = time_st + t_st + "-";
      t_st = String(t.sec);
      if (t.sec < 10 ){
        t_st = "0"+t_st;
      }
      time_st = time_st + t_st;
// выводим на индикатор
      sk_module.setDisplayToString(time_st);
  }


  
  if (beacon_on) 
  {
// Запоминаем время последней работы маяка на передачу
      last_time = millis();
 
// выводим сообщение о том, что мы сейчас работаем на передачу 
      sk_module.setDisplayToString("BEACON  ");
// "зажигаем" светодиод работы на передачу
      sk_module.setLED(1, 7);
// включаем режим передачи
// для радиостанций с необходимостью нажатия PTT
      digitalWrite(CW_PTT_PIN, HIGH);

// выдаем строку текста посимвольно кодом Морзе
      int st_len = st_beacon.length();
      for (int i=0; i<st_len; i++) {
        keyCwForCharacter(st_beacon[i]);
      }
  
// выключаем режим передачи
// для радиостанций с необходимостью нажатия PTT
      digitalWrite(CW_PTT_PIN, LOW);

// "гасим" светодиод работы на передачу
    sk_module.setLED(0, 7);
      
  }
  
  delay (200);
}

// Функция преобразования символа в строку кода Морзе
// Написано примитивненько, но работает
// можно дополнить любыми символами
char *morseForSymbol(char symbol) {
  char *morse = NULL;

  if (symbol == '.') {
    morse = ".-.-.-";
  } else if (symbol == 'a' || symbol == 'A') {
    morse = ".-";
  } else if (symbol == 'b' || symbol == 'B') {
    morse = "-...";
  } else if (symbol == 'c' || symbol == 'C') {
    morse = "-.-.";
  } else if (symbol == 'd' || symbol == 'D') {
    morse = "-..";
  } else if (symbol == 'e' || symbol == 'E') {
    morse = ".";
  } else if (symbol == 'f' || symbol == 'F') {
    morse = "..-.";
  } else if (symbol == 'g' || symbol == 'G') {
    morse = "--.";
  } else if (symbol == 'h' || symbol == 'H') {
    morse = "....";
  } else if (symbol == 'i' || symbol == 'I') {
    morse = "..";
  } else if (symbol == 'j' || symbol == 'J') {
    morse = ".---";
  } else if (symbol == 'k' || symbol == 'K') {
    morse = "-.-";
  } else if (symbol == 'l' || symbol == 'L') {
    morse = ".-..";
  } else if (symbol == 'm' || symbol == 'M') {
    morse = "--";
  } else if (symbol == 'n' || symbol == 'N') {
    morse = "-.";
  } else if (symbol == 'o' || symbol == 'O') {
    morse = "---";
  } else if (symbol == 'p' || symbol == 'P') {
    morse = ".--.";
  } else if (symbol == 'q' || symbol == 'Q') {
    morse = "--.-";
  } else if (symbol == 'r' || symbol == 'R') {
    morse = ".-.";
  } else if (symbol == 's' || symbol == 'S') {
    morse = "...";
  } else if (symbol == 't' || symbol == 'T') {
    morse = "-";
  } else if (symbol == 'u' || symbol == 'U') {
    morse = "..-";
  } else if (symbol == 'v' || symbol == 'V') {
    morse = "...-";
  } else if (symbol == 'w' || symbol == 'W') {
    morse = ".--";
  } else if (symbol == 'x' || symbol == 'X') {
    morse = "-..-";
  } else if (symbol == 'y' || symbol == 'Y') {
    morse = "-.--";
  } else if (symbol == 'z' || symbol == 'Z') {
    morse = "--..";
  } else if (symbol == '0') {
    morse = "-----";
  } else if (symbol == '1') {
    morse = ".----";
  } else if (symbol == '2') {
    morse = "..---";
  } else if (symbol == '3') {
    morse = "...--";
  } else if (symbol == '4') {
    morse = "....-";
  } else if (symbol == '5') {
    morse = ".....";
  } else if (symbol == '6') {
    morse = "-....";
  } else if (symbol == '7') {
    morse = "--...";
  } else if (symbol == '8') {
    morse = "---..";
  } else if (symbol == '9') {
    morse = "----.";
  } else if (symbol == '?') {
    morse = "..--..";
  } else if (symbol == '=') {
    morse = "-...-";
  } else if (symbol == '/') {
    morse = "-..-.";
  } else if (symbol == '+') {
    morse = ".-.-.";
  } else if (symbol == ',') {
    morse = "--..--";
  } else {
    // For anything else, return a question mark
    morse = "..--.."; 
  }
  return morse;
}

// функция вывода символа азбукой Морзе, в качестве параметра передается символ
void keyCwForCharacter(char symbol) {

  if (symbol == ' ') {
    delay(t_between_words);
// проверяем, не решили ли сменить режим работы маяка
// опрашиваем кнопки
    byte buttons = sk_module.getButtons();
// нажатием кнопок переключаем режим работы маяка
    if (buttons != 0) {
      switch (buttons) {
        case 1 : 
// выбор режима 0 - работа на передачу отключена
          current_mode = 0;
          break;
        case 2 : 
// выбор режима 1 - работа с указанной в настройках паузой между циклами передачи (переменная TIME_RPT)
          current_mode = 1;
          break;
        case 4 : 
// выбор режима 2 - работа в течении TIME_BEG минут в начале каждого часа
          current_mode = 2;
          break;
        case 8 : 
// выбор режима 3 - непрерывная работа, до отмены данного режима
          current_mode = 3;
          break;
    }
  }
// зажигаем соответствующий режиму работы светодиод
    for (int i=0; i <= 3; i++) {
       if ( i == current_mode ) {
          sk_module.setLED(1, i);
       } else {
          sk_module.setLED(0, i);
       }
     
    }
// окончание паузы между словами (пробела)  

  } else {
    char *symbol_in_morse = morseForSymbol(symbol);
    int j_len = strlen(symbol_in_morse);
    int j = 0;

    for (j=0; j<j_len; j++) {
      char dotdash = *(symbol_in_morse + j);
      int t_delay = 0;
      if (dotdash == '.') {
        t_delay = t_dot;
      } else if (dotdash == '-') {
        t_delay = t_dash;
      } else {
        t_delay = t_tick;
      }
  
// нажимаем "ключ"      
      digitalWrite(CW_PIN, HIGH);
// включаем звуковой тон
      tone(CW_AUDIO_KEYER_PIN, CW_AUDIO_FREQUENCY);
// задержка длительности звука/нажатия ключа      
      delay(t_delay);
// выключаем звуковой тон      
      noTone(CW_AUDIO_KEYER_PIN);
// "отпускаем" ключ
      digitalWrite(CW_PIN, LOW);
// выдерживаем паузу между точками/тире      
      delay(t_tick);
    }
// выдерживаем паузу между символами
    delay(t_between_char);
  }
}

// функция установки текущего времени в DS1302
void SetTime () {

boolean ddone;
String t_st;
int thr, tmin;
unsigned long lt;

  lt = millis();
// опрашиваем таймер
  Time t = rtc.time();
  thr = t.hr;
  tmin = t.min;
  delay (1000);
  ddone = false;

  while (!ddone) {

// опрашиваем кнопки
  byte buttons = sk_module.getButtons();
// нажатием кнопок переключаем режим работы маяка
  if (buttons != 0) {
    lt = millis();
    switch (buttons) {
      case 16 : 
// +1 к часам
        thr = thr + 1;
        if ( thr > 23 ) {
          thr = 0;        
        }
        break;
      case 32 : 
// +1 к минутам
        tmin = tmin + 1;
        if ( tmin > 59 ) {
          tmin = 0;        
        }
        break;
      case 64 : 
// записываем время в RTC и выходим
        ddone = true;
        Time tt(2017, 06, 30, thr, tmin, 00, Time::kFriday);
        rtc.time(tt);
        break;
    }
  }
  
      time_st = "";
      t_st = String(thr);
      if (thr < 10 ){
        t_st = "0"+t_st;
      }
      time_st = time_st + t_st + "-";
      t_st = String(tmin);
      if (tmin < 10 ){
        t_st = "0"+t_st;
      }
      time_st = time_st + t_st + "   ";
// выводим на индикатор
      sk_module.setDisplayToString(time_st);

      delay (250);

      if ( (millis()-lt) > 20000) {
        ddone = true;
      }
  }  
}

int getTempDS18b20 () {
  byte i;
  byte present = 0;
  byte type_s;
  int current_temp;
  byte data[12];
  byte addr[8];
  float celsius;

// ищем датчик температуры и получаем из него данные
// поцедура взята из библиотечного примера 
  if ( !ds.search(addr)) {
    ds.reset_search();
    delay(150);
    return -1 ;
  }

  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      type_s = 1;
      break;
    case 0x28:
      type_s = 0;
      break;
    case 0x22:
      type_s = 0;
      break;
    default:
      return -1 ;
  } 
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  delay(600);               // maybe 750ms is enough, maybe not
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad
  for ( i = 0; i < 9; i++) { data[i] = ds.read(); }
  // Convert the data to actual temperature
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
  }
  celsius = (float)raw / 16.0;
// конец поиска датчика температуры


// считываем показания датчика температуры
  current_temp = int(celsius);

  if (current_temp == -1) {
    current_temp = 20;
  }
  return current_temp;
}

