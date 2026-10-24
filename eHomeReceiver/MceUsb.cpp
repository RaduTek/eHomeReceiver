#include "MceUsb.h"
#include "Config.h"

MceUsb_ MceUsb;

#if MCE_USB_INTERRUPT_EPS
#define MCE_EP_ATTR USB_ENDPOINT_TYPE_INTERRUPT
#define MCE_EP_INTERVAL 1
#define MCE_EP_TYPE_IN EP_TYPE_INTERRUPT_IN
#define MCE_EP_TYPE_OUT EP_TYPE_INTERRUPT_OUT
#else
#define MCE_EP_ATTR USB_ENDPOINT_TYPE_BULK
#define MCE_EP_INTERVAL 0
#define MCE_EP_TYPE_IN EP_TYPE_BULK_IN
#define MCE_EP_TYPE_OUT EP_TYPE_BULK_OUT
#endif

#define MS_OS_STRING_INDEX 0xEE
#define MS_OS_COMPAT_ID_INDEX 0x0004

// Replaces the core's device descriptor so we control VID/PID/bcdDevice
// without a custom board definition. Class EF/02/01 (IAD) is kept because
// the CDC function is still present.
static const DeviceDescriptor deviceDescriptor PROGMEM =
    D_DEVICE(0xEF, 0x02, 0x01, 64, MCE_USB_VID, MCE_USB_PID, MCE_USB_BCD_DEVICE,
             IMANUFACTURER, IPRODUCT, ISERIAL, 1);

static const char manufacturerString[] PROGMEM = MCE_USB_MANUFACTURER;
static const char productString[] PROGMEM = MCE_USB_PRODUCT;

// "MSFT100" + vendor code: tells Windows to issue the MS OS vendor requests.
static const uint8_t msOsString[] PROGMEM = {
    0x12, 0x03,
    'M', 0, 'S', 0, 'F', 0, 'T', 0, '1', 0, '0', 0, '0', 0,
    MCE_MS_VENDOR_CODE, 0x00};

struct MsCompatIdDescriptor {
  uint32_t dwLength;
  uint16_t bcdVersion;
  uint16_t wIndex;
  uint8_t bCount;
  uint8_t reserved[7];
  uint8_t bFirstInterfaceNumber;
  uint8_t reserved1;
  char compatibleID[8];
  char subCompatibleID[8];
  uint8_t reserved2[6];
} __attribute__((packed));

struct MceInterfaceDescriptor {
  InterfaceDescriptor iface;
  EndpointDescriptor first;
  EndpointDescriptor second;
};

#if MCE_DEBUG_USB
// Control requests arrive in the USB interrupt, where printing is not
// possible; they are queued here and printed from loop().
struct ControlLogEntry {
  USBSetup setup;
  bool descriptor;  // GET_DESCRIPTOR passed to getDescriptor()
  bool handled;
};
#define CONTROL_LOG_SIZE 16  // power of two
static ControlLogEntry controlLog[CONTROL_LOG_SIZE];
static volatile uint8_t controlHead, controlTail;
static volatile uint8_t controlLost;

static void logControl(const USBSetup &setup, bool descriptor, bool handled) {
  uint8_t next = (controlHead + 1) & (CONTROL_LOG_SIZE - 1);
  if (next == controlTail) {
    controlLost++;
    return;
  }
  controlLog[controlHead].setup = setup;
  controlLog[controlHead].descriptor = descriptor;
  controlLog[controlHead].handled = handled;
  controlHead = next;
}

static void printHex8(uint8_t v) {
  if (v < 0x10)
    Serial.print('0');
  Serial.print(v, HEX);
}

static void printHex16(uint16_t v) {
  printHex8(v >> 8);
  printHex8(v & 0xFF);
}
#else
#define logControl(setup, descriptor, handled)
#endif

void MceUsb_::printControlLog() {
#if MCE_DEBUG_USB
  while (controlTail != controlHead) {
    ControlLogEntry &e = controlLog[controlTail];
    Serial.print(e.descriptor ? F("ctl desc ") : F("ctl req  "));
    printHex8(e.setup.bmRequestType);
    Serial.print(' ');
    printHex8(e.setup.bRequest);
    Serial.print(F(" wValue="));
    printHex8(e.setup.wValueH);
    printHex8(e.setup.wValueL);
    Serial.print(F(" wIndex="));
    printHex16(e.setup.wIndex);
    Serial.print(F(" wLength="));
    printHex16(e.setup.wLength);
    Serial.println(e.handled ? F(" -> handled") : (e.descriptor ? F(" -> core") : F(" -> STALL")));
    controlTail = (controlTail + 1) & (CONTROL_LOG_SIZE - 1);
  }
  if (controlLost) {
    Serial.print(F("! control log overflow: "));
    Serial.println(controlLost);
    controlLost = 0;
  }
#endif
}

MceUsb_::MceUsb_() : PluggableUSBModule(2, 1, epTypes) {
#if MCE_USB_OUT_FIRST
  epTypes[0] = MCE_EP_TYPE_OUT;
  epTypes[1] = MCE_EP_TYPE_IN;
#else
  epTypes[0] = MCE_EP_TYPE_IN;
  epTypes[1] = MCE_EP_TYPE_OUT;
#endif
  PluggableUSB().plug(this);
}

// Endpoints are numbered in the order they appear in epTypes
uint8_t MceUsb_::inEp() const {
  return pluggedEndpoint + MCE_USB_OUT_FIRST;
}

uint8_t MceUsb_::outEp() const {
  return pluggedEndpoint + !MCE_USB_OUT_FIRST;
}

int MceUsb_::getInterface(uint8_t *interfaceCount) {
  *interfaceCount += 1;
  MceInterfaceDescriptor desc = {
      D_INTERFACE(pluggedInterface, 2, 0xFF, 0xFF, 0xFF),
#if MCE_USB_OUT_FIRST
      D_ENDPOINT(USB_ENDPOINT_OUT(outEp()), MCE_EP_ATTR, USB_EP_SIZE, MCE_EP_INTERVAL),
      D_ENDPOINT(USB_ENDPOINT_IN(inEp()), MCE_EP_ATTR, USB_EP_SIZE, MCE_EP_INTERVAL)};
#else
      D_ENDPOINT(USB_ENDPOINT_IN(inEp()), MCE_EP_ATTR, USB_EP_SIZE, MCE_EP_INTERVAL),
      D_ENDPOINT(USB_ENDPOINT_OUT(outEp()), MCE_EP_ATTR, USB_EP_SIZE, MCE_EP_INTERVAL)};
#endif
  return USB_SendControl(0, &desc, sizeof(desc));
}

// Sends a PROGMEM ASCII string as a UTF-16 string descriptor.
static int sendStringDescriptor(const char *str_P) {
  uint8_t len = strlen_P(str_P);
  uint8_t header[2] = {(uint8_t)(2 + len * 2), 3};
  if (USB_SendControl(0, header, 2) < 0)
    return -1;
  for (uint8_t i = 0; i < len; i++) {
    uint8_t ch[2] = {(uint8_t)pgm_read_byte(str_P + i), 0};
    if (USB_SendControl(0, ch, 2) < 0)
      return -1;
  }
  return 1;
}

static int sendDescriptor(USBSetup &setup) {
  if (setup.wValueH == USB_DEVICE_DESCRIPTOR_TYPE)
    return USB_SendControl(TRANSFER_PGM, &deviceDescriptor, sizeof(deviceDescriptor));

  if (setup.wValueH == USB_STRING_DESCRIPTOR_TYPE) {
    switch (setup.wValueL) {
    case IMANUFACTURER:
      return sendStringDescriptor(manufacturerString);
    case IPRODUCT:
      return sendStringDescriptor(productString);
    case MS_OS_STRING_INDEX:
      return USB_SendControl(TRANSFER_PGM, msOsString, sizeof(msOsString));
    }
  }
  return 0;
}

int MceUsb_::getDescriptor(USBSetup &setup) {
  int ret = sendDescriptor(setup);
  logControl(setup, true, ret != 0);
  return ret;
}

bool MceUsb_::setup(USBSetup &setup) {
  bool handled = handleRequest(setup);
  logControl(setup, false, handled);
  return handled;
}

bool MceUsb_::handleRequest(USBSetup &setup) {
  // MS OS 1.0 Extended Compat ID request: maps our interface to
  // USB\MS_COMP_USBCIR&MS_SUBCOMP_<id>, which irbus.inf (XP) and usbcir.inf match.
  if (setup.bmRequestType == (REQUEST_DEVICETOHOST | REQUEST_VENDOR | REQUEST_DEVICE) &&
      setup.bRequest == MCE_MS_VENDOR_CODE && setup.wIndex == MS_OS_COMPAT_ID_INDEX) {
    MsCompatIdDescriptor desc = {};
    desc.dwLength = sizeof(desc);
    desc.bcdVersion = 0x0100;
    desc.wIndex = MS_OS_COMPAT_ID_INDEX;
    desc.bCount = 1;
    desc.bFirstInterfaceNumber = pluggedInterface;
    desc.reserved1 = 0x01;
    static_assert(sizeof(MCE_USB_SUBCOMPATIBLE_ID) <= sizeof(desc.subCompatibleID) + 1,
                  "MCE_USB_SUBCOMPATIBLE_ID is longer than 8 characters");
    memcpy(desc.compatibleID, "USBCIR", 6);
    memcpy(desc.subCompatibleID, MCE_USB_SUBCOMPATIBLE_ID, sizeof(MCE_USB_SUBCOMPATIBLE_ID) - 1);
    return USB_SendControl(0, &desc, sizeof(desc)) >= 0;
  }
  return false;
}

uint8_t MceUsb_::getShortName(char *name) {
  static const char serial[] = MCE_USB_SERIAL;
  memcpy(name, serial, sizeof(serial) - 1);
  return sizeof(serial) - 1;
}

int MceUsb_::read(uint8_t *buf, int len) {
  if (!USBDevice.configured() || !USB_Available(outEp()))
    return 0;
  int n = USB_Recv(outEp(), buf, len);
  return n < 0 ? 0 : n;
}

bool MceUsb_::write(const uint8_t *data, uint8_t len, bool block) {
  if (!USBDevice.configured())
    return false;
  if (!block && USB_SendSpace(inEp()) < len)
    return false;
  return USB_Send(inEp() | TRANSFER_RELEASE, data, len) == len;
}
