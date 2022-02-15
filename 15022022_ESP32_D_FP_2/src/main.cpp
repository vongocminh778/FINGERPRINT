#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "FPC1020.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ezButton.h> /*Button library*/
#include <ArduinoJson.h>
#include <HTTPClient.h>
// Replace the next variables with your SSID/Password combination
// const char* ssid = "MI";
// const char* password = "123456789";
// const char *ssid = "Matsuya MIC";
// const char *password = "M@tsuyaR&D2020";
const char *ssid = "Matsuya_R&D_factory";
const char *password = "Matsuya@2017";

const char *mqtt_server = "113.161.152.35";

#define SCREEN_WIDTH 128 /*OLED display width, in pixels*/
#define SCREEN_HEIGHT 64 /*OLED display height, in pixels*/
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#define BTN_XN 27
ezButton button(BTN_XN);             /*Declare pin for button*/
#define BUTTON_PIN_BITMASK 0x8000000 // 2^27 in hex
RTC_DATA_ATTR int bootCount = 0;

WiFiClient espClient;
PubSubClient client(espClient);
FPC1020 fpc;

extern uint8_t rBuf[36864];
bool flag_finger = false; // flag check status fingerprint sensor
bool cal_time_1 = false;  // flag check button pressed
bool flag_mqtt_success = true;
String messageTemp = ""; // msg callback return
String _device_name_master = "D_FP_2";         // Device name
String _device_name_slave = "ESP8266_SLAVE_2"; // Device name
String _status1 = "";
String _status2 = "";
int count = 10; // variable check error to restart esp

char const *_Topic_send_callmaintenance = "esp32/input/callmaintenance/D_FP_2";     // send call maintanance
char const *_Topic_receive_callmaintenance = "esp32/output/callmaintenance/D_FP_2"; // receive call maintanance

char const *_Topic_send_cancelmaintenance = "esp32/input/cancelmaintenance/D_FP_2";
char const *_Topic_receive_cancelmaintenance = "esp32/output/cancelmaintenance/D_FP_2";

char const *_Topic_send_slave = "esp8266/output/checkstatus/ESP8266_SLAVE_2";
char const *_Topic_receive_slave = "esp8266/input/checkstatus/ESP8266_SLAVE_2";

const char *serverName_rawfingerimage = "http://113.161.152.35:2087/api/Fingers/fpimage";
// const char *serverName_rawfingerimage = "http://113.161.152.35/WebAPI_FN/api/values/imgfp";

unsigned long pressedTime = 0;  /*variable for button*/
unsigned long releasedTime = 0; /*variable for button*/

// Timer0
volatile int interruptCounter;
int totalInterruptCounter;
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
void IRAM_ATTR onTimer()
{
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Timer1
volatile int interruptCounter1;
int totalInterruptCounter1;
hw_timer_t *timer1 = NULL;
portMUX_TYPE timerMux1 = portMUX_INITIALIZER_UNLOCKED;
void IRAM_ATTR onTimer1()
{
  // Critical Code here
  portENTER_CRITICAL_ISR(&timerMux1);
  interruptCounter1++;
  portEXIT_CRITICAL_ISR(&timerMux1);
}

// Declare function
void setup_wifi();
void callback(char *topic, byte *message, unsigned int length);
void reconnect();
void display_oled(int size_character, int x, int y, String content);
void send_data_mqtt(char const *_path, String _emp_number);
void send_data_slave(char const *_path, String Status1, String Status2);
String send_data_raw_image(uint8_t *payload);

// Setup default
void setup()
{
  Serial.println(F("---------------fnsend_data_raw_image---------------"));
  Serial.begin(115200);

  // oled setting
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize with the I2C addr 0x3C
  display.clearDisplay();                    // Clear the buffer.
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  // display.setRotation(2); //rotate oled

  // setup timer0
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);

  // setup timer1
  timer1 = timerBegin(1, 80, true);              // timer 1, MWDT clock period = 12.5 ns * TIMGn_Tx_WDT_CLK_PRESCALE -> 12.5 ns * 80 -> 1000 ns = 1 us, countUp
  timerAttachInterrupt(timer1, &onTimer1, true); // edge (not level) triggered
  timerAlarmWrite(timer1, 250000, true);         // 250000 * 1 us = 250 ms, autoreload true
  timerAlarmEnable(timer1);                      // enable

  // Configuration GPIO
  button.setDebounceTime(50); // set debounce button time to 50 milliseconds
  pinMode(BTN_XN, INPUT_PULLUP);

  // default settings
  setup_wifi();

  // mqtt callback
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Function FPC1020
  fpc.reset();
  fpc.init();
  delay(10);
  fpc.setup();

  display_oled(2, 10, 20, "Bat Dau!!");
  delay(1000);
}

void loop()
{
  // timer for deep mode
  if (interruptCounter > 0)
  {
    portENTER_CRITICAL(&timerMux);
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);
    totalInterruptCounter++;
  }
  if (interruptCounter1 > 0)
  {
    portENTER_CRITICAL(&timerMux1);
    interruptCounter1--;
    portEXIT_CRITICAL(&timerMux1);
    totalInterruptCounter1++;
  }

  // button
  button.loop();

  // mqtt
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  // setup fpc1020 capture img
  fpc.command(FPC102X_REG_FINGER_PRESENT_QUERY); // 0X20
  fpc.command(FPC102X_REG_WAIT_FOR_FINGER);      // 0X24
  delay(10);
  uint8_t interrupt = fpc.interrupt(true);
  if (interrupt == 0x81) // have a finger ?
  {
    if (flag_finger == false) // captured img
    {
      messageTemp = "";
      fpc.command(FPC1020X_REG_CAPTURE_IMAGE);
      delay(20);
      uint8_t interrupt = fpc.interrupt(true);
      Serial.printf_P("Interrupt status: 0x%02X\n", interrupt);
      totalInterruptCounter = 0; // set flag sleep mode back to default
      delay(20);
      if (interrupt == 0x20)
      {
        fpc.capture_image(); // capture img
        delay(10);
        flag_finger = true;
        messageTemp = send_data_raw_image(&rBuf[0]);
      }
    }
    if (flag_finger == true && messageTemp != "" && messageTemp != "Null")
    {
      if (button.isPressed() && cal_time_1 == false)
      {
        pressedTime = millis();
        cal_time_1 = true;
      }
    }
    else if (flag_finger == true && messageTemp == "")
    {
      display_oled(2, 6, 10, F(" Loi ket     noi!"));
    }
    else
    {
      display_oled(2, 6, 10, F(" Xin thu     lai!"));
    }
  }
  else if (interrupt != 0x81 && cal_time_1 == false)
  {
    display_oled(2, 10, 20, "Bat Dau!!");
    messageTemp = "";
    flag_finger = false;
  }

  if (button.isReleased() && cal_time_1 == true)
  {
    cal_time_1 = false;
    releasedTime = millis();
    long pressDuration = releasedTime - pressedTime;
    if (pressDuration < 1000)
    {
      send_data_mqtt(_Topic_send_callmaintenance, messageTemp);
      Serial.println("A short press is detected");
      messageTemp = "";
      delay(1000);
    }
    else if (pressDuration > 1000)
    {
      send_data_mqtt(_Topic_send_cancelmaintenance, messageTemp);
      Serial.println("A long press is detected");
      messageTemp = "";
      delay(1000);
    }
  }
  if (totalInterruptCounter1 > 10 && flag_mqtt_success == false) // check MQTT receive
  {
    send_data_slave(_Topic_send_slave, _status1, _status2); // ON , OFF
    totalInterruptCounter1 = 0;
  }
  /*******************Sleep Mode****************************/
  //  if(totalInterruptCounter > 10){
  //   ++bootCount;
  //   Serial.println("Boot number: " + String(bootCount));
  //   esp_sleep_enable_ext0_wakeup(GPIO_NUM_27,0);
  //   Serial.println(F("Going to sleep now"));
  //   display_oled(2, 25,20, "Sleep!!");  //Function Display
  //   esp_deep_sleep_start();
  //  }
  if (count <= 0) // disconnect server
  {
    display_oled(2, 25, 20, "Reset!");
    delay(100);
    ESP.restart();
  }
  memset(rBuf, 0, 36864); // free memory
  ESP.getFreeHeap();
}

String send_data_raw_image(uint8_t *payload)
{
  Serial.println(F("---------------fnsend_data_raw_image---------------"));
  HTTPClient http;
  http.setConnectTimeout(500); // time connection to server
  http.setTimeout(500);        // time get response from server
  http.begin(serverName_rawfingerimage);
  http.addHeader("Content-Type", "application/octet-stream");
  int httpResponseCode = http.sendRequest("POST", (uint8_t *)payload, 36864);
  if (httpResponseCode > 0)
  {
    String response_confirm = http.getString();
    Serial.println(response_confirm); // Get the response to the request
    Serial.println(httpResponseCode); // Print return code
    (response_confirm.length() <= 4) ? display_oled(3, 25, 20, response_confirm) : display_oled(2, 6, 10, F(" Xin thu     lai!"));
    flag_finger = true;
    delay(100);
    return response_confirm;
  }
  else
  {
    Serial.print(F("Error on sending POST: "));
    Serial.println(httpResponseCode);
    flag_finger = true;
    delay(100);
    count--;
    return "";
  }
  http.end();
  ESP.getFreeHeap();
}
// function send data to mqtt
void send_data_mqtt(char const *_path, String _emp_number)
{
  Serial.println(F("---------------fnsend_data_mqtt---------------"));
  StaticJsonDocument<96> doc; // Serialize to json
  doc["device_name"] = _device_name_master;
  doc["emp_number"] = _emp_number;
  doc["flag_control"] = "";
  doc["value_sensity"] = "";
  char output[96];

  serializeJson(doc, output);

  if (client.publish(_path, output) == true)
  {
    Serial.println(F("---------------Success sending mqtt master---------------"));
  }
  else
  {
    Serial.println(F("Error sending mqtt"));
  }
}

void send_data_slave(char const *_path, String Status1, String Status2)
{
  Serial.println(F("---------------fnsend_data_slave---------------"));
  StaticJsonDocument<96> doc; // Serialize to json
  doc["device_name"] = _device_name_master;
  doc["status1"] = Status1;
  doc["status2"] = Status2;
  char output[96];

  serializeJson(doc, output);

  if (client.publish(_path, output) == true)
  {
    Serial.println(F("---------------Success sending mqtt slave---------------"));
  }
  else
  {
    Serial.println(F("Error sending mqtt"));
  }
}

// function connection to wifi
void setup_wifi()
{
  Serial.println(F("---------------fnsetup_wifi---------------"));
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.setHostname(("Fingerprint_" + _device_name_master).c_str()); // define hostname
  WiFi.begin(ssid, password);
  display.println(F("Connecting"));

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    display.print(F("."));
  }

  // Clear the buffer.
  display.clearDisplay();
  display.display();
  display.setCursor(0, 0);

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // display.println(F("Wifi connected!"));
  // display.print(F("IP:"));
  // display.println(WiFi.localIP());
  // display.display();
}

// function listen mqtt return data
void callback(char *topic, byte *message, unsigned int length)
{
  Serial.println(F("---------------fncallback---------------"));
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  messageTemp = "";
  for (int i = 0; i < length; i++)
  {
    messageTemp += (char)message[i];
  }
  Serial.print("Message: ");
  Serial.println(messageTemp);
  Serial.println();

  if (String(topic) == _Topic_receive_callmaintenance || String(topic) == _Topic_receive_cancelmaintenance)
  {
    if (messageTemp != "Null")
    {
      totalInterruptCounter1 = 0;
      // display_oled(3, 25, 20, messageTemp);
      String flag = messageTemp.substring(0, 5);
      if (flag == "flag0")
      {
        display_oled(2, 6, 10, F(" Xin thu     lai!"));
        delay(2000);
      }
      else if (flag == "flag1")
      {
        display_oled(2, 6, 10, F(" Goi bao     tri!"));
        _status1 = "flag1";
        _status2 = "flag0";
        send_data_slave(_Topic_send_slave, _status1, _status2); // ON , OFF
        flag_mqtt_success = false;
        delay(2000);
      }
      else if (flag == "flag2")
      {
        display_oled(2, 6, 10, F("  Bat dau   bao tri!"));
        _status1 = "flag0";
        _status2 = "flag0";
        send_data_slave(_Topic_send_slave, _status1, _status2); // ON , OFF
        flag_mqtt_success = false;
        delay(2000);
      }
      else if (flag == "flag3")
      {
        display_oled(2, 6, 10, F("Ket thuc   bao tri! "));
        _status1 = "flag0";
        _status2 = "flag1";
        send_data_slave(_Topic_send_slave, _status1, _status2); // ON , OFF
        flag_mqtt_success = false;
        delay(2000);
      }
      else if (flag == "flag4")
      {
        display_oled(2, 6, 10, F("Xac nhan   bao tri! "));
        _status1 = "flag0";
        _status2 = "flag0";
        send_data_slave(_Topic_send_slave, _status1, _status2); // ON , OFF
        flag_mqtt_success = false;
        delay(2000);
      }
      else if (flag == "flag5")
      {
        display_oled(2, 6, 10, F(" Huy bao     tri!"));
        _status1 = "flag0";
        _status2 = "flag0";
        send_data_slave(_Topic_send_slave, _status1, _status2); // ON , OFF
        flag_mqtt_success = false;
        delay(2000);
      }
      else
      {
        display_oled(2, 6, 10, F(" Xin thu     lai!"));
      }
    }
    else
    {
      display_oled(2, 6, 10, F(" Xin thu     lai!"));
      flag_finger = false;
    }
  }

  if (String(topic) == _Topic_receive_slave)
  {
    Serial.println("---------------callback from slave---------------");
    flag_mqtt_success = true;
    StaticJsonDocument<192> doc;
    DeserializationError error = deserializeJson(doc, messageTemp);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    const char *device_name = doc["device_name"]; // "flag0"
    const char *status1 = doc["status1"];         // "flag0"
    const char *status2 = doc["status2"];         // "flag0"
    Serial.print(F("status1 :"));
    Serial.println(status1);
    Serial.print(F("status2 :"));
    Serial.println(status2);
    if (_device_name_slave == device_name)
    {
      if (String(status1) != _status1 || String(status2) != _status2)
      {
        send_data_slave(_Topic_send_slave, _status1, _status2); // ON , OFF
        Serial.println("---------------------------------");
        Serial.print(F("status1 :"));
        Serial.println(String(status1));
        Serial.print(F("status2 :"));
        Serial.println(String(status2));
        flag_mqtt_success = false;
        totalInterruptCounter1 = 0;
      }
    }
  }
}

// function reconnec when loss signal wifi
void reconnect()
{
  Serial.println(F("---------------fnreconnect---------------"));
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(_device_name_master.c_str()))
    {
      Serial.println("connected");
      // Subscribe
      // client.subscribe(_Topic_receive_img);
      client.subscribe(_Topic_receive_callmaintenance);
      client.subscribe(_Topic_receive_cancelmaintenance);
      client.subscribe(_Topic_receive_slave);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      count--;
      if (count <= 3)
      {
        display_oled(2, 25, 20, "Reset!");
        delay(100);
        ESP.restart();
      }
      Serial.println(count);
      delay(1000);
    }
  }
}

// function display oled
void display_oled(int size_character, int x, int y, String content)
{
  display.clearDisplay();
  display.setTextSize(size_character);
  display.setTextColor(WHITE);
  display.setCursor(x, y);
  display.println(content);
  // display.setRotation(2);
  display.display();
}