#ifndef CGI_H
#define CGI_H

#include "httpd.h"

int cgiLed(HttpdConnData *connData);
void tplLed(HttpdConnData *connData, char *token, void **arg);
int cgiReadFlash(HttpdConnData *connData);
void tplCounter(HttpdConnData *connData, char *token, void **arg);
int cgiProgram(HttpdConnData *connData);
void tplProgramming(HttpdConnData *connData, char *token, void **arg);

int cgiCmuCam4color(HttpdConnData *connData);
int cgiCmuCam4bw(HttpdConnData *connData);
int cgiCmuCam4tw(HttpdConnData *connData);
int cgiCmuCam4tc(HttpdConnData *connData);

#endif