/*
  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

// NMEA2000 Remote Control for EV-1
//   - Reads 433 MHz commands via RXB6 receiver
//   - Sends NMEA2000 messages to EV-1 Course Computer

// Version 0.7, 29.08.2021, AK-Homberger

#define ESP32_CAN_TX_PIN GPIO_NUM_5  // Set CAN TX port to 5 
#define ESP32_CAN_RX_PIN GPIO_NUM_4  // Set CAN RX port to 4

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <N2kMsg.h>
#include <NMEA2000.h>
#include <NMEA2000_CAN.h>
#include <RCSwitch.h>
#include <algorithm>

//#include <Arduino.h>
//#include <NMEA2000_CAN.h>  // This will automatically choose right CAN library and create suitable NMEA2000 object
#include <Seasmart.h>
//#include <N2kMessages.h>
//#include <WiFi.h>
#include <WebServer.h>
//#include <OneWire.h>
//#include <OneButton.h>
//#include <DallasTemperature.h>
//#include <Preferences.h>
#include <ArduinoJson.h>

#include "N2kDataToNMEA0183.h"
#include "List.h"
#include "index_html.h"

#include "RaymarinePilot.h"
#include "N2kDeviceList.h"
#include "BoatData.h"
//////////////////////////////////////////////////////////////////////////////////////////////////
/*
  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

// Version 1.3, 04.08.2020, AK-Homberger

//#define ESP32_CAN_TX_PIN GPIO_NUM_5  // Set CAN TX port to 5 (Caution!!! Pin 2 before)
//#define ESP32_CAN_RX_PIN GPIO_NUM_4  // Set CAN RX port to 4

//#define ESP32_CAN_TX_PIN GPIO_NUM_5  // Set CAN TX port to 5 
//#define ESP32_CAN_RX_PIN GPIO_NUM_4  // Set CAN RX port to 4





#define ENABLE_DEBUG_LOG 0 // Debug log, set to 1 to enable AIS forward on USB-Serial / 2 for ADC voltage to support calibration
#define UDP_Forwarding 0   // Set to 1 for forwarding AIS from serial2 to UDP brodcast
//#define HighTempAlarm 12   // Alarm level for fridge temperature (higher)
//#define LowVoltageAlarm 11 // Alarm level for battery voltage (lower)

//#define ADC_Calibration_Value 34.3 // The real value depends on the true resistor values for the ADC input (100K / 27 K)

#define WLAN_CLIENT 0  // Set to 1 to enable client network. 0 to act as AP only

// Wifi cofiguration Client and Access Point
const char *AP_ssid = "toke_nmea";  // ESP32 as AP
const char *CL_ssid = "MyWLAN";   // ESP32 as client in network

const char *AP_password = "tokeger6668";   // AP password. Must be longer than 7 characters
const char *CL_password = "clientpassword";  // Client password

// Put IP address details here
IPAddress AP_local_ip(192, 168, 15, 1);  // Static address for AP
IPAddress AP_gateway(192, 168, 15, 1);
IPAddress AP_subnet(255, 255, 255, 0);

IPAddress CL_local_ip(192, 168, 1, 10);  // Static address for Client Network. Please adjust to your AP IP and DHCP range!
IPAddress CL_gateway(192, 168, 1, 1);
IPAddress CL_subnet(255, 255, 255, 0);

int wifiType = 0; // 0= Client 1= AP

const uint16_t ServerPort = 2222; // Define the port, where server sends data. Use this e.g. on OpenCPN. Use 39150 for Navionis AIS

// UPD broadcast for Navionics, OpenCPN, etc.
const char * udpAddress = "192.168.15.255"; // UDP broadcast address. Should be the network of the ESP32 AP (please check!)
const int udpPort = 2000; // port 2000 lets think Navionics it is an DY WLN10 device

// Create UDP instance
WiFiUDP udp;

// Struct to update BoatData. See BoatData.h for content
tBoatData BoatData;

int NodeAddress;  // To store last Node Address

Preferences preferences;             // Nonvolatile storage on ESP32 - To store LastDeviceAddress

int buzzerPin = 12;   // Buzzer on GPIO 12
//int buttonPin = 0;    // Button on GPIO 0 to acknowledge alarm with buzzer
//int alarmstate = false; // Alarm state (low voltage/temperature)
//int acknowledge = false; // Acknowledge for alarm, button pressed

//OneButton button(buttonPin, false); // The OneButton library is used to debounce the acknowledge button

const size_t MaxClients = 10;
bool SendNMEA0183Conversion = true; // Do we send NMEA2000 -> NMEA0183 conversion
bool SendSeaSmart = false; // Do we send NMEA2000 messages in SeaSmart format

WiFiServer server(ServerPort, MaxClients);
WiFiServer json(90);

using tWiFiClientPtr = std::shared_ptr<WiFiClient>;
LinkedList<tWiFiClientPtr> clients;

tN2kDataToNMEA0183 tN2kDataToNMEA0183(&NMEA2000, 0);

// Set the information for other bus devices, which messages we support
const unsigned long TransmitMessages[] PROGMEM = {127489L, // Engine dynamic
                                                  126208UL,   // Set Pilot Mode
                                                  126720UL,   // Send Key Command
                                                  65288UL,    // Send Seatalk Alarm State
                                                  0
                                                 };
const unsigned long ReceiveMessages[] PROGMEM = {/*126992L,*/ // System time
      127250L, // Heading
      127258L, // Magnetic variation
      128259UL,// Boat speed
      128267UL,// Depth
      129025UL,// Position
      129026L, // COG and SOG
      129029L, // GNSS
      130306L, // Wind
      128275UL,// Log
      127245UL,// Rudder
      65288UL,    // Read Seatalk Alarm State
      65379UL,    // Read Pilot Mode
      0
    };

// Forward declarations
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg);
void SendNMEA0183Message(const tNMEA0183Msg &NMEA0183Msg);

// Data wire for teperature (Dallas DS18B20) is plugged into GPIO 13 on the ESP32
//#define ONE_WIRE_BUS 13
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
//OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
//DallasTemperature sensors(&oneWire);

WebServer webserver(80);

#define MiscSendOffset 120
#define SlowDataUpdatePeriod 1000  // Time between CAN Messages sent

// Battery voltage is connected GPIO 34 (Analog ADC1_CH6)
//const int ADCpin = 34;
//float voltage = 0;
//float temp = 0;

// Task handle for OneWire read (Core 0 on ESP32)
TaskHandle_t Task1;

// Serial port 2 config (GPIO 16)
const int baudrate = 38400;
const int rs_config = SERIAL_8N1;

// Buffer config

#define MAX_NMEA0183_MESSAGE_SIZE 150 // For AIS
char buff[MAX_NMEA0183_MESSAGE_SIZE];

// NMEA message for AIS receiving and multiplexing
tNMEA0183Msg NMEA0183Msg;
tNMEA0183 NMEA0183;


void debug_log(char* str) {
#if ENABLE_DEBUG_LOG == 1
  Serial.println(str);
#endif
}

//*****************************************************************************

void Ereignis_Index()    // Wenn "http://<ip address>/" aufgerufen wurde
{
  webserver.send(200, "text/html", indexHTML);  //dann Index Webseite senden
}

void Ereignis_js()      // Wenn "http://<ip address>/gauge.min.js" aufgerufen wurde
{
  webserver.send(200, "text/html", gauge);     // dann gauge.min.js senden
}

void handleNotFound()
{
  webserver.send(404, "text/plain", "File Not Found\n\n");
}

void setup_wifi_ap() {

  uint8_t chipid[6];
  uint32_t id = 0;
  int i = 0;
  int wifi_retry = 0;

  // Init USB serial port
  Serial.begin(115200);

  // Init AIS serial port 2
  Serial2.begin(baudrate, rs_config);
  NMEA0183.Begin(&Serial2, 3, baudrate);

  if (WLAN_CLIENT == 1) {
    Serial.println("Start WLAN Client");         // WiFi Mode Client

    WiFi.config(CL_local_ip, CL_gateway, CL_subnet, CL_gateway);
    delay(100);
    WiFi.begin(CL_ssid, CL_password);

    while (WiFi.status() != WL_CONNECTED  && wifi_retry < 20) {         // Check connection, try 10 seconds
      wifi_retry++;
      delay(500);
      Serial.print(".");
    }
  }

  if (WiFi.status() != WL_CONNECTED) {   // No client connection start AP
    // Init wifi connection
    Serial.println("Start WLAN AP");         // WiFi Mode AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_ssid, AP_password);
    delay(100);
    WiFi.softAPConfig(AP_local_ip, AP_gateway, AP_subnet);
    IPAddress IP = WiFi.softAPIP();
    Serial.println("");
    Serial.print("AP IP address: ");
    Serial.println(IP);
    wifiType = 1;

  } else {  // Wifi Client connection was sucessfull

    Serial.println("");
    Serial.println("WiFi client connected");
    Serial.println("IP client address: ");
    Serial.println(WiFi.localIP());
  }

  // Start OneWire
  //sensors.begin();

  // Start TCP server
  server.begin();

  // Start JSON server
  json.begin();


  // Start Web Server
  webserver.on("/", Ereignis_Index);
  webserver.on("/gauge.min.js", Ereignis_js);
  //webserver.on("/ADC.txt", Ereignis_ADC);
  webserver.onNotFound(handleNotFound);

  webserver.begin();
  Serial.println("HTTP server started");

  // Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega

  NMEA2000.SetN2kCANMsgBufSize(8);
  NMEA2000.SetN2kCANReceiveFrameBufSize(250);
  NMEA2000.SetN2kCANSendFrameBufSize(250);

  esp_efuse_mac_get_default(chipid);
  for (i = 0; i < 6; i++) id += (chipid[i] << (7 * i));

  // Set product information
  NMEA2000.SetProductInformation("1", // Manufacturer's Model serial code
                                 100, // Manufacturer's product code
                                 "NMEA 2000 WiFi Gateway",  // Manufacturer's Model ID
                                 "1.0.2.25 (2019-07-07)",  // Manufacturer's Software version code
                                 "1.0.2.0 (2019-07-07)" // Manufacturer's Model version
                                );
  // Set device information
  NMEA2000.SetDeviceInformation(id, // Unique number. Use e.g. Serial number. Id is generated from MAC-Address
                                130, // Device function=Analog to NMEA 2000 Gateway. See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                25, // Device class=Inter/Intranetwork Device. See codes on  http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                2046 // Just choosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
                               );

  // If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below

  NMEA2000.SetForwardType(tNMEA2000::fwdt_Text); // Show in clear text. Leave uncommented for default Actisense format.

  preferences.begin("nvs", false);                          // Open nonvolatile storage (nvs)
  NodeAddress = preferences.getInt("LastNodeAddress", 32);  // Read stored last NodeAddress, default 32
  preferences.end();

  Serial.printf("NodeAddress=%d\n", NodeAddress);

  NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, NodeAddress);

  NMEA2000.ExtendTransmitMessages(TransmitMessages);
  NMEA2000.ExtendReceiveMessages(ReceiveMessages);
  NMEA2000.AttachMsgHandler(&tN2kDataToNMEA0183); // NMEA 2000 -> NMEA 0183 conversion
  NMEA2000.SetMsgHandler(HandleNMEA2000Msg); // Also send all NMEA2000 messages in SeaSmart format

  tN2kDataToNMEA0183.SetSendNMEA0183MessageCallback(SendNMEA0183Message);

  NMEA2000.Open();

  delay(200);
}


//*****************************************************************************
void SendBufToClients(const char *buf) {
  for (auto it = clients.begin() ; it != clients.end(); it++) {
    if ( (*it) != NULL && (*it)->connected() ) {
      (*it)->println(buf);
    }
  }
}

#define MAX_NMEA2000_MESSAGE_SEASMART_SIZE 500
//*****************************************************************************
//NMEA 2000 message handler
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg) {


  if ( !SendSeaSmart ) return;

  char buf[MAX_NMEA2000_MESSAGE_SEASMART_SIZE];
  if ( N2kToSeasmart(N2kMsg, millis(), buf, MAX_NMEA2000_MESSAGE_SEASMART_SIZE) == 0 ) return;
  SendBufToClients(buf);
}


//*****************************************************************************
void SendNMEA0183Message(const tNMEA0183Msg &NMEA0183Msg) {
  if ( !SendNMEA0183Conversion ) return;

  char buf[MAX_NMEA0183_MESSAGE_SIZE];
  if ( !NMEA0183Msg.GetMessage(buf, MAX_NMEA0183_MESSAGE_SIZE) ) return;
  SendBufToClients(buf);
}


bool IsTimeToUpdate(unsigned long NextUpdate) {
  return (NextUpdate < millis());
}
unsigned long InitNextUpdate(unsigned long Period, unsigned long Offset = 0) {
  return millis() + Period + Offset;
}

void SetNextUpdate(unsigned long &NextUpdate, unsigned long Period) {
  while ( NextUpdate < millis() ) NextUpdate += Period;
}


void SendN2kEngine() {
  static unsigned long SlowDataUpdated = InitNextUpdate(SlowDataUpdatePeriod, MiscSendOffset);
  tN2kMsg N2kMsg;

  if ( IsTimeToUpdate(SlowDataUpdated) ) {
    SetNextUpdate(SlowDataUpdated, SlowDataUpdatePeriod);

//    SetN2kEngineDynamicParam(N2kMsg, 0, N2kDoubleNA, N2kDoubleNA, CToKelvin(temp), voltage, N2kDoubleNA, N2kDoubleNA, N2kDoubleNA, N2kDoubleNA, N2kInt8NA, N2kInt8NA, true);
//    NMEA2000.SendMsg(N2kMsg);
  }
}


//*****************************************************************************
void AddClient(WiFiClient &client) {
  Serial.println("New Client.");
  clients.push_back(tWiFiClientPtr(new WiFiClient(client)));
}

//*****************************************************************************
void StopClient(LinkedList<tWiFiClientPtr>::iterator &it) {
  Serial.println("Client Disconnected.");
  (*it)->stop();
  it = clients.erase(it);
}

//*****************************************************************************
void CheckConnections() {
  WiFiClient client = server.available();   // listen for incoming clients

  if ( client ) AddClient(client);

  for (auto it = clients.begin(); it != clients.end(); it++) {
    if ( (*it) != NULL ) {
      if ( !(*it)->connected() ) {
        StopClient(it);
      } else {
        if ( (*it)->available() ) {
          char c = (*it)->read();
          if ( c == 0x03 ) StopClient(it); // Close connection by ctrl-c
        }
      }
    } else {
      it = clients.erase(it); // Should have been erased by StopClient
    }
  }
}



void handle_json() {

  WiFiClient client = json.available();

  // Do we have a client?
  if (!client) return;

  // Serial.println(F("New client"));

  // Read the request (we ignore the content in this example)
  while (client.available()) client.read();

  // Allocate JsonBuffer
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<800> root;

  root["Latitude"] = BoatData.Latitude;
  root["Longitude"] = BoatData.Longitude;
  root["Heading"] = BoatData.Heading;
  root["COG"] = BoatData.COG;
  root["SOG"] = BoatData.SOG;
  root["STW"] = BoatData.STW;
  root["AWS"] = BoatData.AWS;
  root["TWS"] = BoatData.TWS;
  root["MaxAws"] = BoatData.MaxAws;
  root["MaxTws"] = BoatData.MaxTws;
  root["AWA"] = BoatData.AWA;
  root["TWA"] = BoatData.TWA;
  root["TWD"] = BoatData.TWD;
  root["TripLog"] = BoatData.TripLog;
  root["Log"] = BoatData.Log;
  root["RudderPosition"] = BoatData.RudderPosition;
  root["WaterTemperature"] = BoatData.WaterTemperature;
  root["WaterDepth"] = BoatData.WaterDepth;
  root["Variation"] = BoatData.Variation;
  root["Altitude"] = BoatData.Altitude;
  root["GPSTime"] = BoatData.GPSTime;
  root["DaysSince1970"] = BoatData.DaysSince1970;


  //Serial.print(F("Sending: "));
  //serializeJson(root, Serial);
  //Serial.println();

  // Write response headers
  client.println("HTTP/1.0 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  // Write JSON document
  serializeJsonPretty(root, client);

  // Disconnect
  client.stop();
}



void loop_wifi_ap() {
  unsigned int size;
  int wifi_retry;

  webserver.handleClient();
  handle_json();

  if (NMEA0183.GetMessage(NMEA0183Msg)) {  // Get AIS NMEA sentences from serial2

    SendNMEA0183Message(NMEA0183Msg);      // Send to TCP clients

    NMEA0183Msg.GetMessage(buff, MAX_NMEA0183_MESSAGE_SIZE); // send to buffer

#if ENABLE_DEBUG_LOG == 1
    Serial.println(buff);
#endif

#if UDP_Forwarding == 1
    size = strlen(buff);
    udp.beginPacket(udpAddress, udpPort);  // Send to UDP
    udp.write((byte*)buff, size);
    udp.endPacket();
#endif
  }

  SendN2kEngine();
  CheckConnections();
  NMEA2000.ParseMessages();

  int SourceAddress = NMEA2000.GetN2kSource();
  if (SourceAddress != NodeAddress) { // Save potentially changed Source Address to NVS memory
    NodeAddress = SourceAddress;      // Set new Node Address (to save only once)
    preferences.begin("nvs", false);
    preferences.putInt("LastNodeAddress", SourceAddress);
    preferences.end();
    Serial.printf("Address Change: New Address=%d\n", SourceAddress);
  }

  tN2kDataToNMEA0183.Update(&BoatData);

  // Dummy to empty input buffer to avoid board to stuck with e.g. NMEA Reader
  if ( Serial.available() ) {
    Serial.read();
  }

  // Alarm handling
  //button.tick();

#if ENABLE_DEBUG_LOG == 2
  Serial.print("Voltage:" ); Serial.println(voltage);
  //Serial.print("Temperature: ");Serial.println(temp);
  Serial.println("");
#endif

  if (wifiType == 0) {                                          // Check connection if working as client
    wifi_retry = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_retry < 5 ) {  // Connection lost, 5 tries to reconnect
      wifi_retry++;
      Serial.println("WiFi not connected. Try to reconnect");
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      WiFi.mode(WIFI_STA);
      WiFi.begin(CL_ssid, CL_password);
      delay(100);
    }
    if (wifi_retry >= 5) {
      Serial.println("\nReboot");                                  // Did not work -> restart ESP32
      ESP.restart();
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////


#define ESP32_RCSWITCH_PIN GPIO_NUM_15  // Set RCSWITCH port to 15 (RXB6 receiver)
#define KEY_DELAY 300  // 300 ms break between keys
#define BEEP_TIME 200  // 200 ms beep time

#define BUZZER_PIN 2  // Buzzer connected to GPIO 2

//int NodeAddress;  // To store last Node Address

//Preferences preferences;             // Nonvolatile storage on ESP32 - To store LastDeviceAddress

RCSwitch mySwitch = RCSwitch();

unsigned long key_time = 0;
unsigned long beep_time = 0;
bool beep_status = false;

/*
const unsigned long Key_Minus_1 = 13434472; // Change values to individual values programmed to remote control
const unsigned long Key_Plus_1 = 13434468;
const unsigned long Key_Minus_10 = 13434476;
const unsigned long Key_Plus_10 = 13434466;
const unsigned long Key_Tack_Portside = 13434474;
const unsigned long Key_Tack_Starboard = 13434470;
const unsigned long Key_Auto = 13434478;
const unsigned long Key_Wind = 13434465;
*/

const unsigned long Keys_Minus_1[] = { 13434472,12168808,5457512 };  
const unsigned long Keys_Plus_1[] = { 13434468,12168804,5457508 };
const unsigned long Keys_Minus_10[] = { 13434476,12168812,5457516 };
const unsigned long Keys_Plus_10[] = { 13434466,12168802,5457506 };
const unsigned long Keys_Tack_Portside[] = { 13434474,12168810,5457514 };
const unsigned long Keys_Tack_Starboard[] = { 13434470,12168806,5457510 };
const unsigned long Keys_Auto[] = { 13434478,12168814,5457518 };
const unsigned long Keys_Wind[] = { 13434465,12168801,5457505 };

/*
const unsigned long TransmitMessages[] PROGMEM = {126208UL,   // Set Pilot Mode
                                                  126720UL,   // Send Key Command
                                                  65288UL,    // Send Seatalk Alarm State
                                                  0
                                                 };

const unsigned long ReceiveMessages[] PROGMEM = { 127250UL,   // Read Heading
                                                  65288UL,    // Read Seatalk Alarm State
                                                  65379UL,    // Read Pilot Mode
                                                  0
                                                };
*/
tN2kDeviceList *pN2kDeviceList;
short pilotSourceAddress = -1;


void setup_evoPilot() {
  uint8_t chipid[6];
  uint32_t id = 0;
  int i = 0;
  
  WiFi.mode(WIFI_OFF);
  btStop();

  esp_efuse_mac_get_default(chipid);
  for (i = 0; i < 6; i++) id += (chipid[i] << (7 * i));

  // Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega
  NMEA2000.SetN2kCANReceiveFrameBufSize(150);
  NMEA2000.SetN2kCANMsgBufSize(8);
  // Set Product information
  NMEA2000.SetProductInformation("00000001", // Manufacturer's Model serial code
                                 100, // Manufacturer's product code
                                 "Evo Pilot Remote",  // Manufacturer's Model ID
                                 "1.0.0.0",  // Manufacturer's Software version code
                                 "1.0.0.0" // Manufacturer's Model version
                                );
  // Set device information
  NMEA2000.SetDeviceInformation(id, // Unique number. Use e.g. Serial number.
                                132, // Device function=Analog to NMEA 2000 Gateway. See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                25, // Device class=Inter/Intranetwork Device. See codes on  http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                2046 // Just choosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
                               );

  Serial.begin(115200);
  delay(100);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  mySwitch.enableReceive(ESP32_RCSWITCH_PIN);  // Receiver on GPIO15 on ESP32

  // Uncomment 3 rows below to see, what device will send to bus
  //NMEA2000.SetForwardStream(&Serial);  // PC output on due programming port
  //NMEA2000.SetForwardType(tNMEA2000::fwdt_Text); // Show in clear text. Leave uncommented for default Actisense format.
  //NMEA2000.SetForwardOwnMessages();

  preferences.begin("nvs", false);                          // Open nonvolatile storage (nvs)
  NodeAddress = preferences.getInt("LastNodeAddress", 34);  // Read stored last NodeAddress, default 34
  preferences.end();
  Serial.printf("NodeAddress=%d\n", NodeAddress);

  // If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below
  NMEA2000.SetMode(tNMEA2000::N2km_NodeOnly, NodeAddress); //N2km_NodeOnly N2km_ListenAndNode
  NMEA2000.ExtendTransmitMessages(TransmitMessages);
  NMEA2000.ExtendReceiveMessages(ReceiveMessages);

  NMEA2000.SetMsgHandler(RaymarinePilot::HandleNMEA2000Msg);

  pN2kDeviceList = new tN2kDeviceList(&NMEA2000);
  //NMEA2000.SetDebugMode(tNMEA2000::dm_ClearText); // Uncomment this, so you can test code without CAN bus chips on Arduino Mega
  NMEA2000.EnableForward(false); // Disable all msg forwarding to USB (=Serial)
  NMEA2000.Open();

  Serial.println((String) "NMEA2000 Open");
}


// Beep on if key received

void BeepOn() {
  if (beep_status == true) return;  // Already On

  digitalWrite(BUZZER_PIN, HIGH);
  beep_time = millis();
  beep_status = true;
}


// Beep off after BEEP_TIME

void BeepOff() {
  if (beep_status == true && millis() > beep_time + BEEP_TIME) {
    digitalWrite(BUZZER_PIN, LOW);
    beep_status = false;
  }
}


// Get device source address (of EV-1)

int getDeviceSourceAddress(String model) {
  if (!pN2kDeviceList->ReadResetIsListUpdated()) return -1;
  for (uint8_t i = 0; i < N2kMaxBusDevices; i++) {
    const tNMEA2000::tDevice *device = pN2kDeviceList->FindDeviceBySource(i);
    if ( device == 0 ) continue;

    String modelVersion = device->GetModelVersion();

    if (modelVersion.indexOf(model) >= 0) {
      return device->GetSource();
    }
  }
  return -2;
}


// Receive 433 MHz commands from remote and send SeatalkNG codes to EV-1 (if available)

void Handle_AP_Remote(void) {
  unsigned long key = 0;

  if (pilotSourceAddress < 0) pilotSourceAddress = getDeviceSourceAddress("EV-1"); // Try to get EV-1 Source Address

  if (mySwitch.available()) {
    key = mySwitch.getReceivedValue();
    mySwitch.resetAvailable();
  }

  if (key > 0 && millis() > key_time + KEY_DELAY) {
    key_time = millis();   // Remember time of last key received

    if (std::find(std::begin(Keys_Plus_1), std::end(Keys_Plus_1), key) != std::end(Keys_Plus_1)) {
      Serial.println("+1");
      BeepOn();
      if (pilotSourceAddress < 0) return; // No EV-1 detected. Return!
      tN2kMsg N2kMsg;
      RaymarinePilot::KeyCommand(N2kMsg, pilotSourceAddress, KEY_PLUS_1);
      NMEA2000.SendMsg(N2kMsg);
    }

    else if (std::find(std::begin(Keys_Plus_10), std::end(Keys_Plus_10), key) != std::end(Keys_Plus_10)) {
      Serial.println("+10");
      BeepOn();
      if (pilotSourceAddress < 0) return; // No EV-1 detected. Return!
      tN2kMsg N2kMsg;
      RaymarinePilot::KeyCommand(N2kMsg, pilotSourceAddress, KEY_PLUS_10);
      NMEA2000.SendMsg(N2kMsg);      
    }

    else if (std::find(std::begin(Keys_Minus_1), std::end(Keys_Minus_1), key) != std::end(Keys_Minus_1)){
      Serial.println("-1");
      BeepOn();
      if (pilotSourceAddress < 0) return; // No EV-1 detected. Return!
      tN2kMsg N2kMsg;
      RaymarinePilot::KeyCommand(N2kMsg, pilotSourceAddress, KEY_MINUS_1);
      NMEA2000.SendMsg(N2kMsg);      
    }

    else if (std::find(std::begin(Keys_Minus_10), std::end(Keys_Minus_10), key) != std::end(Keys_Minus_10)) {
      Serial.println("-10");
      BeepOn();
      if (pilotSourceAddress < 0) return; // No EV-1 detected. Return!
      tN2kMsg N2kMsg;
      RaymarinePilot::KeyCommand(N2kMsg, pilotSourceAddress, KEY_MINUS_10);
      NMEA2000.SendMsg(N2kMsg);      
    }

    else if (std::find(std::begin(Keys_Tack_Portside), std::end(Keys_Tack_Portside), key) != std::end(Keys_Tack_Portside)) {
      Serial.println("Tack Portside");
      BeepOn();
      if (pilotSourceAddress < 0) return; // No EV-1 detected. Return!
      tN2kMsg N2kMsg;
      RaymarinePilot::KeyCommand(N2kMsg, pilotSourceAddress, KEY_TACK_PORTSIDE);
      NMEA2000.SendMsg(N2kMsg);      
    }

    else if (std::find(std::begin(Keys_Tack_Starboard), std::end(Keys_Tack_Starboard), key) != std::end(Keys_Tack_Starboard)) {
      Serial.println("Tack Starboard");
      BeepOn();
      if (pilotSourceAddress < 0) return; // No EV-1 detected. Return!
      tN2kMsg N2kMsg;
      RaymarinePilot::KeyCommand(N2kMsg, pilotSourceAddress, KEY_TACK_STARBORD);
      NMEA2000.SendMsg(N2kMsg);      
    }

    else if (std::find(std::begin(Keys_Wind), std::end(Keys_Wind), key) != std::end(Keys_Wind)) {
      Serial.println("Setting PILOT_MODE_Wind");
      BeepOn();
      if (pilotSourceAddress < 0) return; // No EV-1 detected. Return!
      tN2kMsg N2kMsg;
      RaymarinePilot::SetEvoPilotMode(N2kMsg, pilotSourceAddress, PILOT_MODE_WIND);
      NMEA2000.SendMsg(N2kMsg);     
    }

    else if (std::find(std::begin(Keys_Auto), std::end(Keys_Auto), key) != std::end(Keys_Auto)) {
      Serial.println("Setting PILOT_MODE_AUTO");
      BeepOn();
      if (pilotSourceAddress < 0) return; // No EV-1 detected. Return!
      tN2kMsg N2kMsg;
      RaymarinePilot::SetEvoPilotMode(N2kMsg, pilotSourceAddress, PILOT_MODE_AUTO);
      NMEA2000.SendMsg(N2kMsg);      
    }
    else 
       Serial.printf("received unkown key: %d\n", key);
  }
  BeepOff();
}

void loop() {
  Handle_AP_Remote();
  loop_wifi_ap();
 /* NMEA2000.ParseMessages();

  int SourceAddress = NMEA2000.GetN2kSource();
  if (SourceAddress != NodeAddress) { // Save potentially changed Source Address to NVS memory
    NodeAddress = SourceAddress;      // Set new Node Address (to save only once)
    preferences.begin("nvs", false);
    preferences.putInt("LastNodeAddress", SourceAddress);
    preferences.end();
    Serial.printf("Address Change: New Address=%d\n", SourceAddress);
  }*/
}
