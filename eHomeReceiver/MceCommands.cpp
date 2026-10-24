#include "MceCommands.h"
#include "Config.h"
#include "MceProtocol.h"

static MceResponseSink sink;

// Device state reported back to the host
static uint16_t rxTimeout = MCE_DEFAULT_TIMEOUT_UNITS;
static uint8_t cfsPrescaler, cfsDivisor;  // carrier, only used by TX
static uint8_t txMask;
static uint8_t rxPort = 1;  // 1 = long range receiver

// Parser state
enum ParseState : uint8_t { ST_PORT, ST_CMD, ST_DATA, ST_TXDATA };
static ParseState state = ST_PORT;
static uint8_t port, cmd, need, got;
static uint8_t args[2];

#if MCE_DEBUG_CMDS
static void debugHex(const __FlashStringHelper *prefix, const uint8_t *data, uint8_t len) {
  Serial.print(prefix);
  for (uint8_t i = 0; i < len; i++) {
    Serial.print(' ');
    if (data[i] < 0x10)
      Serial.print('0');
    Serial.print(data[i], HEX);
  }
  Serial.println();
}
#else
#define debugHex(prefix, data, len)
#endif

static void respond(const uint8_t *data, uint8_t len) {
  debugHex(F("<"), data, len);
  sink(data, len);
}

static void respond2(uint8_t port, uint8_t rsp) {
  uint8_t r[] = {port, rsp};
  respond(r, sizeof(r));
}

static void respond3(uint8_t port, uint8_t rsp, uint8_t a) {
  uint8_t r[] = {port, rsp, a};
  respond(r, sizeof(r));
}

static void respond4(uint8_t port, uint8_t rsp, uint8_t a, uint8_t b) {
  uint8_t r[] = {port, rsp, a, b};
  respond(r, sizeof(r));
}

static void respondIllegal() {
  respond2(MCE_CMD_PORT_SYS, MCE_RSP_CMD_ILLEGAL);
}

// Argument bytes that follow a host command, or -1 if the command is unknown.
static int8_t commandArgSize(uint8_t port, uint8_t cmd) {
  if (port == MCE_CMD_PORT_SYS) {
    switch (cmd) {
    case MCE_CMD_GETPORTSTATUS:
      return 1;
    case MCE_CMD_G_REVISION:
    case MCE_CMD_GETWAKESOURCE:
    case MCE_CMD_GETWAKEVERSION:
    case MCE_CMD_GETWAKESUPPORT:
    case MCE_CMD_GETDEVDETAILS:
    case MCE_CMD_GETEMVER:
    case MCE_CMD_FLASHLED:
    case MCE_CMD_RESUME:
    case MCE_CMD_RESET:
    case MCE_CMD_NOP:
      return 0;
    }
  } else {
    switch (cmd) {
    case MCE_CMD_SETIRCFS:
    case MCE_CMD_SETIRTIMEOUT:
      return 2;
    case MCE_CMD_SIG_END:
    case MCE_CMD_SETIRTXPORTS:
    case MCE_CMD_SETIRRXPORTEN:
      return 1;
    case MCE_CMD_PING:
    case MCE_CMD_UNKNOWN2:
    case MCE_CMD_GETIRCFS:
    case MCE_CMD_GETIRTIMEOUT:
    case MCE_CMD_GETIRTXPORTS:
    case MCE_CMD_GETIRRXPORTEN:
    case MCE_CMD_GETIRNUMPORTS:
      return 0;
    }
  }
  return -1;
}

static void handleSys() {
  switch (cmd) {
  case MCE_CMD_GETEMVER:
    respond3(MCE_CMD_PORT_SYS, MCE_RSP_EQEMVER, MCE_EMULATOR_VERSION);
    break;
  case MCE_CMD_G_REVISION: {
    // hw/sw revision, 4 bytes
    uint8_t r[] = {MCE_CMD_PORT_SYS, MCE_CMD_G_REVISION, 0x00, 0x01, 0x00, 0x01};
    respond(r, sizeof(r));
    break;
  }
  case MCE_CMD_GETWAKEVERSION: {
    // protocol, payload, address, version: no wake support (all zero)
    uint8_t r[] = {MCE_CMD_PORT_SYS, MCE_RSP_EQWAKEVERSION, 0x00, 0x00, 0x00, 0x00};
    respond(r, sizeof(r));
    break;
  }
  case MCE_CMD_GETWAKESUPPORT:
    respond3(MCE_CMD_PORT_SYS, MCE_RSP_EQWAKESUPPORT, 0x00);
    break;
  case MCE_CMD_GETWAKESOURCE:
    respond3(MCE_CMD_PORT_SYS, MCE_RSP_GETWAKESOURCE, 0x00);
    break;
  case MCE_CMD_GETDEVDETAILS:
    respond3(MCE_CMD_PORT_SYS, MCE_RSP_EQDEVDETAILS, 0x00);
    break;
  case MCE_CMD_GETPORTSTATUS: {
    // port, ?, ?, not-connected flag, ?: we have no TX ports at all
    uint8_t r[] = {MCE_CMD_PORT_SYS, MCE_RSP_GETPORTSTATUS, args[0], 0x00, 0x00, 0x01, 0x00};
    respond(r, sizeof(r));
    break;
  }
  case MCE_CMD_FLASHLED:
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    break;
  case MCE_CMD_RESUME:
  case MCE_CMD_RESET:
  case MCE_CMD_NOP:
    break;
  }
}

static void handleIr() {
  switch (cmd) {
  case MCE_CMD_GETIRNUMPORTS:
    respond4(MCE_CMD_PORT_IR, MCE_RSP_EQIRNUMPORTS, 0 /* tx */, 1 /* rx */);
    break;
  case MCE_CMD_SETIRTIMEOUT:
    rxTimeout = ((uint16_t)args[0] << 8) | args[1];
    if (!rxTimeout)
      rxTimeout = MCE_DEFAULT_TIMEOUT_UNITS;
    // Devices echo the new value
    // fall through
  case MCE_CMD_GETIRTIMEOUT:
    respond4(MCE_CMD_PORT_IR, MCE_RSP_EQIRTIMEOUT, rxTimeout >> 8, rxTimeout & 0xFF);
    break;
  case MCE_CMD_SETIRCFS:
    cfsPrescaler = args[0];
    cfsDivisor = args[1];
    // fall through
  case MCE_CMD_GETIRCFS:
    respond4(MCE_CMD_PORT_IR, MCE_RSP_EQIRCFS, cfsPrescaler, cfsDivisor);
    break;
  case MCE_CMD_SETIRTXPORTS:
    txMask = args[0];
    // fall through
  case MCE_CMD_GETIRTXPORTS:
    respond3(MCE_CMD_PORT_IR, MCE_RSP_EQIRTXPORTS, txMask);
    break;
  case MCE_CMD_SETIRRXPORTEN:
    // Only the demodulating receiver exists; remember the request anyway
    rxPort = args[0];
    // fall through
  case MCE_CMD_GETIRRXPORTEN:
    respond3(MCE_CMD_PORT_IR, MCE_RSP_EQIRRXPORTEN, rxPort);
    break;
  case MCE_CMD_UNKNOWN2:
    respond4(MCE_CMD_PORT_IR, MCE_CMD_UNKNOWN, 0x00, 0x00);
    break;
  case MCE_CMD_SIG_END:
  case MCE_CMD_PING:
    break;
  }
}

static void dispatch() {
#if MCE_DEBUG_CMDS
  uint8_t c[4] = {port, cmd, args[0], args[1]};
  debugHex(F(">"), c, 2 + need);
#endif
  if (port == MCE_CMD_PORT_SYS)
    handleSys();
  else
    handleIr();
}

namespace MceCommands {

void begin(MceResponseSink responseSink) {
  sink = responseSink;
}

void feed(const uint8_t *data, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    uint8_t b = data[i];
    switch (state) {
    case ST_PORT:
      if (b == MCE_CMD_PORT_SYS || b == MCE_CMD_PORT_IR) {
        port = b;
        state = ST_CMD;
      } else if ((b & MCE_PORT_MASK) == MCE_IRDATA) {
        // TX data from the host: not supported yet, skip the samples
        need = b & MCE_PACKET_LENGTH_MASK;
        if (need)
          state = ST_TXDATA;
      } else if (b != MCE_CMD_NULL) {
        debugHex(F("? stray"), &b, 1);
      }
      break;

    case ST_CMD: {
      cmd = b;
      int8_t size = commandArgSize(port, cmd);
      if (size < 0) {
#if MCE_DEBUG_CMDS
        uint8_t c[] = {port, cmd};
        debugHex(F("> unknown"), c, 2);
#endif
        respondIllegal();
        state = ST_PORT;
      } else if (size == 0) {
        need = 0;
        dispatch();
        state = ST_PORT;
      } else {
        need = size;
        got = 0;
        state = ST_DATA;
      }
      break;
    }

    case ST_DATA:
      args[got++] = b;
      if (got == need) {
        dispatch();
        state = ST_PORT;
      }
      break;

    case ST_TXDATA:
      if (--need == 0)
        state = ST_PORT;
      break;
    }
  }
}

uint16_t timeoutUnits() {
  return rxTimeout;
}

}  // namespace MceCommands
