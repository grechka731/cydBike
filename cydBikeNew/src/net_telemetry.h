#pragma once
#include "config.h"

void        netBegin();
void        netUpdate();
bool        netStarted();
const char* netApSsid();
const char* netApIp();
int         netClientCount();
bool        netStaConnected();
const char* netStaIp();
