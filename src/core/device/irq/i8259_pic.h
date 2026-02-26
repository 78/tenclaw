#pragma once

#include "core/device/device.h"

// Minimal dual 8259A PIC stub.  Absorbs all reads/writes silently.
// Actual interrupt delivery uses the I/O APIC; this stub exists solely
// to prevent "unhandled PIO" log noise from the guest kernel's legacy
// PIC initialisation and shutdown sequences (ports 0x20-0x21, 0xA0-0xA1).
class I8259Pic : public Device {
public:
    static constexpr uint16_t kMasterBase = 0x20;
    static constexpr uint16_t kSlaveBase  = 0xA0;
    static constexpr uint16_t kRegCount   = 2;
};
