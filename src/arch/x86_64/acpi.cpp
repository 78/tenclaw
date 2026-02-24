#include "arch/x86_64/acpi.h"
#include <cstring>

namespace x86 {

static uint8_t AcpiChecksum(const void* data, size_t len) {
    uint8_t sum = 0;
    auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; i++) sum += p[i];
    return static_cast<uint8_t>(-sum);
}

#pragma pack(push, 1)

struct AcpiRsdp {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
};
static_assert(sizeof(AcpiRsdp) == 36);

struct AcpiHeader {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    char     creator_id[4];
    uint32_t creator_revision;
};
static_assert(sizeof(AcpiHeader) == 36);

// MADT Local APIC entry
struct MadtLocalApic {
    uint8_t  type;        // 0 = Processor Local APIC
    uint8_t  length;      // 8
    uint8_t  processor_id;
    uint8_t  apic_id;
    uint32_t flags;       // bit 0 = enabled
};
static_assert(sizeof(MadtLocalApic) == 8);

// MADT I/O APIC entry
struct MadtIoApic {
    uint8_t  type;        // 1 = I/O APIC
    uint8_t  length;      // 12
    uint8_t  io_apic_id;
    uint8_t  reserved;
    uint32_t io_apic_address;
    uint32_t gsi_base;
};
static_assert(sizeof(MadtIoApic) == 12);

#pragma pack(pop)

GPA BuildAcpiTables(uint8_t* ram, uint32_t num_cpus) {
    // --- MADT ---
    uint8_t* madt_base = ram + AcpiLayout::kMadt;
    memset(madt_base, 0, 256);

    AcpiHeader* madt = reinterpret_cast<AcpiHeader*>(madt_base);
    memcpy(madt->signature, "APIC", 4);
    madt->revision = 3;
    memcpy(madt->oem_id, "TNCLAW", 6);
    memcpy(madt->oem_table_id, "TENCLAW ", 8);
    madt->oem_revision = 1;
    memcpy(madt->creator_id, "TNCL", 4);
    madt->creator_revision = 1;

    uint8_t* p = madt_base + sizeof(AcpiHeader);

    // MADT body: Local APIC address + flags
    *reinterpret_cast<uint32_t*>(p) = 0xFEE00000;  // LAPIC address
    p += 4;
    *reinterpret_cast<uint32_t*>(p) = 0x00000001;  // Flags: PCAT_COMPAT
    p += 4;

    // Local APIC entries (one per CPU)
    for (uint32_t i = 0; i < num_cpus; i++) {
        auto* entry = reinterpret_cast<MadtLocalApic*>(p);
        entry->type = 0;
        entry->length = sizeof(MadtLocalApic);
        entry->processor_id = static_cast<uint8_t>(i);
        entry->apic_id = static_cast<uint8_t>(i);
        entry->flags = 1; // enabled
        p += sizeof(MadtLocalApic);
    }

    // I/O APIC entry at standard address 0xFEC00000
    auto* ioapic = reinterpret_cast<MadtIoApic*>(p);
    ioapic->type = 1;
    ioapic->length = sizeof(MadtIoApic);
    ioapic->io_apic_id = static_cast<uint8_t>(num_cpus);
    ioapic->reserved = 0;
    ioapic->io_apic_address = 0xFEC00000;
    ioapic->gsi_base = 0;
    p += sizeof(MadtIoApic);

    madt->length = static_cast<uint32_t>(p - madt_base);
    madt->checksum = AcpiChecksum(madt_base, madt->length);

    // --- XSDT ---
    uint8_t* xsdt_base = ram + AcpiLayout::kXsdt;
    memset(xsdt_base, 0, 128);

    AcpiHeader* xsdt = reinterpret_cast<AcpiHeader*>(xsdt_base);
    memcpy(xsdt->signature, "XSDT", 4);
    xsdt->revision = 1;
    memcpy(xsdt->oem_id, "TNCLAW", 6);
    memcpy(xsdt->oem_table_id, "TENCLAW ", 8);
    xsdt->oem_revision = 1;
    memcpy(xsdt->creator_id, "TNCL", 4);
    xsdt->creator_revision = 1;

    // One entry: pointer to MADT
    uint64_t* xsdt_entries = reinterpret_cast<uint64_t*>(
        xsdt_base + sizeof(AcpiHeader));
    xsdt_entries[0] = AcpiLayout::kMadt;

    xsdt->length = sizeof(AcpiHeader) + sizeof(uint64_t);
    xsdt->checksum = AcpiChecksum(xsdt_base, xsdt->length);

    // --- RSDP ---
    uint8_t* rsdp_base = ram + AcpiLayout::kRsdp;
    memset(rsdp_base, 0, sizeof(AcpiRsdp));

    AcpiRsdp* rsdp = reinterpret_cast<AcpiRsdp*>(rsdp_base);
    memcpy(rsdp->signature, "RSD PTR ", 8);
    memcpy(rsdp->oem_id, "TNCLAW", 6);
    rsdp->revision = 2;  // ACPI 2.0+
    rsdp->rsdt_address = 0;
    rsdp->length = sizeof(AcpiRsdp);
    rsdp->xsdt_address = AcpiLayout::kXsdt;

    // Checksum for first 20 bytes (ACPI 1.0 portion)
    rsdp->checksum = AcpiChecksum(rsdp_base, 20);
    // Extended checksum for all 36 bytes
    rsdp->extended_checksum = AcpiChecksum(rsdp_base, sizeof(AcpiRsdp));

    LOG_INFO("ACPI tables built: RSDP@0x%llX XSDT@0x%llX MADT@0x%llX",
             AcpiLayout::kRsdp, AcpiLayout::kXsdt, AcpiLayout::kMadt);

    return AcpiLayout::kRsdp;
}

} // namespace x86
