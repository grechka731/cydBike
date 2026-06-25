#pragma once
#include <stdint.h>

void setupI2CBus();
int scanI2C(uint8_t* out, int maxn);
