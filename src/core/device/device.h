#pragma once

#include "core/vmm/types.h"

class Device {
public:
    virtual ~Device() = default;

    virtual void PioRead(uint16_t offset, uint8_t size, uint32_t* value) {
        *value = 0xFFFFFFFF;
    }

    virtual void PioWrite(uint16_t offset, uint8_t size, uint32_t value) {
    }

    virtual void MmioRead(uint64_t offset, uint8_t size, uint64_t* value) {
        *value = 0;
    }

    virtual void MmioWrite(uint64_t offset, uint8_t size, uint64_t value) {
    }
};
