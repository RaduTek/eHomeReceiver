// Edge timestamping for a demodulating IR receiver (TSOP382xx, VS1838...).
// Timer1 runs at 2 MHz (0.5 us ticks), extended to 32 bits by an overflow
// counter. The input backend is selected in Config.h.
#pragma once

#include <Arduino.h>

#define IR_TICKS_PER_US 2

struct IrEdge {
  uint32_t time;  // Timer1 ticks
  bool pulse;     // level after this edge: true = IR carrier present
};

namespace IrCapture {
void begin();

// Pops the oldest edge. Returns false if none is pending.
bool pop(IrEdge &edge);

// Current time in Timer1 ticks.
uint32_t now();

// Edges lost because the buffer was full (debug counter).
uint16_t overruns();
}
