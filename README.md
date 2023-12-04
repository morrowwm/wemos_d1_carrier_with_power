# wemos_d1_carrier_with_power
Wemos D1 Mini carrier board with power DC volt output and I2C interface

This is a simple board takes input up to ~30 VDC and powers a [Wemos D1 Mini](https://universal-solder.ca/product/wemos-d1-mini-esp8266-wifi-iot-module/).
The Wemos D1 controls a MOSFET which switches the input voltage, and has I2C broken out to a 4 pin header. 

Useful for anything which senses the environment and controls something that requires more power of voltage than the D1's ESP8266 can provide. I used it to control a dough proofing oven.

![dough proofer](dough_proofing_controller.jpeg)

A dough proofing oven controller. Temperature and humidity sensed using an AOSONG AHT25. The MOSFET controls some silicon heating pads.

# Instructions to build
1. In both the Visual Code dough proofer include directory and the Arduino IDE heatbox source folder, there is a secrets.h file. Edit it to set WLAN_SSID1 and WLAN_PASS1 to match your wi-fi credentials. If you are using MQTT, you might want to adjust the three #defines just below to suit your setup. Also the mqtt_server variable further down.
2. What libraries you require depend on the sensors you attach.
3. On the hardware side, the 4-pin header marked J4 has D1, D2, 3V3  and GND connections, moving left to right with the edge closest to J4 facing you.

# Use
1. Before developing with the Wemos D1 connected to your computer via USB, make sure the jumper header marked J2 is __disconnected__. This will ensure you can't power the device from both the external power jack and your computer. Doing so will destroy at least the voltage regulator and possibly the Wemos D1 and your computer.
2. The code contains the ability to do OTA programming. 
