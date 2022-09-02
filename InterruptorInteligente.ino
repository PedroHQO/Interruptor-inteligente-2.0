
 
//WIFI -----------------------
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
 
 
//Arduino--------------------
#include <EEPROM.h>
 
 
//Sinric Pro-----------------
#include <SinricPro.h>
#include <SinricProSwitch.h>
 
//Settings-----------------------------------------------------------------------------------
#define MySSID "Querino"                                  
#define MyWifiPassword "09080288"               
 
#define Sinric_Key          "801038c6-96e0-429e-a6aa-004e1966a5a6"
#define Sinric_Secret       "47fc56f3-5198-49c9-a878-1aaa3b4f3c56-a6bad080-66a1-4ece-a6ef-d06d2e7779ca"
 
#define Switch_ID           "63051599da0cc632437401d6"
#define UpdateDelay  5                                    //5 Seconds before HUB Update
 
#define SwitchGND   0                                     //Switch Ground Pin
#define SwitchPin   5                                                                                                                                                                                                                                                             //Switch Control Pin
#define RelayPin    4
//Living Room Light Relay Pin
//-------------------------------------------------------------------------------------------
 
 
//Object Declarations
HTTPClient http;
WiFiClient client;
WiFiServer server(80);
 
 
//Control Variables------------
bool State = false;
bool SwitchState = false;
 
//Function Prototypes------------------------------------------------
void LAN();
void turnOn(String deviceId);
void turnOff(String deviceId);
void UpdateHub(bool CommandSent = false);
 
bool onPowerState(const String &deviceId, bool &state) 
{
  if(state)
    turnOn(deviceId);
  else
    turnOff(deviceId);
     
  return true; // request handled properly
}
 
 
 
 
void setup() 
{
  pinMode(SwitchGND, OUTPUT);
  pinMode(SwitchPin, INPUT);
  pinMode(RelayPin, OUTPUT);
   
  digitalWrite(SwitchGND, LOW);
  digitalWrite(RelayPin,  LOW);
  
   
  EEPROM.begin(4);
  State = EEPROM.read(1);
  SwitchState = EEPROM.read(2);
 
  if(State)
    digitalWrite(RelayPin, LOW);
  else
    digitalWrite(RelayPin, HIGH);
     
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  
  //WiFi Connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(MySSID, MyWifiPassword);
  Serial.print("\nConnecting to Wifi: ");
  Serial.print(MySSID);
 
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println("[OK]");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
 
  //Setup SinricPro + Callback Functions
  SinricProSwitch& mySwitch = SinricPro[Switch_ID];
  mySwitch.onPowerState(onPowerState);
  SinricPro.onConnected   ([](){ Serial.println("[SinricPro]: Connected");});
  SinricPro.onDisconnected([](){ Serial.println("[SinricPro]: Disconnected");});
  SinricPro.begin(Sinric_Key, Sinric_Secret);
   
  //Start the server
  server.begin();
 
  //Start OTA Update Service
  ArduinoOTA.setHostname("Interruptor Inteligente");
  ArduinoOTA.begin();
 
  //Blink Lights to Indicate Successful Boot
  for(int i = 0; i < 2; i++)
  {
    delay(1000);
    if(State)
      digitalWrite(RelayPin, HIGH);
    else
      digitalWrite(RelayPin, LOW);
    delay(1000);
    if(State)
      digitalWrite(RelayPin, LOW);
    else
      digitalWrite(RelayPin, HIGH);
  }
}
 
 
 
 
void loop()
{
  ArduinoOTA.handle();
   
  SinricPro.handle();
 
  LAN();
 
  UpdateHub(false);
   
  static uint64_t then = millis()/100;
  static uint64_t now = millis()/100;
   
  now = millis()/100;
  if(now - then > 1)
  {
   bool newSwitchState = digitalRead(SwitchPin);
    if(newSwitchState != SwitchState)
    {
      then = now;
      SwitchState = newSwitchState;
      EEPROM.write( 2, SwitchState);
      EEPROM.commit();
           
      if(State)
      {
        turnOff(Switch_ID);
      }
      else
      {
        turnOn(Switch_ID);
      }
    }
  }
}
 
 
void turnOn(String deviceId) 
{
  if (deviceId == Switch_ID)
  {
    digitalWrite(RelayPin, LOW); 
 
    State = true;
     
    EEPROM.write( 1, State);
    EEPROM.commit();
 
    Serial.println("  Turn ON");
 
    UpdateHub(true);
  }
}
 
void turnOff(String deviceId) 
{
   if (deviceId == Switch_ID)
   {
     digitalWrite(RelayPin, HIGH); 
      
     State = false;
 
     EEPROM.write( 1, State);
     EEPROM.commit();
 
     Serial.println("  Turn OFF");
 
     UpdateHub(true);
   }
}
 
 
void LAN()
{
  //Check if a client has connected
  WiFiClient clientLAN = server.available();
  if (!clientLAN)
    return;
 
  //Wait until the client sends some data
  while (!clientLAN.available()) 
  {
    delay(1);
  }
 
  //Read the first line of the request
  Serial.print("\nLAN request: ");
  String request = clientLAN.readStringUntil('\r');
  Serial.println(request);
 
   
  //Handle the request
  if (request.indexOf("OK") != -1)
    return;
  if (request.indexOf("RESTART") != -1)
    ESP.restart();
   
  if (request.indexOf("_off") != -1)
    turnOff(Switch_ID);
  if (request.indexOf("_on") != -1)
    turnOn(Switch_ID);
     
  clientLAN.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
  clientLAN.flush();
}
 
 
void UpdateHub(bool CommandSent)
{
  static uint64_t LastUpdate = millis()/1000;
 
  if(millis()/1000 - LastUpdate >= 120)
  {
    SinricProSwitch& mySwitch = SinricPro[Switch_ID];
    mySwitch.sendPowerStateEvent(State);
    LastUpdate = millis()/1000;
  }
}
