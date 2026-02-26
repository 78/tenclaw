#pragma once

#include "core/device/device.h"
#include <ctime>

// Minimal CMOS/RTC (MC146818) emulation.
// Ports: 0x70 (address/NMI control) and 0x71 (data).
class CmosRtc : public Device {
public:
    static constexpr uint16_t kBasePort = 0x70;
    static constexpr uint16_t kRegCount = 2;

    void PioRead(uint16_t offset, uint8_t size, uint32_t* value) override;
    void PioWrite(uint16_t offset, uint8_t size, uint32_t value) override;

private:
    uint8_t ReadRegister(uint8_t reg) const;

    uint8_t index_ = 0;

    // Status registers
    static constexpr uint8_t kRegSeconds     = 0x00;
    static constexpr uint8_t kRegMinutes     = 0x02;
    static constexpr uint8_t kRegHours       = 0x04;
    static constexpr uint8_t kRegDayOfWeek   = 0x06;
    static constexpr uint8_t kRegDayOfMonth  = 0x07;
    static constexpr uint8_t kRegMonth       = 0x08;
    static constexpr uint8_t kRegYear        = 0x09;
    static constexpr uint8_t kRegStatusA     = 0x0A;
    static constexpr uint8_t kRegStatusB     = 0x0B;
    static constexpr uint8_t kRegStatusC     = 0x0C;
    static constexpr uint8_t kRegStatusD     = 0x0D;
    static constexpr uint8_t kRegCentury     = 0x32;

    static uint8_t ToBcd(int val);
};
