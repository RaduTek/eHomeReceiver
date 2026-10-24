// Host -> device command stream parser and response generator.
#pragma once

#include <Arduino.h>

// Sends one response (at most 8 bytes) to the host.
typedef void (*MceResponseSink)(const uint8_t *data, uint8_t len);

namespace MceCommands {
void begin(MceResponseSink sink);

// Feeds bytes received on the OUT endpoint. Commands may span transfers.
void feed(const uint8_t *data, uint8_t len);

// Receive timeout set by the host, in 50 us units.
uint16_t timeoutUnits();
}
