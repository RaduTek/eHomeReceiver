#include "IrEncoder.h"
#include "Config.h"
#include "IrCapture.h"
#include "MceProtocol.h"

#define TICKS_PER_UNIT (MCE_TIME_UNIT_US * IR_TICKS_PER_US)

static IrPacketSink sink;

// Packet under construction: [0] = header, [1..] = samples
static uint8_t packet[1 + MCE_MAX_SAMPLES];
static uint8_t sampleCount;
static uint32_t packetStart;

// Burst state
static bool idle = true;
static bool level;          // level since lastEdge (true = pulse)
static uint32_t lastEdge;

#if MCE_DEBUG_IR
static void debugSample(bool pulse, uint32_t ticks) {
  Serial.print(pulse ? '+' : '-');
  Serial.print(ticks / IR_TICKS_PER_US);
  Serial.print(' ');
}
#else
#define debugSample(pulse, ticks)
#endif

static void flush() {
  if (!sampleCount)
    return;
  packet[0] = MCE_IRDATA | sampleCount;
  sink(packet, 1 + sampleCount);
  sampleCount = 0;
}

static void pushByte(uint8_t b) {
  if (!sampleCount)
    packetStart = IrCapture::now();
  packet[1 + sampleCount++] = b;
  if (sampleCount == MCE_MAX_SAMPLES)
    flush();
}

static void emitSample(bool pulse, uint32_t ticks) {
  debugSample(pulse, ticks);
  uint32_t units = (ticks + TICKS_PER_UNIT / 2) / TICKS_PER_UNIT;
  uint8_t pulseBit = pulse ? MCE_PULSE_BIT : 0;
  while (units) {
    uint8_t chunk = units > MCE_MAX_PULSE_LENGTH ? MCE_MAX_PULSE_LENGTH : units;
    pushByte(pulseBit | chunk);
    units -= chunk;
  }
}

static void startBurst() {
  idle = false;
#if IR_ACTIVITY_LED
  digitalWrite(LED_BUILTIN, HIGH);
#endif
}

static void endBurst(uint32_t timeoutTicks) {
  emitSample(level, timeoutTicks);
  flush();
  static const uint8_t trailer = MCE_IRDATA_TRAILER;
  sink(&trailer, 1);
  idle = true;
  level = false;
#if MCE_DEBUG_IR
  Serial.println();
#endif
#if IR_ACTIVITY_LED
  digitalWrite(LED_BUILTIN, LOW);
#endif
}

namespace IrEncoder {

void begin(IrPacketSink packetSink) {
  sink = packetSink;
#if IR_ACTIVITY_LED
  pinMode(LED_BUILTIN, OUTPUT);
#endif
}

void poll(uint16_t timeoutUnits) {
  if (!timeoutUnits)
    timeoutUnits = MCE_DEFAULT_TIMEOUT_UNITS;
  uint32_t timeoutTicks = (uint32_t)timeoutUnits * TICKS_PER_UNIT;

  IrEdge edge;
  while (IrCapture::pop(edge)) {
    if (idle) {
      // A burst starts with the first pulse; the idle gap is not reported.
      if (!edge.pulse)
        continue;
      startBurst();
    } else {
      if (edge.pulse == level)
        continue;  // missed opposite edge: keep measuring from the first one
      uint32_t duration = edge.time - lastEdge;
      if (duration >= timeoutTicks) {
        // Gap longer than the timeout that poll() hasn't closed yet
        endBurst(timeoutTicks);
        if (!edge.pulse)
          continue;
        startBurst();
      } else {
        emitSample(level, duration);
      }
    }
    level = edge.pulse;
    lastEdge = edge.time;
  }

  uint32_t now = IrCapture::now();
  if (!idle && now - lastEdge >= timeoutTicks)
    endBurst(timeoutTicks);
  else if (sampleCount && now - packetStart >= IR_FLUSH_US * IR_TICKS_PER_US)
    flush();
}

}  // namespace IrEncoder
