// MCE / eHome IR port protocol constants.
// Values follow the Linux mceusb driver (drivers/media/rc/mceusb.c).
#pragma once

#define MCE_TIME_UNIT_US 50   // one sample unit
#define MCE_MAX_SAMPLES 30    // samples per IR data packet
#define MCE_PULSE_BIT 0x80
#define MCE_PULSE_MASK 0x7F
#define MCE_MAX_PULSE_LENGTH 0x7F

#define MCE_IRDATA 0x80       // IR data packet header: 0x80 | count
#define MCE_IRDATA_TRAILER 0x80
#define MCE_PORT_MASK 0xE0
#define MCE_PACKET_LENGTH_MASK 0x1F

// Port bytes
#define MCE_CMD_NULL 0x00
#define MCE_CMD_PORT_IR 0x9F
#define MCE_CMD_PORT_SYS 0xFF

// MCE_CMD_PORT_SYS commands
#define MCE_CMD_G_REVISION 0x0B
#define MCE_CMD_GETPORTSTATUS 0x11
#define MCE_CMD_GETWAKESOURCE 0x17
#define MCE_CMD_GETWAKEVERSION 0x18
#define MCE_CMD_UNKNOWN8 0x19
#define MCE_CMD_UNKNOWN9 0x1B
#define MCE_CMD_GETWAKESUPPORT 0x20
#define MCE_CMD_GETDEVDETAILS 0x21
#define MCE_CMD_GETEMVER 0x22
#define MCE_CMD_FLASHLED 0x23
#define MCE_CMD_RESUME 0xAA
#define MCE_CMD_RESET 0xFE
#define MCE_CMD_NOP 0xFF

// MCE_CMD_PORT_IR commands
#define MCE_CMD_SIG_END 0x01
#define MCE_CMD_PING 0x03
#define MCE_CMD_UNKNOWN 0x04
#define MCE_CMD_UNKNOWN2 0x05
#define MCE_CMD_SETIRCFS 0x06
#define MCE_CMD_GETIRCFS 0x07
#define MCE_CMD_SETIRTXPORTS 0x08
#define MCE_CMD_SETIRTIMEOUT 0x0C
#define MCE_CMD_GETIRTIMEOUT 0x0D
#define MCE_CMD_GETIRTXPORTS 0x13
#define MCE_CMD_SETIRRXPORTEN 0x14
#define MCE_CMD_GETIRRXPORTEN 0x15
#define MCE_CMD_GETIRNUMPORTS 0x16

// Responses (device -> host)
#define MCE_RSP_EQIRCFS 0x06
#define MCE_RSP_EQIRTXPORTS 0x08
#define MCE_RSP_EQIRTIMEOUT 0x0C
#define MCE_RSP_GETPORTSTATUS 0x11
#define MCE_RSP_EQIRRXPORTEN 0x14
#define MCE_RSP_EQIRRXCFCNT 0x15
#define MCE_RSP_EQIRNUMPORTS 0x16
#define MCE_RSP_GETWAKESOURCE 0x17
#define MCE_RSP_EQWAKEVERSION 0x18
#define MCE_RSP_EQWAKESUPPORT 0x20
#define MCE_RSP_EQDEVDETAILS 0x21
#define MCE_RSP_EQEMVER 0x22
#define MCE_RSP_CMD_ILLEGAL 0xFE

// Emulator interface version we claim (IR2CMPT = v2 compatible)
#define MCE_EMULATOR_VERSION 2

// Default receive timeout: 100 ms in 50 us units
#define MCE_DEFAULT_TIMEOUT_UNITS 2000
