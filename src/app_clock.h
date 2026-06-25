#pragma once
#include <stdint.h>

void     clockSetEpoch(uint32_t epochUtc);
bool     clockHasTime();
uint32_t clockNowEpochUtc();

void     clockFormatTime(char* out, int n);
void     clockFormatTimeSec(char* out, int n);
void     clockFormatDate(char* out, int n);
