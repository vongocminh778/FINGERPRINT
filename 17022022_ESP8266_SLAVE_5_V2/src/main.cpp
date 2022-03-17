#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

const unsigned int PIN1 = 4; // BT
const unsigned int PIN2 = 5; // QL

// //Declare wifi and password
// const char *ssid = "Matsuya MIC";
// const char *password = "M@tsuyaR&D2020";
const char *ssid = "Matsuya_R&D_factory";
const char *password = "Matsuya@2017";

WiFiClient espClient;
PubSubClient client(espClient);

// String _device_name_slave = "ESP8266_SLAVE_1";
// String _device_name_master = "D_FP_1";
// char const *_Topic_send_master = "esp8266/input/checkstatus/ESP8266_SLAVE_1";
// char const *_Topic_receive_master = "esp8266/output/checkstatus/ESP8266_SLAVE_1";

// String _device_name_slave = "ESP8266_SLAVE_2";
// String _device_name_master = "D_FP_2";
// char const *_Topic_send_master = "esp8266/input/checkstatus/ESP8266_SLAVE_2";
// char const *_Topic_receive_master = "esp8266/output/checkstatus/ESP8266_SLAVE_2";

// String _device_name_slave = "ESP8266_SLAVE_3";
// String _device_name_master = "D_FP_3";
// char const *_Topic_send_master = "esp8266/input/checkstatus/ESP8266_SLAVE_3";
// char const *_Topic_receive_master = "esp8266/output/checkstatus/ESP8266_SLAVE_3";

// String _device_name_slave = "ESP8266_SLAVE_4";
// String _device_name_master = "D_FP_4";
// char const *_Topic_send_master = "esp8266/input/checkstatus/ESP8266_SLAVE_4";
// char const *_Topic_receive_master = "esp8266/output/checkstatus/ESP8266_SLAVE_4";

String _device_name_slave = "ESP8266_SLAVE_5";
String _device_name_master = "D_FP_5";
char const *_Topic_send_master = "esp8266/input/checkstatus/ESP8266_SLAVE_5";
char const *_Topic_receive_master = "esp8266/output/checkstatus/ESP8266_SLAVE_5";

// String _device_name_slave = "ESP8266_SLAVE_6";
// String _device_name_master = "D_FP_6";
// char const *_Topic_send_master = "esp8266/input/checkstatus/ESP8266_SLAVE_6";
// char const *_Topic_receive_master = "esp8266/output/checkstatus/ESP8266_SLAVE_6";

const char *mqtt_server = "192.168.1.56";
byte machineStatus1;
byte machineStatus2;
String Flagcheckstatus1;
String Flagcheckstatus2;
unsigned long previousMillis = 0;
int couter = 4;
bool flag_send = 0;
String messageTemp = "";
int count_timer = 0;
bool sent_inf;
// bool recieve_success;
int count_times = 0;

void setup_wifi();
void callback(char *topic, byte *message, unsigned int length);
void send_data_mqtt(char const *_path, String flag_status1, String flag_status2);
void reconnect();

void setup()
{
  Serial.begin(115200);
  pinMode(PIN1, OUTPUT);
  pinMode(PIN2, OUTPUT);

  // settings wifi and mqtt
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  machineStatus1 = LOW; // set first status
  machineStatus2 = LOW; // set first status
  Flagcheckstatus1 = "flag0";
  Flagcheckstatus2 = "flag0";
  digitalWrite(PIN1, machineStatus1);
  digitalWrite(PIN2, machineStatus2);
  Serial.println("Ket noi thanh cong");
}

//=======================================================================
//                    Main Program Loop
//=======================================================================
void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= 30000))
  {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
  else
  {
    if (couter == 0)
    {
      Serial.println("Reset now...");
      ESP.restart();
    }
    if (flag_send == 1)
    {
      send_data_mqtt(_Topic_send_master, Flagcheckstatus1, Flagcheckstatus2);
      flag_send = 0;
      delay(2000);
    }
  }
}

void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.hostname(_device_name_slave);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *message, unsigned int length)
{
  count_times++;
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  messageTemp = "";
  for (unsigned i = 0; i < length; i++)
  {
    messageTemp += (char)message[i];
  }
  Serial.print("Message: ");
  Serial.println(messageTemp);

  // Serial.println();
  // Changes the output state according to the message
  if (String(topic) == _Topic_receive_master)
  {
    // recieve_success = true;
    StaticJsonDocument<192> doc;

    DeserializationError error = deserializeJson(doc, messageTemp);

    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    const char *_device_name = doc["device_name"];
    const char *Status1 = doc["status1"]; // "flag0"
    const char *Status2 = doc["status2"]; // "flag0"
    Serial.print(F("Status1 :"));
    Serial.println(Status1);
    Serial.print(F("Status2 :"));
    Serial.println(Status2);
    String check_name = _device_name;
    if (check_name == _device_name_master)
    {
      if (String(Status1) == "flag1") // light1
      {
        machineStatus1 = HIGH;
        // Flagcheckstatus1 = (machineStatus1 == HIGH) ? "flag1" : "flag0";
        if (machineStatus2 == HIGH)
        {
          machineStatus2 = LOW; // Light A turn ON => Light B turn OFF
        }
        Flagcheckstatus1 = Status1;
        digitalWrite(PIN1, machineStatus1);
        Serial.println("Line 1 ON");
      }
      else if (String(Status1) == "flag0")
      {
        machineStatus1 = LOW;
        Flagcheckstatus1 = Status1;
        digitalWrite(PIN1, machineStatus1);
        Serial.println("Line 1 OFF");
      }
      if (String(Status2) == "flag1") // light2
      {
        machineStatus2 = HIGH;
        // Flagcheckstatus2 = (machineStatus2 == HIGH) ? "flag1" : "flag0";
        if (machineStatus1 == HIGH)
        {
          machineStatus1 = LOW; // Light B turn ON => Light A turn OFF
        }
        Flagcheckstatus2 = Status2;
        digitalWrite(PIN2, machineStatus2);
        Serial.println("Line 2 ON");
      }
      else if (String(Status2) == "flag0")
      {
        machineStatus2 = LOW;
        Flagcheckstatus2 = Status2;
        digitalWrite(PIN2, machineStatus2);
        Serial.println("Line 2 OFF");
      }

      // if (messageTemp.length() > 0)
      // {
      //   send_data_mqtt(_Topic_send_master, Flagcheckstatus1, Flagcheckstatus2);
      //   recieve_success = false;
      // }
    }
  }
}

void send_data_mqtt(char const *_path, String flag_status1, String flag_status2)
{
  StaticJsonDocument<192> doc;
  doc["device_name"] = _device_name_slave;
  doc["status1"] = flag_status1;
  doc["status2"] = flag_status2;
  char output[192];

  serializeJson(doc, output);
  Serial.println(output);
  if (client.publish(_path, output) == true)
  {
    flag_send = 0;
    Serial.println(F("Success sending mqtt"));
    count_timer = 0;
    sent_inf = true;
  }
  else
  {
    flag_send = 1;
    couter--;
    Serial.println(F("Error sending mqtt"));
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.println(client.connected());
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(_device_name_slave.c_str()))
    {
      Serial.println("connected");
      // Subscribe
      client.subscribe(_Topic_receive_master, 1);
      client.getWriteError();
    }
    else
    {
      WiFi.disconnect();
      WiFi.reconnect();
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
