# CoreIoTHub
2023-08-08 11:51:11
IoT - CoreIoTHub - ESP32:
- 2 Ways ESP-Now Hub with auto reconnect after restart board.
- Receive ESP-Now message from CoreIoTSensor Project.
- Read random dummy data from the sensor client node(ESP8266). 
- Able to show dashboard on local browser. 
ie.
http://192.168.1.167/


GitHub:
https://github.com/CrayJThiemsert/CoreIoTHub.git

Local Loction: legion5pro
C:\Users\crayj\OneDrive\Documents\PlatformIO\Projects\CoreIoTHub

2023-08-05 11:53:17
IoT ESP-Now auto pairing:

Reference:
ESP-NOW: Auto-pairing for ESP32/ESP8266 with Bidirectional Communication and Web Server:
https://randomnerdtutorials.com/esp-now-auto-pairing-esp32-esp8266/

Hub:
Spec:
- ESP32 Board = ESP32 is high performance than ESP8266

Objective:
- Gather info from many devices.
- Easy to setup and pair to the client devices.

Client Devices:
Spec:
- ESP8266 Board = It just use ESP-Now to send out the data to the Hub only.

Objective:
- Send data to hub.
- Easy to setup and pair to the hub.

==>
How to pairing:
1) Turn on Hub
  > Provide its mac address to let the client devices know. By naming SSID.
  Naming convensions:
  Hub-[Mac Address]
  ie.
  Hub-E0:5A:1B:A1:E4:DC

  > Set WIFI STA mode

2) Turn on Client Devices
3) Paring
4) Start working process