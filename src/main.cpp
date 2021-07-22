//
// Example code for https requests with a certificate store and to update the certificate store
// 
// To genereate the certificate store use this python script:
// https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/BearSSL_CertStore/certs-from-mozilla.py
//
// You can also find a version that only adds some Certificate Stores to save storage called custom-certs-from-mozilla.py
// 
// Make sure that it is uploaded, otherwize you won't be able to make requests
// If you are using PlatformIO you can see how here: https://randomnerdtutorials.com/esp8266-nodemcu-vs-code-platformio-littlefs/
// If you are using the Arduino IDE you can see how here: https://randomnerdtutorials.com/install-esp8266-nodemcu-littlefs-arduino/
//
// Note: I have only used the PlatformIO method and I'm not sure if the Arduino IDE will work.
// 
// Most of the Certificate Store code is from:
// https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/BearSSL_CertStore/BearSSL_CertStore.ino
// 
// Code by Federico Martinez (@mtz_federico). Released to the public domain
//

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <CertStoreBearSSL.h>
#include <time.h>
#include <LittleFS.h>

const char *ssid = "<SSID-GOES-HERE>";
const char *pass = "<PASSWORD-GOES-HERE>";

// A single, global CertStore which can be used by all
// connections.  Needs to stay live the entire time any of
// the WiFiClientBearSSLs are present.
BearSSL::CertStore certStore;

// Set time via NTP, as required for x.509 validation
void setClock() {
  // configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t timenNow = time(nullptr);
  while (timenNow < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    timenNow = time(nullptr);
  }
  /*
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&timenNow, &timeinfo);
  Serial.print("Current time: ");
  Serial.println(asctime(&timeinfo));
  */
  Serial.print("UNIX epoch: ");
  Serial.println(String(timenNow));
}

// Try and connect using a WiFiClientBearSSL to specified host:port and dump URL
void fetchURL(const char *host, const char *path, const uint16_t port = 443) {
  BearSSL::WiFiClientSecure *client = new BearSSL::WiFiClientSecure();
  // Integrate the cert store with this connection
  client->setCertStore(&certStore);

  Serial.printf("Trying: %s:%i...", host, port);
  client->connect(host, port);
  if (!client->connected()) {
    Serial.println("Error connecting to server");
    return;
  }

  /*
  String dataToSend = "test=true";
  String msgLength = String(dataToSend.length());
  */

  Serial.println("Connected!");
  client->print("GET ");
  client->print(path);
  client->print(" HTTP/1.0\r\nHost: ");
  client->print(host);
  client->print("\r\nUser-Agent: ESP8266\r\n");
  /*client->print("Content-Type: application/x-www-form-urlencoded\r\n");
  client->print("Content-Length: ");
  client->print(msgLength);
  client->print("\r\n");
  client->print("\r\n");
  client->print(dataToSend);*/
  client->print("\r\n");

  String response = client->readString(); //  Until('\n');
  int bodyStartIndex =  response.indexOf("\r\n\r\n") + 4;
  String subSTR = response.substring(bodyStartIndex);

  Serial.println("----- Response Starts -----");
  Serial.println(response);
  Serial.println("----- Response Ends -----");

  Serial.println("----- Payload Starts -----");
  Serial.println(subSTR);
  Serial.println("----- Payload Ends -----");

  client->stop();

  delete client;
}

void listFiles() {
  Serial.println("Listing files:");
  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    Serial.print(dir.fileName());
    Serial.print(" - ");
    Serial.println(dir.fileSize());
  }
}

// Downloads the certificate store and replaces the one in LittleFS
void updateCertStore() {
  // Code to download to LittleFS from https://github.com/esp8266/Arduino/issues/5175
  const String host = "example.com";
  const uint16_t port = 443;
  const String path = "/certs.ar";
  String newCertStoreDir = "/newCerts.ar";

  int contentLength = -1;
  int httpCode;
  String currentETag; // = "60d00407-2e"; // set the ETag here to only update if it is different
  String NewFileETag;

  BearSSL::WiFiClientSecure client;
  client.setBufferSizes(1024, 256);
  client.setCertStore(&certStore);

  Serial.print("[updateCertStore] Connecting to ");
  Serial.println(host);

  // Connect
  client.connect(host, port);

  // If not connected, return
  if (!client.connected()) {
    client.stop();
    Serial.println("[updateCertStore] Error connecting to server");
    return;
  }

  // HTTP GET
  client.print("GET ");
  client.print(path);
  client.print(" HTTP/1.0\r\nHost: ");
  client.print(host);
  client.print("\r\nUser-Agent: ESP8266\r\n");
  if (currentETag != "" && currentETag != NULL) {
    client.print("If-None-Match: ");
    client.print('"');
    client.print(currentETag);
    client.print('"');
    client.print("\r\n");
  }
  client.print("\r\n");

  // Handle headers
  while (client.connected()) {
    String header = client.readStringUntil('\n');
    if (header.startsWith(F("HTTP"))) {
      httpCode = header.substring(9, 13).toInt();
      // Serial.printf("Status code is: %i\n", httpCode);
      if (httpCode != 200) { // Only download the file if the status is 200
        // If the status is 304, there is no file change.
        Serial.println(String("[updateCertStore] Stopping connection. Status code is: ") + String(httpCode));
        client.stop();
        Serial.println(header);
        return;
      }
    }
    if (header.startsWith(F("Content-Length: "))) {
      contentLength = header.substring(15).toInt();
    }

    if (header.startsWith(F("ETag: "))) {
      int ETagEndIndex = header.indexOf("\r") -1;
      NewFileETag = header.substring(7, ETagEndIndex);
      Serial.print("NewFileETag is: ");
      Serial.println(NewFileETag);
    }

    if (header == F("\r")) {
      break;
    }
  }

  if (!(contentLength > 0)) {
    Serial.println(F("[updateCertStore] HTTP content length is 0"));
    client.stop();
    return;
  }

  // Open file for write
  fs::File f = LittleFS.open(newCertStoreDir, "w+");
  if (!f) {
    Serial.println( F("[updateCertStore] file open failed"));
    client.stop();
    return;
  }

  // Download file
  int remaining = contentLength;
  int received;
  uint8_t buff[512] = { 0 };

  // read all data from server
  while (client.available() && remaining > 0) {
    // read up to buffer size
    received = client.readBytes(buff, ((remaining > sizeof(buff)) ? sizeof(buff) : remaining));

    // write it to file
    f.write(buff, received);

    if (remaining > 0) {
      remaining -= received;
    }
    yield(); // https://forum.arduino.cc/t/trying-to-understand-yield/432160/4
  }

  if (remaining != 0)
    Serial.println("[updateCertStore] File truncated - " + String(remaining));

  // Close LittleFS file
  f.close();

  // Stop client
  client.stop();

  // delete the old cert store
  bool success = LittleFS.remove("/certs.ar");

  if (!success) {
    Serial.println("[updateCertStore] Error deleting the previous cert store");
    return;
  }

  // rename the new cert store file
  success = LittleFS.rename(newCertStoreDir, "/certs.ar");

  if (success) {
    Serial.println("[updateCertStore] Success updating the cert store");
  }

  return;
}


void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  if (!LittleFS.begin()) {
    Serial.println("Error starting LittlFS");
  }

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Mac Address: ");
  Serial.println(WiFi.macAddress());

  setClock(); // Required for X.509 validation

  int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.printf("Number of CA certs read: %d\n", numCerts);
  if (numCerts == 0) {
    Serial.printf("No certs found. Did you run certs-from-mozilla.py and upload the LittleFS directory before running?\n");
    Serial.printf("You can find it at:\nhttps://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/BearSSL_CertStore/certs-from-mozilla.py");
    listFiles();
    return; // Can't connect to anything w/o certs!
  }

  // fetchURL("example.com", "/path", 443);

  // listFiles();

  // updateCertStore();

  time_t timenNow = time(nullptr);

  Serial.print("UNIX epoch: ");
  Serial.println(timenNow);
}

void loop() {
  
}
