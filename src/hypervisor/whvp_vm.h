#pragma once

#include "hypervisor/whvp_platform.h"
#include "core/vmm/types.h"
#include <memory>

namespace whvp {

class WhvpVm {
public:
    ~WhvpVm();

    static std::unique_ptr<WhvpVm> Create(uint32_t cpu_count);

    WHV_PARTITION_HANDLE Handle() const { return partition_; }

    bool MapMemory(GPA gpa, void* hva, uint64_t size,
                   WHV_MAP_GPA_RANGE_FLAGS flags);
    bool UnmapMemory(GPA gpa, uint64_t size);

    WhvpVm(const WhvpVm&) = delete;
    WhvpVm& operator=(const WhvpVm&) = delete;

private:
    WhvpVm() = default;
    WHV_PARTITION_HANDLE partition_ = nullptr;
};

} // namespace whvp
