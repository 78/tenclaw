#pragma once

#include "vmm/types.h"

namespace x86 {

// Guest physical addresses for ACPI tables
namespace AcpiLayout {
    constexpr GPA kRsdp = 0x4000;
    constexpr GPA kXsdt = 0x4100;
    constexpr GPA kMadt = 0x4200;
}

// Build minimal ACPI tables (RSDP, XSDT, MADT) in guest RAM.
// Returns the GPA of the RSDP for boot_params.acpi_rsdp_addr.
GPA BuildAcpiTables(uint8_t* ram, uint32_t num_cpus);

} // namespace x86
