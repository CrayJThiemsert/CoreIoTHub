/*
  Jirachai Thiemsert & Rui Santos
  Complete project details at https://RandomNerdTutorials.com/?s=esp-now
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
  Based on JC Servaye example: https://github.com/Servayejc/esp_now_web_server/
*/
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "AsyncTCP.h"
#include <ArduinoJson.h>

#define MAX_BOARDS 6

// Replace with your network credentials (STATION)
const char* ssid = "Killing Me Softly 2G";
const char* password = "14mP455w0rd";

esp_now_peer_info_t slave;
int chan; 

enum MessageType {PAIRING, DATA,};
MessageType messageType;

int counter = 0;

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
  uint8_t msgType;
  uint8_t id;
  float temp;
  float hum;
  unsigned int readingId;
} struct_message;

typedef struct struct_pairing {       // new structure for pairing
    uint8_t msgType;
    uint8_t id;
    uint8_t macAddr[6];
    uint8_t channel;
} struct_pairing;

struct_message incomingReadings;
struct_message outgoingSetpoints;
struct_pairing pairingData;

// Declare array of board struct
typedef struct struct_board {       // new structure for board mac addresses
    uint8_t macAddr[6];
};
const int numBoards = MAX_BOARDS;
struct_board array_boards[numBoards];

AsyncWebServer server(80);
AsyncEventSource events("/events");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP-NOW DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {  font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #2f4468; color: white; font-size: 1.7rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .reading { font-size: 2.8rem; }
    .packet { color: #bebebe; }
    .card.temperature { color: #fd7e14; }
    .card.humidity { color: #1b78e2; }
  </style>
</head>
<body>
  <div class="topnav">
    <h3>ESP-NOW DASHBOARD</h3>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> BOARD #1 - TEMPERATURE</h4><p><span class="reading"><span id="t1"></span> &deg;C</span></p><p class="packet">Reading ID: <span id="rt1"></span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> BOARD #1 - HUMIDITY</h4><p><span class="reading"><span id="h1"></span> &percnt;</span></p><p class="packet">Reading ID: <span id="rh1"></span></p>
      </div>

      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> BOARD #2 - TEMPERATURE</h4><p><span class="reading"><span id="t2"></span> &deg;C</span></p><p class="packet">Reading ID: <span id="rt2"></span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> BOARD #2 - HUMIDITY</h4><p><span class="reading"><span id="h2"></span> &percnt;</span></p><p class="packet">Reading ID: <span id="rh2"></span></p>
      </div>
    </div>
  </div>
  <div id="container_debug" class="content">
    Hello debug
  </div>
  <div id="container_board" class="content">
    Hello div
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('new_readings', function(e) {
  console.log("new_readings", e.data);
  const container = document.getElementById('container_debug');

  var obj = JSON.parse(e.data);

  container.innerHTML = JSON.stringify(obj, null, 2);

  document.getElementById("t"+obj.id).innerHTML = obj.temperature.toFixed(2);
  document.getElementById("h"+obj.id).innerHTML = obj.humidity.toFixed(2);
  document.getElementById("rt"+obj.id).innerHTML = obj.readingId;
  document.getElementById("rh"+obj.id).innerHTML = obj.readingId;
 }, false);

 source.addEventListener('draw_boards', function(e) {
  console.log("draw_boards", e.data);
  const container = document.getElementById('container_board');
  container.innerHTML = ''; // Clear existing content

  var obj = JSON.parse(e.data);

  container.innerHTML = JSON.stringify(obj, null, 2);

  obj.forEach((item, index) => {
      const divId = `div${index + 1}`;
      const div = document.createElement('div');
      div.id = divId;
      div.className = 'data-div'; // Add any desired CSS class

      div.innerHTML = `
          <h2>${item.readingId}</h2>
          <p>${item.temperature}</p>
          <!-- Add more content as needed -->
      `;

      container.appendChild(div);
  });
 }, false);
}
</script>
</body>
</html>)rawliteral";

bool verifyBoardsList(const uint8_t * mac_addr) {
  bool result = false;

  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println();
  Serial.print("verifyBoardsList - macStr=");         
  Serial.println(macStr);

  int newMacAddrIndex = 0;

  for(int i=0; i < numBoards; i++) {
    char macBoardStr[18];
    snprintf(macBoardStr, sizeof(macBoardStr), "%02x:%02x:%02x:%02x:%02x:%02x",
            array_boards[i].macAddr[0], array_boards[i].macAddr[1], array_boards[i].macAddr[2], array_boards[i].macAddr[3], array_boards[i].macAddr[4], array_boards[i].macAddr[5]);
    Serial.print("verifyBoardsList - macBoardStr=");         
    Serial.println(macBoardStr);

    if (strcmp(macBoardStr, macStr) == 0) {
      result = true;
      return result;
    } else {
      if (strcmp(macBoardStr, "00:00:00:00:00:00") == 0) {
        // if mac addres is not existing,, add this mac address into board array.
        for (int j = 0; j < 6; j++) {
          array_boards[i].macAddr[j] = mac_addr[j];
        }
        
        result = true;
        return result;
      }
    }

    
  }

  return result;
}

void readDataToSend() {
  outgoingSetpoints.msgType = DATA;
  outgoingSetpoints.id = 1;
  outgoingSetpoints.temp = random(0, 40);
  outgoingSetpoints.hum = random(0, 100);
  outgoingSetpoints.readingId = counter++;

  StaticJsonDocument<1000> root;
  String payload;
  
  // create a JSON document with received data and send it by event to the web page
  root["id"] = outgoingSetpoints.id;
  root["temperature"] = outgoingSetpoints.temp;
  root["humidity"] = outgoingSetpoints.hum;
  root["readingId"] = String(outgoingSetpoints.readingId);
  serializeJson(root, payload);
  Serial.print("event send :");
  serializeJson(root, Serial);
  events.send(payload.c_str(), "new_readings", millis());
  Serial.println();
}

void printOutgoingSendings(){
  // Display Sendings in Serial Monitor
  Serial.println();
  Serial.println("OUT GOING SENDINGS");
  Serial.print("Temperature: ");
  Serial.print(outgoingSetpoints.temp);
  Serial.println(" ºC");
  Serial.print("Humidity: ");
  Serial.print(outgoingSetpoints.hum);
  Serial.println(" %");
  Serial.print("Led: ");
  Serial.println(outgoingSetpoints.readingId);
  Serial.println("==>");
}

void printIncomingReadings(){
  // Display Readings in Serial Monitor
  Serial.println();
  Serial.println("<==INCOMING READINGS");
  Serial.print("Temperature: ");
  Serial.print(incomingReadings.temp);
  Serial.println(" ºC");
  Serial.print("Humidity: ");
  Serial.print(incomingReadings.hum);
  Serial.println(" %");
  Serial.print("Led: ");
  Serial.println(incomingReadings.readingId);
}

// ---------------------------- esp_ now -------------------------
void printMAC(const uint8_t * mac_addr){
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
}

bool addPeer(const uint8_t *peer_addr) {      // add pairing
  memset(&slave, 0, sizeof(slave));
  const esp_now_peer_info_t *peer = &slave;
  memcpy(slave.peer_addr, peer_addr, 6);
  
  slave.channel = chan; // pick a channel
  slave.encrypt = 0; // no encryption
  // check if the peer exists
  bool exists = esp_now_is_peer_exist(slave.peer_addr);
  if (exists) {
    // Slave already paired.
    Serial.println("Already Paired");
    return true;
  }
  else {
    esp_err_t addStatus = esp_now_add_peer(peer);
    if (addStatus == ESP_OK) {
      // Pair success
      Serial.println("Pair success");
      return true;
    }
    else 
    {
      Serial.println("Pair failed");
      return false;
    }
  }
} 

void updateBoardsInfoPage()  {
  const size_t capacity = JSON_ARRAY_SIZE(6) + JSON_ARRAY_SIZE(4) + 60;
  StaticJsonDocument<capacity> board_data;

  // Sample data format: [ { "readingId": 1, "temperature": 25 }, { "readingId": 2, "temperature": 30 } ]
  JsonArray outerArray = board_data.to<JsonArray>();
  // Create a nested JSON object using createNestedObject()
  JsonObject innerObject1 = outerArray.createNestedObject();
  innerObject1["id"] = incomingReadings.id;
  innerObject1["readingId"] = String(incomingReadings.readingId);
  innerObject1["humidity"] = incomingReadings.hum;
  innerObject1["temperature"] = incomingReadings.temp;

  // JsonObject innerObject2 = outerArray.createNestedObject();
  // innerObject2["readingId"] = 2;
  // innerObject2["temperature"] = 30;

  // JsonArray outerArray = board_data.to<JsonArray>();

  // // Array 1
  // JsonArray innerArray1 = outerArray.createNestedArray();
  // innerArray1.add("Apple");
  // innerArray1.add("Banana");
  // innerArray1.add("Cherry");

  // // Array 2
  // JsonArray innerArray2 = outerArray.createNestedArray();
  // innerArray2.add("Lemon");
  // innerArray2.add("Mango");

  String boardlist_payload;
  serializeJson(board_data, boardlist_payload);
  events.send(boardlist_payload.c_str(), "draw_boards", millis());
  Serial.println();
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  Serial.print(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success to " : "Delivery Fail to ");
  printMAC(mac_addr);
  Serial.println();
}

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) { 
  Serial.print(len);
  Serial.print(" bytes of data received from : ");
  printMAC(mac_addr);
  Serial.println();

  StaticJsonDocument<1000> root;
  String payload;

  uint8_t type = incomingData[0];       // first message byte is the type of message 
  switch (type) {
  case DATA :                           // the message is data type
    // Update board mac address lists
    verifyBoardsList(mac_addr);

    memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
    // create a JSON document with received data and send it by event to the web page
    root["id"] = incomingReadings.id;
    root["temperature"] = incomingReadings.temp;
    root["humidity"] = incomingReadings.hum;
    root["readingId"] = String(incomingReadings.readingId);
    serializeJson(root, payload);
    Serial.print("event send :");
    serializeJson(root, Serial);
    events.send(payload.c_str(), "new_readings", millis());
    Serial.println();

    // new dynamic each boards data
    // board_data["title"] = incomingReadings.id;
    // board_data["description"] = incomingReadings.temp;
    // board = board_data;
    // serializeJson(board, boardlist_payload);
    // events.send(boardlist_payload.c_str(), "draw_boards", millis());
    // Serial.println();
    updateBoardsInfoPage();

    printIncomingReadings();
    break;
  
  case PAIRING:                            // the message is a pairing request 
    memcpy(&pairingData, incomingData, sizeof(pairingData));
    Serial.println(pairingData.msgType);
    Serial.println(pairingData.id);
    Serial.print("Pairing request from: ");
    printMAC(mac_addr);
    Serial.println();
    Serial.println(pairingData.channel);
    if (pairingData.id > 0) {     // do not replay to server itself
      if (pairingData.msgType == PAIRING) { 
        pairingData.id = 0;       // 0 is server

        // Server is in AP_STA mode: peers need to send data to server soft AP MAC address 
        WiFi.softAPmacAddress(pairingData.macAddr);   
        pairingData.channel = chan;
        Serial.println("send response");
        esp_err_t result = esp_now_send(mac_addr, (uint8_t *) &pairingData, sizeof(pairingData));
        addPeer(mac_addr);
      }  
    }  
    break; 
  }
}

void initESP_NOW(){
    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
      Serial.println("Error initializing ESP-NOW");
      return;
    }
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
} 

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial && millis() < 5000);

  Serial.println();
  Serial.println("Core IoT Hub - Serial Ready!!");

  Serial.println();
  Serial.print("Server MAC Address:  ");
  Serial.println(WiFi.macAddress());

  uint8_t macAddress[6];
  // Use sscanf to parse the MAC address string
  if (sscanf(WiFi.macAddress().c_str(), "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
              &macAddress[0], &macAddress[1], &macAddress[2],
              &macAddress[3], &macAddress[4], &macAddress[5]) != 6) {
      fprintf(stderr, "Invalid MAC address format\n");
      // return;
  }

  // Print the converted MAC address
  printf("MAC Address: ");
  for (int i = 0; i < 6; i++) {
      printf("%02X", macAddress[i]);
      if (i < 5) {
          printf(":");
      }
  }
  printf("\n");

  // add hub board mac address into board array list
  verifyBoardsList(macAddress);

  Serial.println();

  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_AP_STA);
  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }

  Serial.print("Server SOFT AP MAC Address:  ");
  Serial.println(WiFi.softAPmacAddress());

  chan = WiFi.channel();
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  initESP_NOW();
  
  // Start Web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  

  // Events 
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  
  // start server
  server.begin();

}

void loop() {
  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 10000;
  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
    Serial.println();

    int arraySize = sizeof(array_boards) / sizeof(struct_board);
    Serial.print("Number of elements in array_boards array: ");
    Serial.println(arraySize);

    events.send("ping",NULL,millis());
    lastEventTime = millis();
    readDataToSend();
    printOutgoingSendings();
    esp_now_send(NULL, (uint8_t *) &outgoingSetpoints, sizeof(outgoingSetpoints));
  }
}