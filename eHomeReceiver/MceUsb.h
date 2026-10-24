// USB function for the eHome IR receiver, plugged into the Arduino core's
// USB stack next to the CDC serial port.
#pragma once

#include <Arduino.h>
#include <PluggableUSB.h>

class MceUsb_ : public PluggableUSBModule {
public:
  MceUsb_();

  // Non-blocking read from the OUT endpoint. Returns bytes read (0 if none).
  int read(uint8_t *buf, int len);

  // Sends one packet on the IN endpoint (len <= 64). If block is false and
  // the endpoint has no room (host not polling), the data is dropped and
  // false is returned.
  bool write(const uint8_t *data, uint8_t len, bool block);

  // Prints control requests recorded by the USB interrupt (MCE_DEBUG_USB).
  void printControlLog();

protected:
  bool setup(USBSetup &setup) override;
  int getInterface(uint8_t *interfaceCount) override;
  int getDescriptor(USBSetup &setup) override;
  uint8_t getShortName(char *name) override;

private:
  uint8_t epTypes[2];

  bool handleRequest(USBSetup &setup);

  uint8_t inEp() const;
  uint8_t outEp() const;
};

extern MceUsb_ MceUsb;
