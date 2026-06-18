#pragma once

// Keep the stock Heltec V3 display, battery, and SX1262 configuration.
#include "../heltec_v3/variant.h"

// Two cascaded 74HC165 shift registers.
#define INPUTBROKER_SERIAL_TYPE 1
#define KB_LOAD 33
#define KB_CLK 47
#define KB_DATA 34
