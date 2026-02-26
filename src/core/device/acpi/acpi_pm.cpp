#include "core/device/acpi/acpi_pm.h"

void AcpiPm::TriggerPowerButton() {
    pm1_sts_ |= (1u << 8);  // PWRBTN_STS
    pm1_en_  |= (1u << 8);  // Ensure PWRBTN_EN so kernel SCI handler sees status & enable
    if (sci_cb_) sci_cb_();
}

void AcpiPm::RaiseSci() {
    if ((pm1_sts_ & pm1_en_) && sci_cb_) {
        sci_cb_();
    }
}

void AcpiPm::PioRead(uint16_t offset, uint8_t size, uint32_t* value) {
    switch (offset) {
    case 0:
        if (size == 4) {
            *value = pm1_sts_ | (static_cast<uint32_t>(pm1_en_) << 16);
        } else {
            *value = pm1_sts_;
        }
        break;
    case 2:
        *value = pm1_en_;
        break;
    case 4:
        *value = pm1_cnt_;
        break;
    default:
        *value = 0;
        break;
    }
}

void AcpiPm::PioWrite(uint16_t offset, uint8_t size, uint32_t value) {
    switch (offset) {
    case 0:
        pm1_sts_ &= ~static_cast<uint16_t>(value);
        if (size == 4) {
            pm1_en_ = static_cast<uint16_t>(value >> 16);
        }
        break;
    case 2:
        pm1_en_ = static_cast<uint16_t>(value);
        break;
    case 4: {
        pm1_cnt_ = static_cast<uint16_t>(value) | 1u;
        if (value & (1u << 13)) {
            uint8_t slp_typ = (value >> 10) & 7;
            LOG_INFO("ACPI: SLP_EN set (SLP_TYP=%u)", slp_typ);
            if (slp_typ == kSlpTypS5 && shutdown_cb_) {
                LOG_INFO("ACPI: S5 power off requested");
                shutdown_cb_();
            }
        }
        break;
    }
    default:
        break;
    }
}
