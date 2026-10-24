# Arduino based Windows Media Center (eHome) IR Receiver (ATmega32u4)

Firmware that turns an ATmega32u4 board (Arduino Leonardo, DFRobot Beetle, Pro Micro...)
plus a 38 kHz IR receiver module into a Windows Media Center–compatible **eHome USB IR
receiver**. It speaks the MCE IR port protocol (the same one the Linux `mceusb` driver
implements), so Windows binds its **in-box** driver (`usbcir.sys` → `circlass.sys` →
`hidir.sys`) and MCE (RC6) remotes work without installing anything on Windows Vista
and later. Windows XP (Media Center Edition) uses its own in-box `irbus.sys` driver too,
but needs a small INF to be installed once, see [Windows XP](#windows-xp-media-center-edition).

This version is receive-only. It has no IR transmit, no learning (carrier measurement)
receiver and no wake-from-sleep.

### AI disclaimer

This project was developed with the aid of AI tools, using the Linux kernel 
[mceusb.c](https://github.com/torvalds/linux/blob/master/drivers/media/rc/mceusb.c) driver 
implementation as a reference.

## How it is recognized by Windows

The board shows up as a composite USB device:

| Interface | Function | Driver |
|---|---|---|
| 0–1 | CDC serial port (uploads, debug log) | `usbser.sys` |
| 2 | MCE IR port, class FF/FF/FF, 2 endpoints (bulk or interrupt, see `Config.h`) | `usbcir.sys` (Vista+) / `irbus.sys` (XP) |

The firmware reports a Microsoft OS 1.0 compatible ID for interface 2 (string `0xEE` +
Extended Compat ID), so no Microsoft VID/PID is needed. Windows sees it as
`USB\MS_COMP_USBCIR&MS_SUBCOMP_<id>`, with `<id>` set by `MCE_USB_SUBCOMPATIBLE_ID`
in `Config.h`:

| `<id>` | Meaning | Matched by Vista and later (`usbcir.inf`) | Listed in XP SP3 `irbus.inf` |
|---|---|---|---|
| `IRRX` (default) | receive-only "Snowflake" | yes | yes (but not applied, see below) |
| `IRV2` | transceiver "Beanbag" | yes | yes (but not applied, see below) |
| `IR2CMPT` | generic compatible device | yes | no |

XP's `irbus.inf` lists the `IRRX`/`IRV2` compatible IDs, but XP doesn't assign MS OS
compatible IDs to individual functions of a composite device, so it never matches
them on interface 2. That's why XP needs the INF described below.

The device descriptor (VID/PID/bcdDevice) and the product string are served by the
firmware itself (`MceUsb.cpp`), so the stock **Arduino Leonardo** board selection is
all you need.

## Hardware

TSOP382xx / VS1838B (or similar 38 kHz demodulating receiver):

```
TSOP VS  ── 100 Ω ──┬── 5V
                    └── 4.7 µF ── GND
TSOP GND ─────────────── GND
TSOP OUT ─────────────── IR input pin (see below)
```

Pick the input backend in `eHomeReceiver/Config.h`:

| Backend | `IR_INPUT` | Pins | Notes |
|---|---|---|---|
| External interrupt | `IR_INPUT_EXTINT` | `IR_EXTINT_NUM` = 0 (SCL), 1 (SDA), 2 (RX), 3 (TX), 6 (D7, Leonardo only) | Edge time read from Timer1 in the ISR; uses up I²C/Serial1 on that pin. |
| Analog comparator | `IR_INPUT_ANACOMP` | `IR_ANACOMP_ADC` = 7 (A0), 6 (A1), 5 (A2), 4 (A3), 1 (A4), 0 (A5) | For boards with no free interrupt pin. Comparator vs the 1.1 V bandgap triggers Timer1 input capture (hardware timestamps). Disables the ADC (`analogRead()` unavailable). |

The default is the comparator on **A0**. Both backends take over **Timer1**, so
`analogWrite()` on D9/D10 and the Servo library can't be used. `LED_BUILTIN` (D13)
lights during IR bursts and toggles on the host's FLASHLED command.

## Build and flash

Arduino IDE: open `eHomeReceiver/eHomeReceiver.ino`, select **Arduino Leonardo**, and
upload. The CDC port stays, so the usual auto-reset upload works.

arduino-cli:

```
arduino-cli compile --fqbn arduino:avr:leonardo eHomeReceiver
arduino-cli upload  --fqbn arduino:avr:leonardo -p COMx eHomeReceiver
```

## First connection on Windows Vista and later

1. Plug in the board. Device Manager should show a COM port and **Microsoft eHome
   Infrared Transceiver** (or "eHome Infrared Transceiver") plus a HID IR device.
   In USBTreeView, interface 2 should list the compatible ID
   `USB\MS_COMP_USBCIR&MS_SUBCOMP_IRRX` (or whichever ID is configured).
2. Press buttons on an MCE remote. Arrows/OK/volume/play should act on Windows.

**The MS OS descriptor is cached.** Windows asks for string `0xEE` only once per
VID+PID+bcdDevice and stores the result under
`HKLM\SYSTEM\CurrentControlSet\Control\usbflags\VVVVPPPPRRRR` (currently
`120900070101`). If the device was ever
plugged in with different descriptors (or the lookup failed), either delete that key
(admin) and replug, or bump `MCE_USB_BCD_DEVICE` in `Config.h`.

## Windows XP (Media Center Edition)

XP does **not** install the driver automatically. You have to install
`drivers/xp/ehome-ir-xp.inf` once. It matches the IR interface
(`USB\VID_1209&PID_0007&MI_02`) and reuses the receive-only section of XP's own
`irbus.inf`: the in-box `irbus.sys` driver, IR decoder settings and power keys. It
contains no driver files of its own.

Requirements: XP SP2 or later (for the IAD the composite layout needs); it was tested
on XP MCE with SP3's `irbus.sys` 5.1.2600.5512.

1. Copy the `drivers/xp` folder to the XP machine and plug in the board.
2. In Device Manager, open the unknown **eHome Infrared Transceiver** device (interface 2)
   and choose *Update Driver → Install from a list or specific location → Don't search,
   I will choose → Have Disk*, then point it at the copied folder.
3. Accept the unsigned-driver warning (*Continue Anyway*). If Setup asks for the XP CD,
   browse to `C:\WINDOWS\system32\drivers`, where `irbus.sys` already is.
4. The device installs as **eHome Infrared Receiver (DIY)**, followed by the HID IR
   devices from `input.inf`/`keyboard.inf`.

If you change `MCE_USB_VID`/`MCE_USB_PID` in `Config.h`, update the hardware ID in the
INF to match.

The CDC serial port (interface 0) has no driver on XP and shows up as an unknown
device; that doesn't affect the IR function. To use the debug log on XP, install the
Arduino CDC INF with `USB\VID_1209&PID_0007&MI_00` added.

## Debugging

Open the Serial Monitor on the board's COM port (any baud rate):

- `MCE_DEBUG_CMDS 1` logs every host command (`> 9F 0D`) and our response (`< 9F 0C 07 D0`).
  Unknown commands are logged as `> unknown` and answered with `FF FE` (illegal command).
  This log is the way to find anything Windows expects that isn't implemented.
- `MCE_DEBUG_IR 1` prints each burst as a pulse/space train in µs (`+2650 -900 +450 ...`).
- `! dropped packets` means IR data was produced while no host driver was reading the
  IN endpoint (it's dropped rather than stalling capture). Edge overruns mean the
  128-edge capture buffer filled up.

## Fallbacks if Windows does not start the device

- **Interrupt endpoints:** set `MCE_USB_INTERRUPT_EPS 1` (some receivers use interrupt
  rather than bulk endpoints), bump `MCE_USB_BCD_DEVICE`, and reflash.
- **No CDC:** if `usbcir` refuses to bind to a composite child, build with CDC
  disabled (add `-DCDC_DISABLED` via a custom board or `platform.local.txt`) so the IR
  interface becomes interface 0, and change the device class in `MceUsb.cpp` to
  `0x00, 0x00, 0x00`. Uploading then needs a manual reset (double-tap on Leonardo-style
  boards) to enter the bootloader.

## File overview

| File | Purpose |
|---|---|
| `eHomeReceiver/Config.h` | Input backend, pins, USB identity, debug switches |
| `eHomeReceiver/MceUsb.*` | PluggableUSB module: descriptors, MS OS descriptors, endpoint I/O |
| `eHomeReceiver/MceCommands.*` | Host command parser and responses |
| `eHomeReceiver/IrCapture.*` | Timer1 edge capture (external interrupt or comparator backend) |
| `eHomeReceiver/IrEncoder.*` | Edges → MCE IR data packets (50 µs units, ≤30 samples, 0x80 trailer) |
| `eHomeReceiver/MceProtocol.h` | Protocol constants (from Linux `mceusb.c`) |
| `drivers/xp/ehome-ir-xp.inf` | Windows XP install for the IR interface (uses the in-box `irbus.inf`) |
