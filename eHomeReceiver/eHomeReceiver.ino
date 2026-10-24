// eHome (Windows Media Center) USB IR receiver for ATmega32u4 boards.
//
// Shows up as a composite device: the usual CDC serial port (uploads, debug
// log) plus a vendor interface that Windows binds to its in-box usbcir.sys
// driver via a Microsoft OS compatible ID. See README.md.

#include "Config.h"
#include "IrCapture.h"
#include "IrEncoder.h"
#include "MceCommands.h"
#include "MceUsb.h"

static uint16_t droppedPackets;

static void sendIrPacket(const uint8_t *packet, uint8_t len) {
  // Don't stall capture if the host isn't reading (e.g. no driver bound)
  if (!MceUsb.write(packet, len, false))
    droppedPackets++;
}

static void sendResponse(const uint8_t *data, uint8_t len) {
  MceUsb.write(data, len, true);
}

#if MCE_DEBUG_CMDS || MCE_DEBUG_IR
static void reportDrops() {
  static uint16_t lastDropped, lastOverruns;
  uint16_t overruns = IrCapture::overruns();
  if (droppedPackets != lastDropped || overruns != lastOverruns) {
    lastDropped = droppedPackets;
    lastOverruns = overruns;
    Serial.print(F("! dropped packets: "));
    Serial.print(droppedPackets);
    Serial.print(F(", edge overruns: "));
    Serial.println(overruns);
  }
}
#endif

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  MceCommands::begin(sendResponse);
  IrEncoder::begin(sendIrPacket);
  IrCapture::begin();
}

void loop() {
  uint8_t buf[USB_EP_SIZE];
  int n = MceUsb.read(buf, sizeof(buf));
  if (n > 0)
    MceCommands::feed(buf, n);

  IrEncoder::poll(MceCommands::timeoutUnits());

#if MCE_DEBUG_CMDS || MCE_DEBUG_IR
  reportDrops();
#endif
#if MCE_DEBUG_USB
  MceUsb.printControlLog();
#endif
}
