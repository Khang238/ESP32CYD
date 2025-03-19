#ifndef DEAUTH_H
#define DEAUTH_H

#include <Arduino.h>

void start_deauth(int wifi_number, int attack_type, uint16_t reason, bool isClone = false);
void stop_deauth();
char* randString();

bool mayOutput = false;
String brige = "";

extern int eliminated_stations;
extern int deauth_type;

#endif