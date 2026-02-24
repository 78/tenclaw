#include "hypervisor/whvp_vm.h"
#include <intrin.h>

namespace whvp {

WhvpVm::~WhvpVm() {
    if (partition_) {
        WHvDeletePartition(partition_);
        partition_ = nullptr;
    }
}

std::unique_ptr<WhvpVm> WhvpVm::Create(uint32_t cpu_count) {
    auto vm = std::unique_ptr<WhvpVm>(new WhvpVm());

    HRESULT hr = WHvCreatePartition(&vm->partition_);
    if (FAILED(hr)) {
        LOG_ERROR("WHvCreatePartition failed: 0x%08lX", hr);
        return nullptr;
    }

    WHV_PARTITION_PROPERTY prop{};
    prop.ProcessorCount = cpu_count;
    hr = WHvSetPartitionProperty(vm->partition_,
        WHvPartitionPropertyCodeProcessorCount,
        &prop, sizeof(prop.ProcessorCount));
    if (FAILED(hr)) {
        LOG_ERROR("Set ProcessorCount failed: 0x%08lX", hr);
        return nullptr;
    }

    memset(&prop, 0, sizeof(prop));
    prop.LocalApicEmulationMode = WHvX64LocalApicEmulationModeXApic;
    hr = WHvSetPartitionProperty(vm->partition_,
        WHvPartitionPropertyCodeLocalApicEmulationMode,
        &prop, sizeof(prop.LocalApicEmulationMode));
    if (FAILED(hr)) {
        LOG_WARN("Set APIC emulation failed: 0x%08lX (non-fatal)", hr);
    }

    // Query WHVP clock frequencies for diagnostics.
    uint64_t proc_freq = 0, intr_freq = 0;
    hr = WHvGetCapability(WHvCapabilityCodeProcessorClockFrequency,
                          &proc_freq, sizeof(proc_freq), nullptr);
    if (SUCCEEDED(hr) && proc_freq) {
        LOG_INFO("WHVP ProcessorClockFrequency: %llu Hz", proc_freq);
    }
    hr = WHvGetCapability(WHvCapabilityCodeInterruptClockFrequency,
                          &intr_freq, sizeof(intr_freq), nullptr);
    if (SUCCEEDED(hr) && intr_freq) {
        LOG_INFO("WHVP InterruptClockFrequency: %llu Hz", intr_freq);
    }

    // Build CPUID override list: leaf 0x15 (TSC freq) + leaf 1 (features).
    WHV_X64_CPUID_RESULT cpuid_overrides[2]{};
    int num_overrides = 0;

    // Override CPUID 0x15 so the kernel can determine TSC speed without
    // unreliable PIT calibration.
    int cpuid15[4]{};
    __cpuid(cpuid15, 0x15);
    uint32_t denom   = static_cast<uint32_t>(cpuid15[0]);
    uint32_t numer   = static_cast<uint32_t>(cpuid15[1]);
    uint32_t crystal  = static_cast<uint32_t>(cpuid15[2]);

    if (denom && numer) {
        if (crystal == 0) crystal = 38400000; // 38.4 MHz for modern Intel
        auto& o = cpuid_overrides[num_overrides++];
        o.Function = 0x15;
        o.Eax = denom;
        o.Ebx = numer;
        o.Ecx = crystal;
        o.Edx = 0;
        uint64_t tsc_freq = static_cast<uint64_t>(crystal) * numer / denom;
        LOG_INFO("CPUID 0x15 override: crystal=%u Hz, TSC=%llu Hz",
                 crystal, tsc_freq);
    }

    // Override CPUID leaf 1 to mask features WHVP doesn't support:
    //   ECX bit  3: MONITOR/MWAIT  — causes #UD in WHVP
    //   ECX bit 24: TSC-Deadline   — WHVP xAPIC may not fire these
    int cpuid1[4]{};
    __cpuidex(cpuid1, 1, 0);
    {
        constexpr uint32_t kMaskOutEcx = (1u << 3) | (1u << 24);
        auto& o = cpuid_overrides[num_overrides++];
        o.Function = 1;
        o.Eax = static_cast<uint32_t>(cpuid1[0]);
        o.Ebx = static_cast<uint32_t>(cpuid1[1]);
        o.Ecx = static_cast<uint32_t>(cpuid1[2]) & ~kMaskOutEcx;
        o.Edx = static_cast<uint32_t>(cpuid1[3]);
        LOG_INFO("CPUID 1 override: ECX 0x%08X -> 0x%08X (masked MWAIT+TSC-deadline)",
                 static_cast<uint32_t>(cpuid1[2]), o.Ecx);
    }

    if (num_overrides > 0) {
        hr = WHvSetPartitionProperty(vm->partition_,
            WHvPartitionPropertyCodeCpuidResultList,
            cpuid_overrides,
            num_overrides * sizeof(WHV_X64_CPUID_RESULT));
        if (FAILED(hr)) {
            LOG_WARN("CpuidResultList failed: 0x%08lX (non-fatal)", hr);
        }
    }

    hr = WHvSetupPartition(vm->partition_);
    if (FAILED(hr)) {
        LOG_ERROR("WHvSetupPartition failed: 0x%08lX", hr);
        return nullptr;
    }

    LOG_INFO("WHVP partition created (cpus=%u)", cpu_count);
    return vm;
}

bool WhvpVm::MapMemory(GPA gpa, void* hva, uint64_t size,
                        WHV_MAP_GPA_RANGE_FLAGS flags) {
    HRESULT hr = WHvMapGpaRange(partition_, hva, gpa, size, flags);
    if (FAILED(hr)) {
        LOG_ERROR("WHvMapGpaRange(gpa=0x%llX, size=0x%llX) failed: 0x%08lX",
                  gpa, size, hr);
        return false;
    }
    return true;
}

bool WhvpVm::UnmapMemory(GPA gpa, uint64_t size) {
    HRESULT hr = WHvUnmapGpaRange(partition_, gpa, size);
    if (FAILED(hr)) {
        LOG_ERROR("WHvUnmapGpaRange failed: 0x%08lX", hr);
        return false;
    }
    return true;
}

} // namespace whvp
