#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;
extern volatile bool learningMode;
extern unsigned long learningModeStartTime;

void setupWebServer();

#endif // WEB_SERVER_H
