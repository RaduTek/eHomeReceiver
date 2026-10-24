// Build-time configuration for the eHome IR receiver firmware.
#pragma once

// ---------------------------------------------------------------------------
// IR input backend
// ---------------------------------------------------------------------------
// IR_INPUT_EXTINT  : receiver on an external interrupt pin (INT0..3, INT6).
//                    Edge time is read from Timer1 in the ISR (a few us latency).
// IR_INPUT_ANACOMP : receiver on an ADC pin, routed through the analog
//                    comparator into the Timer1 input capture unit. Use this on
//                    boards where no interrupt pin is broken out.
#define IR_INPUT_EXTINT  1
#define IR_INPUT_ANACOMP 2

#define IR_INPUT IR_INPUT_ANACOMP

// IR_INPUT_EXTINT: interrupt number. On the 32u4:
//   0 = PD0 (SCL / D3)   1 = PD1 (SDA / D2)   2 = PD2 (RX / D0)
//   3 = PD3 (TX / D1)    6 = PE6 (D7, Leonardo only)
#define IR_EXTINT_NUM 2

// IR_INPUT_ANACOMP: ADC channel used as comparator negative input.
//   0 = PF0 (A5)  1 = PF1 (A4)  4 = PF4 (A3)  5 = PF5 (A2)  6 = PF6 (A1)  7 = PF7 (A0)
// Note: the ADC is disabled in this mode, so analogRead() is unavailable.
#define IR_ANACOMP_ADC 7

// Microseconds to wait after the first sample of a packet before pushing a
// partially filled packet to the host. Keeps key latency low.
#define IR_FLUSH_US 8000UL

// Light LED_BUILTIN while an IR burst is being received.
#define IR_ACTIVITY_LED 1

// ---------------------------------------------------------------------------
// USB identity
// ---------------------------------------------------------------------------
// Windows binds its eHome driver through the MS OS compatible ID USBCIR, so any
// VID/PID works. 0x1209/0x0007 is a pid.codes test PID (private use only).
#define MCE_USB_VID 0x1209
#define MCE_USB_PID 0x0007
// Windows caches the MS OS descriptor lookup per VID+PID+bcdDevice
// (HKLM\SYSTEM\CurrentControlSet\Control\usbflags\VVVVPPPPRRRR).
// Bump this after changing descriptors to force a fresh lookup.
#define MCE_USB_BCD_DEVICE 0x0101

// MS OS sub-compatible ID (max 8 chars), matched against USB\MS_COMP_USBCIR&MS_SUBCOMP_<id>:
//   "IRRX"    receive-only "Snowflake"   XP SP3 irbus.sys, Vista+ usbcir.sys
//   "IRV2"    transceiver "Beanbag"      XP SP3 irbus.sys, Vista+ usbcir.sys
//   "IR2CMPT" generic compatible device  Vista+ usbcir.sys only
#define MCE_USB_SUBCOMPATIBLE_ID "IRRX"
#define MCE_USB_MANUFACTURER "DIY"
#define MCE_USB_PRODUCT "Arduino USBCIR eHome Infrared Transceiver"
#define MCE_USB_SERIAL "EHIR0001"

// Use interrupt endpoints instead of bulk (fallback if usbcir refuses bulk).
#define MCE_USB_INTERRUPT_EPS 1

// Endpoint order: 0 = IN first (EP4 IN, EP5 OUT), 1 = OUT first (EP4 OUT, EP5 IN).
#define MCE_USB_OUT_FIRST 0

// bRequest used for the MS OS 1.0 vendor request.
#define MCE_MS_VENDOR_CODE 0x01

// ---------------------------------------------------------------------------
// Debug output on the CDC serial port (open the Serial Monitor at any baud)
// ---------------------------------------------------------------------------
#define MCE_DEBUG_CMDS 0  // host commands and our responses, in hex
#define MCE_DEBUG_IR 0    // pulse/space train of each burst, in us
#define MCE_DEBUG_USB 0   // descriptor and vendor/class control requests seen by the IR function

// ---------------------------------------------------------------------------
// Sanity checks
// ---------------------------------------------------------------------------
#if IR_INPUT == IR_INPUT_EXTINT
#if !(IR_EXTINT_NUM == 0 || IR_EXTINT_NUM == 1 || IR_EXTINT_NUM == 2 || IR_EXTINT_NUM == 3 || IR_EXTINT_NUM == 6)
#error "IR_EXTINT_NUM must be 0, 1, 2, 3 or 6"
#endif
#elif IR_INPUT == IR_INPUT_ANACOMP
#if !(IR_ANACOMP_ADC == 0 || IR_ANACOMP_ADC == 1 || (IR_ANACOMP_ADC >= 4 && IR_ANACOMP_ADC <= 7))
#error "IR_ANACOMP_ADC must be 0, 1, 4, 5, 6 or 7"
#endif
#else
#error "IR_INPUT must be IR_INPUT_EXTINT or IR_INPUT_ANACOMP"
#endif
