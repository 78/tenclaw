#include "hypervisor/whvp_platform.h"
#include "core/vmm/types.h"

namespace whvp {

bool IsHypervisorPresent() {
    WHV_CAPABILITY capability{};
    UINT32 size = 0;
    HRESULT hr = WHvGetCapability(
        WHvCapabilityCodeHypervisorPresent,
        &capability, sizeof(capability), &size);
    if (FAILED(hr)) {
        LOG_ERROR("WHvGetCapability failed: 0x%08lX", hr);
        return false;
    }
    return capability.HypervisorPresent != FALSE;
}

} // namespace whvp
