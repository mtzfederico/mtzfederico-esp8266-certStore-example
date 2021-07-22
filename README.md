# mtzfederico-esp8266-certStore-example

This is an example code for the ESP8266 that shows how to make https requests using a certificate store and how to update that certificate store remotely.

# To use this code:

## clone it
```
git clone https://github.com/mtzfederico/mtzfederico-esp8266-certStore-example
```
## 2. Generate the Certificate Store
You can generate the certificate store with [this](https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/BearSSL_CertStore/certs-from-mozilla.py) python program or with my custom one ```custom-certs-from-mozilla.py```, which can be find in this repo.

## 3. Upload it to the ESP
I have only tested it using [PlatformIO](). You can find LittleFS upload instructions for PlatformIO [here](https://randomnerdtutorials.com/esp8266-nodemcu-vs-code-platformio-littlefs/). If you want to use the Arduino IDE, you can find instructions [here](https://randomnerdtutorials.com/install-esp8266-nodemcu-littlefs-arduino/)