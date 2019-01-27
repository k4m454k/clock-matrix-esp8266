/*
  Подключения:
  NodeMCU    -> Matrix
  MOSI-D7-GPIO13  -> DIN
  CLK-D5-GPIO14   -> Clk
  GPIO0-D3        -> LOAD
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h> // Библиотека для OTA-прошивки
#include <SerialCommand.h>
#include <Ticker.h>

Ticker blinker;
SerialCommand SCmd;


#define SUN { \
     {1, 0, 0, 1, 0, 0, 0, 1}, \
     {0, 1, 0, 0, 0, 0, 1, 0}, \
     {0, 0, 0, 1, 1, 0, 0, 0}, \
     {0, 0, 1, 1, 1, 1, 0, 1}, \
     {1, 0, 1, 1, 1, 1, 0, 0}, \
     {0, 0, 0, 1, 1, 0, 0, 0}, \
     {0, 1, 0, 0, 0, 0, 1, 0}, \
     {1, 0, 0, 0, 1, 0, 0, 1}  \
 }
 
#define RAIN { \
     {0, 1, 1, 1, 0, 0, 0, 0}, \
     {1, 0, 0, 0, 1, 1, 1, 0}, \
     {1, 0, 0, 0, 1, 0, 0, 1}, \
     {1, 0, 0, 0, 0, 0, 0, 1}, \
     {0, 1, 1, 1, 1, 1, 1, 0}, \
     {0, 0, 0, 0, 0, 0, 0, 0}, \
     {0, 0, 1, 0, 1, 0, 0, 0}, \
     {0, 1, 0, 1, 0, 0, 0, 0}  \
 }
 
#define CLOUDS { \
     {0, 1, 1, 1, 0, 0, 0, 0}, \
     {1, 0, 0, 0, 1, 1, 1, 0}, \
     {1, 0, 0, 0, 1, 0, 0, 1}, \
     {1, 0, 0, 0, 0, 0, 0, 1}, \
     {0, 1, 1, 1, 1, 1, 1, 0}, \
     {0, 0, 0, 0, 0, 0, 0, 0}, \
     {0, 0, 0, 0, 0, 0, 0, 0}, \
     {0, 0, 0, 0, 0, 0, 0, 0}  \
 }

bool SunIcon[8][8] = SUN;
bool RainIcon[8][8] = RAIN;
bool CloudsIcon[8][8] = CLOUDS;


// =======================================================================
// Конфигурация устройства:
// =======================================================================
const char* ssid     = "";                      // SSID
const char* password = "";                    // пароль
String weatherKey = "9fc8d50c1f5e7b3f010e0eee7375d903";  // Получить API ключ по ссылке http://openweathermap.org/api
String weatherLang = "&lang=ru";
String cityID = "524901"; //Москва
// =======================================================================


WiFiClient client;

String weatherMain = "";
String weatherDescription = "";
String weatherLocation = "";
String country;
int humidity;
int pressure;
int windDegress;
float temp;
float tempMin, tempMax;
int clouds;
float windSpeed;
String date;
String date1;
String currencyRates;
String weatherString;
String weatherString1;
String weatherIcon;

String week;
String days;
String mont;
String years;

String timestr;
String raz; // разделитель часов (двоеточие или пробел, моргает)

int flag = 0; // Флаг отображения, 1-погода или 0-дата


long period;
int offset = 1, refresh = 0;
int pinCS = 0; // Подключение пина CS
int numberOfHorizontalDisplays = 4; // Количество светодиодных матриц по Горизонтали
int numberOfVerticalDisplays = 2; // Количество светодиодных матриц по Вертикали
String decodedMsg;
Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);
//matrix.cp437(true);

int wait = 30; // скорость бегущей строки (задержка между смещениями в мс)

int spacer = 2; //разделитель между символами текста
int width = 5 + spacer; // Регулируем расстояние между символами

void setup(void) {
  matrix.setIntensity(13); // Яркость матрицы от 0 до 15

  // начальные координаты матриц 8*8
  matrix.setRotation(0, 1);        // 1 матрица
  matrix.setRotation(1, 1);        // 2 матрица
  matrix.setRotation(2, 1);        // 3 матрица
  matrix.setRotation(3, 1);        // 4 матрица
  matrix.setRotation(4, 1);        // 1 матрица
  matrix.setRotation(5, 1);        // 2 матрица
  matrix.setRotation(6, 1);        // 3 матрица
  matrix.setRotation(7, 1);        // 4 матрица
  Serial.begin(115200);// Дебаг

  Serial.println(LOW);
  Serial.println(HIGH);

  
  // для обработчика сериал порта
  SCmd.addCommand("date", showdate);
  SCmd.addCommand("weather", showweather);
  SCmd.addDefaultHandler(unrecognized);
  // ============================
  Serial.println("");
  Serial.println("VCC " + ESP.getVcc());
  ConnectToWiFi(false);

  // этот кусок кода для прошивки по вайфай ========================
  ArduinoOTA.setHostname("CLOCK"); // Задаем имя сетевого порта
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch     ";
    else // U_SPIFFS
      type = "filesystem     ";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
    ScrollText("       Start updating " + type,1);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    ScrollText("     Updated!      ",1);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    progressBar((progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  //blinker.attach_ms(1000,DisplayTime);
  // ==============================================================
}



//обработчик комманд с сериал порта
void unrecognized() {
  Serial.println("What?");
}

void showdate() {
  Serial.println("date");
  DisplayDate();
}

void showweather() {
  Serial.println("weather");
  DisplayWeather();
}
//================================

void CheckWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi broken. Reconnect.");
    ConnectToWiFi(true);
  }
}





void progressBar(int Value) {
  matrix.fillScreen(LOW);
  int y = (matrix.height() - 8) / 2;
  int x = map(Value, 0, 100, 0, 32);
  matrix.drawPixel(x, y, HIGH);
  matrix.write();
}

String getDescriptionWiFi() {
  String Description;
  switch (WiFi.status()) {
    case WL_IDLE_STATUS:
      Description = "       Смена статуса.";
      return Description;
      break;
    case WL_NO_SSID_AVAIL:
      Description = "       Не могу найти Wi-Fi " + String(ssid);
      return Description;
      break;
    case WL_CONNECTED:
      Description = "       Соединено.";
      return Description;
      break;
    case WL_CONNECT_FAILED:
      Description = "       Не могу соединиться. Пароль не верный.";
      return Description;
      break;
    case WL_DISCONNECTED:
      Description = "       Нет связи.";
      return Description;
      break;
    default:
      Description = "       С Wi-Fi какая-то хуйня.";
      return Description;
  }

}

void ConnectToWiFi(bool reconn) {
  if (reconn) {
    WiFi.mode(WIFI_STA);
    int retries = 0;
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      retries++;
      if (retries == 200) {
        Serial.println("Connection error");
        ScrollText(utf8rus(getDescriptionWiFi()),1);
        retries = 0;
        return;
      }
      //Serial.println("Wifi stat " + WiFi.status());
    }
  } else {
    ScrollText("       Connecting to " + String(ssid)  + " ",1);
    WiFi.mode(WIFI_STA);
    matrix.fillScreen(LOW);
    WiFi.begin(ssid, password);// Подключаемся к WIFI
    int enn = -2;
    int retries = 0;
    int y = (matrix.height() - 8) / 2; // Центрируем текст по Вертикали
    while (WiFi.status() != WL_CONNECTED) {         // Ждем до посинения
      delay(100);
      //Serial.println("Wifi stat " + WiFi.status());
      matrix.drawChar(enn, y, (String(">"))[0], HIGH, LOW, 1);
      matrix.write();
      enn++;
      retries++;
      if (enn > 30) {
        enn = -2;
        matrix.fillScreen(LOW);
      }

      if (retries == 200) {
        Serial.println("Connection error");
        ScrollText(utf8rus(getDescriptionWiFi()),1);
        retries = 0;
      }
    }    
    matrix.fillScreen(LOW);

    String ipstring = (String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]));

    ScrollText(utf8rus("       Соединено. IP: " + ipstring + "  "),1);
    
    delay(1000);
  }
}


// =======================================================================
#define MAX_DIGITS 16
byte dig[MAX_DIGITS] = {0};
byte digold[MAX_DIGITS] = {0};
byte digtrans[MAX_DIGITS] = {0};
int updCnt = 0;
int dots = 0;
long dotTime = 0;
long clkTime = 0;
long textTime = 0;
int dx = 0;
int dy = 0;
byte del = 0;
int h, m, s;
// =======================================================================


void loop(void) {
  SCmd.readSerial();
  CheckWiFiConnection();
  ArduinoOTA.handle(); // Всегда готовы к прошивке
  //interrupts();
  if (updCnt <= 0) { // каждые 10 циклов получаем данные времени и погоды
    updCnt = 50;
    Serial.println("Getting data ...");
    getTime();
    getWeatherData();
    Serial.println("Data loaded");
    clkTime = millis();
  }

  if (millis() - clkTime > 10000 && !del && flag == 0) { 
    DisplayDate();
    updCnt--;
    clkTime = millis();
  }

  if (millis() - clkTime > 2000 && !del && flag == 1) { 
    DisplayWeather();
    updCnt--;
    clkTime = millis();
  }

    if (millis() - textTime > 2000 && flag == 2) { 
    ScrollText(utf8rus("        Помни про график. Экономь бабло!        "),2);
    updCnt--;
    textTime = millis();
    flag = 3;
  }

  if (millis() - textTime > 2000 && flag == 3) { 
    DisplayText(String(temp, 0) + "c",2);
    drawWeatherIconFromStr(weatherIcon);
    updCnt--;
    textTime = millis();
    flag = 0;
  }

  
  DisplayTime();
//  if (millis() - dotTime > 500) {
//    dotTime = millis();
//    dots = !dots;
//  }


}


void drawIcon(int xStart,int yStart,bool Icon[8][8]){
  for (int y=yStart;y<yStart+8;y++){
    for(int x=xStart;x<xStart+8;x++){
      matrix.drawPixel(x,y,Icon[y-yStart][x-xStart]);  
    }
  }
  matrix.write();
}

void drawWeatherIconFromStr(String iconStr){
  Serial.println("icon: " + iconStr);
  if (iconStr == "01d"){
     
    drawIcon(matrix.width()-8,8,SunIcon);
    }
  if (iconStr == "02d"){ 
    drawIcon(matrix.width()-8,8,SunIcon);
    }
  if (iconStr == "09d"){ 
    drawIcon(matrix.width()-8,8,RainIcon);
    }
  if (iconStr == "10d"){ 
    drawIcon(matrix.width()-8,8,RainIcon);
    }
  if (iconStr == "11d"){ 
    drawIcon(matrix.width()-8,8,RainIcon);
    } //Groza
  if (iconStr == "04d"){ 
    Serial.println("CLOUDS");
    drawIcon(matrix.width()-8,8,CloudsIcon);
    }
  if (iconStr == "03d"){ 
    drawIcon(matrix.width()-8,8,CloudsIcon);
    }
}

// =======================================================================
void DisplayDate() {
  date1 = ("      " + date + "      ");
  ScrollText(utf8rus(date1),2);
  flag = 1;
}

// =======================================================================
void DisplayWeather() {
  weatherString1 = ("      " + weatherString + "      ");
  ScrollText(utf8rus(weatherString1),2);
  flag = 2;
}

// =======================================================================
void DisplayTime() {
  updateTime();

  //if (s & 1)raz = ":"; //каждую четную секунду печатаем двоеточие по центру (чтобы мигало)
  //else raz = " ";
  raz = ":";
  
  String hour1 = String (h / 10);
  String hour2 = String (h % 10);
  String min1 = String (m / 10);
  String min2 = String (m % 10);
  String sec1 = String (s / 10);
  String sec2 = String (s % 10);
  int xh = 2;
  int xm = 19;
  //    int xs = 28;

  timestr = (hour1 + hour2 + raz + min1 + min2);
  DisplayText(timestr,1);
}

// =======================================================================
void DisplayText(String text,byte str) {
  int heiter = 0;
  if (str == 1){
    heiter = 16;
  }else{
    heiter = 0;  
  }
  clearString(str);
  for (int i = 0; i < text.length(); i++) {
    int letter = (matrix.width()) - i * (width - 1);
    int x = (matrix.width() + 1) - letter;
    int y = (matrix.height() - heiter) / 2; // Центрируем текст по Вертикали
    matrix.drawChar(x, y, text[i], HIGH, LOW, 1);
  }
  matrix.write(); // Вывод на дисплей
  delay(10);
}
// =======================================================================

void clearString(byte str){
  int heiter = 0;
  int start;
  if (str == 1){
    heiter = 8;
    start=0;
  }else{
    heiter = 16;
    start = 8;  
  }
  
  for (int x=0;x<matrix.width();x++){
    for (int y=start;y<heiter;y++){
     matrix.drawPixel(x, y, LOW); 
     }
  }
  matrix.write();
}

void ScrollText (String text,byte str) {
  int heiter = 0;
  if (str == 1){
    heiter = 16;
  }else{
    heiter = 0;  
  }
  for ( int i = 32 ; i < (width * text.length() + matrix.width() - 1 - spacer) - 32; i++ ) {
    if (refresh == 1) i = 0;
    refresh = 0;
    //  matrix.fillScreen(LOW);
    int letter = i / width;
    // int x =  1 - i % width;
    int x = (matrix.width() - 1) - i % width;
    int y = (matrix.height() - heiter) / 2; // Центрируем текст по Вертикали

    while ( x + width - spacer >= 0 && letter >= 0 ) {
      if ( letter < text.length() ) {
        matrix.drawChar(x, y, text[letter], HIGH, LOW, 1);
      }
      letter--;
      x -= width;
    }
    matrix.write(); // Вывод на дисплей
    ArduinoOTA.handle();
    delay(wait);
  }
  clearString(str);
}


// =======================================================================
// Берем погоду с сайта openweathermap.org
// =======================================================================



const char *weatherHost = "api.openweathermap.org";

void getWeatherData()
{
  Serial.print("connecting to "); Serial.println(weatherHost);
  if (client.connect(weatherHost, 80)) {
    client.println(String("GET /data/2.5/weather?id=") + cityID + "&units=metric&appid=" + weatherKey + weatherLang + "\r\n" +
                   "Host: " + weatherHost + "\r\nUser-Agent: ArduinoWiFi/1.1\r\n" +
                   "Connection: close\r\n\r\n");
  } else {
    Serial.println("connection failed");
    return;
  }
  String line;
  int repeatCounter = 0;
  while (!client.available() && repeatCounter < 10) {
    delay(500);
    Serial.println("w.");
    repeatCounter++;
  }
  while (client.connected() && client.available()) {
    char c = client.read();
    if (c == '[' || c == ']') c = ' ';
    line += c;
  }

  client.stop();

  DynamicJsonBuffer jsonBuf;
  JsonObject &root = jsonBuf.parseObject(line);
  if (!root.success())
  {
    Serial.println("parseObject() failed");
    return;
  }
  //weatherMain = root["weather"]["main"].as<String>();
  weatherDescription = root["weather"]["description"].as<String>();
  weatherIcon = root["weather"]["icon"].as<String>();
  weatherDescription.toLowerCase();
  //  weatherLocation = root["name"].as<String>();
  //  country = root["sys"]["country"].as<String>();
  temp = root["main"]["temp"];
  humidity = root["main"]["humidity"];
  pressure = root["main"]["pressure"];
  tempMin = root["main"]["temp_min"];
  tempMax = root["main"]["temp_max"];
  windSpeed = root["wind"]["speed"];
  windDegress = root["wind"]["deg"];
  clouds = root["clouds"]["all"];
  String deg = String(char('~' + 25));
  weatherString = "  Температура: " + String(temp, 1) + "'C    ";
  weatherString += "На улице " + weatherDescription;
  weatherString += "    Ветер: " + String(windSpeed, 1) + " м/с " + windDegDescription(windDegress) + "  ";
  weatherString += "    Влажность: " + String(humidity) + "%     ";
  weatherString += "Давление: " + String(pressure / 1.3332239) + " мм ";
  //  weatherString += "Облачность: " + String(clouds) + "% ";
}

String windDegDescription(int windDeg) {
  String windDegString;
  if (windDeg >= 345 || windDeg <= 22) windDegString = "Северный";
  if (windDeg >= 23 && windDeg <= 68) windDegString = "Северо-восточный";
  if (windDeg >= 69 && windDeg <= 114) windDegString = "Восточный";
  if (windDeg >= 115 && windDeg <= 160) windDegString = "Юго-восточный";
  if (windDeg >= 161 && windDeg <= 206) windDegString = "Южный";
  if (windDeg >= 207 && windDeg <= 252) windDegString = "Юго-западный";
  if (windDeg >= 253 && windDeg <= 298) windDegString = "Западный";
  if (windDeg >= 299 && windDeg <= 344) windDegString = "Северо-западный";
  return windDegString;
}



// =======================================================================
// Берем время у GOOGLE
// =======================================================================

float utcOffset = 3; //поправка часового пояса
long localEpoc = 0;
long localMillisAtUpdate = 0;

void getTime()
{
  WiFiClient client;
  if (!client.connect("www.google.ru", 80)) {
    Serial.println("connection to google failed");
    return;
  }

  client.print(String("GET / HTTP/1.1\r\n") +
               String("Host: www.google.com\r\n") +
               String("Connection: close\r\n\r\n"));
  int repeatCounter = 0;
  while (!client.available() && repeatCounter < 10) {
    delay(500);
    //Serial.println(".");
    repeatCounter++;
  }
  Serial.println("Time string:");
  String line;
  client.setNoDelay(false);
  while (client.connected() && client.available()) {
    line = client.readStringUntil('\n');
    //Serial.println(line);
    line.toUpperCase();
    if (line.startsWith("DATE: ")) {
      week = line.substring(6, 9);
      int dayint = line.substring(11, 13).toInt();
      mont = line.substring(14, 17);
      years = line.substring(18, 22);
      h = line.substring(23, 25).toInt();
      m = line.substring(26, 28).toInt();
      s = line.substring(29, 31).toInt();
      localMillisAtUpdate = millis();
      localEpoc = (h * 60 * 60 + m * 60 + s);

      // переводим на русский названия дней недели
      if (week == "MON") week = "Понедельник";
      if (week == "TUE") week = "Вторник";
      if (week == "WED") week = "Среда";
      if (week == "THU") week = "Четверг";
      if (week == "FRI") week = "Пятница";
      if (week == "SAT") week = "Суббота";
      if (week == "SUN") week = "Воскресенье";

      // переводим на русский названия месяцев
      if (mont == "JAN") mont = "Января";
      if (mont == "FEB") mont = "Февраля";
      if (mont == "MAR") mont = "Марта";
      if (mont == "APR") mont = "Апреля";
      if (mont == "MAY") mont = "Мая";
      if (mont == "JUN") mont = "Июня";
      if (mont == "JUL") mont = "Июля";
      if (mont == "AUG") mont = "Августа";
      if (mont == "SEP") mont = "Сентября";
      if (mont == "OCT") mont = "Октября";
      if (mont == "NOV") mont = "Ноября";
      if (mont == "DEC") mont = "Декабря";

      if (dayint <= 9) days = String (dayint % 10);
      else days = String (dayint);

      date = (week + " " + days + " " + mont + " " + years);
    }
  }
  Serial.println("");
  client.stop();
}

// =======================================================================

void updateTime()
{
  long curEpoch = localEpoc + ((millis() - localMillisAtUpdate) / 1000);
  long epoch = round(curEpoch + 3600 * utcOffset + 86400L) % 86400L;
  h = ((epoch  % 86400L) / 3600) % 24;
  m = (epoch % 3600) / 60;
  s = epoch % 60;
}

// =======================================================================


String utf8rus(String source)
{
  int i, k;
  String target;
  unsigned char n;
  char m[2] = { '0', '\0' };

  k = source.length(); i = 0;

  while (i < k) {
    n = source[i]; i++;

    if (n >= 0xC0) {
      switch (n) {
        case 0xD0: {
            n = source[i]; i++;
            if (n == 0x81) {
              n = 0xA8;
              break;
            }
            if (n >= 0x90 && n <= 0xBF) n = n + 0x30 - 1;
            break;
          }
        case 0xD1: {
            n = source[i]; i++;
            if (n == 0x91) {
              n = 0xB8;
              break;
            }
            if (n >= 0x80 && n <= 0x8F) n = n + 0x70 - 1;
            break;
          }
      }
    }
    m[0] = n; target = target + String(m);
  }
  return target;
}
