#pragma once

#include "core/device/device.h"

// Minimal PCI Type 1 configuration mechanism stub (ports 0xCF8-0xCFF).
// Only accepts 32-bit access to CONFIG_ADDRESS (offset 0); all byte/word
// accesses return 0xFF so that the kernel's mechanism-#2 detection fails
// (mechanism #2 relies on byte access to 0xCF8/0xCFA returning 0x00).
// CONFIG_DATA (offset 4-7) always reads 0xFFFFFFFF (no device present).
class PciHostBridge : public Device {
public:
    static constexpr uint16_t kBasePort = 0xCF8;
    static constexpr uint16_t kRegCount = 8;

    void PioRead(uint16_t offset, uint8_t size, uint32_t* value) override {
        if (offset == 0 && size == 4) {
            *value = config_addr_;
            return;
        }
        *value = 0xFFFFFFFF;
    }

    void PioWrite(uint16_t offset, uint8_t size, uint32_t value) override {
        if (offset == 0 && size == 4)
            config_addr_ = value;
    }

private:
    uint32_t config_addr_ = 0;
};
