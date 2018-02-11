HomeSense Basic OTA Firmware
=============================

Reads temperature, humidity, and barometric pressure, and reports the
readings to an MQTT broker.

Includes support for OTA updates from the Arduino IDE.

All configuration is done in the file "config.h". Edit that file with
your own WiFi and MQTT broker details, plus any offsets required for
sensor calibration.

The device automatically generates a unique MQTT client ID based on
the chip ID of the onboard ESP8266, and publishes to topics based on
that ID.

The startup sequence is:

1. Connect to WiFi using the values set in config.h.

2. Connect to your specified MQTT broker.

3. Publish a starting message to the topic "events" of the form:  
  "Device esp-XXXXXX starting up"  
where "XXXXXX" is the unique ID of the device.

4. Periodically publish sensor readings to topics of the form:  
  device/XXXXXX/temperature (Degrees C from DHT12)  
  device/XXXXXX/humidity (% relative humidity from DHT12)  
  device/XXXXXX/pressure (millibars of pressure, ie: kPa x 10, from MS5637-02BA03)  
  device/XXXXXX/temperature2 (Degrees C from MS5637-02BA03)

The device also subscribes to a topic of the form:  
  device/XXXXXX/command  
so that it can process commands sent to it. However, the current
version doesn't currently act on any commands so it simply echos them
to the serial console for debugging.

# License
Released under the terms of the GNU General Public License, v3.
