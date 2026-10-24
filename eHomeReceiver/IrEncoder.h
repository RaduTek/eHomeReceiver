// Converts captured IR edges into MCE IR data packets.
#pragma once

#include <Arduino.h>

// Receives one complete MCE packet (header + samples, at most 31 bytes).
typedef void (*IrPacketSink)(const uint8_t *packet, uint8_t len);

namespace IrEncoder {
void begin(IrPacketSink sink);

// Call from loop(): drains captured edges, flushes packets, detects the end
// of a burst using the receive timeout (in 50 us units).
void poll(uint16_t timeoutUnits);
}
