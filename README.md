# ESP32-Evo-Remote-Pilot-NMEA2000
This is a Remote Control for an EV-1 Auto Pilot via NMEA2000 (SeaTalk-NG).

It is based on the work of this project in GitHub: https://github.com/matztam/raymarine-evo-pilot-remote.
Many thanks for all the work to identify the NMEA 2000 codes for the EV-1 Course Computer.

The hardware consists of an ESP32 NodeMCU (here from AZ-Delivery), a CAN bus transceiver, a 433MHz receiver (RXB6) and a power converter.
Thats's all what is required to control an EV-1 based Auto Pilot with a simple 433 MHz remote control.

A standard 4 button 433 MHz KeyFob is used to send (+1 / -1 degree) and (+10 / -10 degrees) course changes to the autopilot (Auto/Standby is optional and requires two more buttons or an additional KeyFob). An accustical feedback for each received key is provided with a piezo buzzer tone. I tried to use the alarm function of the MFD (as within the Seatalk remote control). But so far I was not successful to send any alarm PGN that generates an alarm on the MFD.

![KeyFob](https://github.com/AK-Homberger/ESP32-Evo-Remote-Pilot-NMEA2000/blob/master/KeyFob.jpg)

![Schematics](https://github.com/AK-Homberger/ESP32-Evo-Remote-Pilot-NMEA2000/blob/master/ESP32EvoPilotSchematics.png)


The code is using the NMEA 2000 libraries from Timo Lappalainen (https://github.com/ttlappalainen). 

Download and install the following libraries from GitHub link above:

 - NMEA2000
 - NMEA2000_esp32
 
The Remote Control is using the RCswitch library (to be installed with the Arduiono IDE Library Manager).

For the ESP32 CAN bus, I use the "Waveshare SN65HVD230 Can Board" as transceiver. It works well with the ESP32 (GPIO pins 4 and 5). 
The 12 Volt is reduced to 5 Volt with a DC Step Down Converter (D24V10F5, https://www.pololu.com/product/2831).

GPIO 15 is used for the data from the RXB6 receiver. As antenna please use a short wire (e.g. 17,3 cm).
GPIO 2 is used to generate accustical feedback with a piezo buzzer.

For programming the KeyFob please use the sketch ESP32_433_Programming.ino (https://github.com/AK-Homberger/ESP32-Evo-Remote-Pilot-NMEA2000/tree/master/ESP32_433_Programming). The Transmitter has to be connected to GPIO 19.

It is possible to integrate the Remote Control code also in other ESP32 NMEA2000 projects (like the WiFi Gateway). But unfortunately the WLAN interfers with the 433 MHz receiver. Either RF part or interrupt for the RCSwitch). I noticed problems with the correct recognition of keys from the remote. As a result, I would suggest to avoid the integration in any other project with WLAN.

# Partlist:


- RXB6 433 Mhz receiver [Link](https://www.makershop.de/en/module/funk/rxb6-433mhz-antenne/)
- ESP32 [Link](https://www.makershop.de/en/plattformen/arduino/pro-micro-mini-usb/) 
- Buzzer  [Link](https://www.makershop.de/en/module/audio/aktiver-summer/)

For programming:
433MHz transmitter [Link](https://www.makershop.de/en/module/funk/superheterodyne-433-mhz-sender/) or [Link2](https://www.makershop.de/en/module/funk/433-sender-empfaenger/)

Remote Control (KeyFob):  [Link](https://www.amazon.de/XCSOURCE-Elektrische-Universal-Garagentor-Fernbedienung-Ersatz-Schl%C3%BCsselanh%C3%A4nger-HS641/dp/B01KJRGMHI/ref=sr_1_9?__mk_de_DE=%C3%85M%C3%85%C5%BD%C3%95%C3%91&dchild=1&keywords=433+mhz+fernbedienung&qid=1601302510&sr=8-9) or [Link2](https://www.amazon.de/EasyULT-Handsender-Fernbedienung-Automatische-Garagentor%C3%B6ffner/dp/B0829WXGR5/ref=sr_1_15?__mk_de_DE=%C3%85M%C3%85%C5%BD%C3%95%C3%91&dchild=1&keywords=433+mhz+fernbedienung&qid=1601307082&sr=8-15)


# Updates
- Version 0.5, 25.09.2020: Changed BeepOn() in code to generate beep even if no EV-1 device is found (to alow testing). 

- Version 0.4, 04.08.2020: Changed TX pin from 2 to 5. Addedd buzzer to pin 2. Store/restore last used NodeAddress.

- Version 0.2, 26.07.2020: Avoid using delay(), added SetN2kPGN65361 (Seatalk Alarm State) to allow alarm beep for key received feedback.

- Version 0.1, 24.07.2020: Initial version. 
