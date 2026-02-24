#pragma once

#include "hypervisor/whvp_vm.h"
#include "vmm/address_space.h"

namespace whvp {

enum class VCpuExitAction {
    kContinue,
    kHalt,
    kShutdown,
    kError,
};

class WhvpVCpu {
public:
    ~WhvpVCpu();

    static std::unique_ptr<WhvpVCpu> Create(WhvpVm& vm, uint32_t vp_index,
                                             AddressSpace* addr_space);

    VCpuExitAction RunOnce();

    bool SetRegisters(const WHV_REGISTER_NAME* names,
                      const WHV_REGISTER_VALUE* values, uint32_t count);
    bool GetRegisters(const WHV_REGISTER_NAME* names,
                      WHV_REGISTER_VALUE* values, uint32_t count);

    WHV_PARTITION_HANDLE Partition() const { return partition_; }
    uint32_t VpIndex() const { return vp_index_; }

    WhvpVCpu(const WhvpVCpu&) = delete;
    WhvpVCpu& operator=(const WhvpVCpu&) = delete;

private:
    WhvpVCpu() = default;

    bool CreateEmulator();

    VCpuExitAction HandleIoPort(const WHV_VP_EXIT_CONTEXT& vp_ctx,
                                 const WHV_X64_IO_PORT_ACCESS_CONTEXT& io);
    VCpuExitAction HandleMmio(const WHV_VP_EXIT_CONTEXT& vp_ctx,
                               const WHV_MEMORY_ACCESS_CONTEXT& mem);

    static HRESULT CALLBACK OnIoPort(
        VOID* ctx, WHV_EMULATOR_IO_ACCESS_INFO* io);
    static HRESULT CALLBACK OnMemory(
        VOID* ctx, WHV_EMULATOR_MEMORY_ACCESS_INFO* mem);
    static HRESULT CALLBACK OnGetRegisters(
        VOID* ctx, const WHV_REGISTER_NAME* names,
        UINT32 count, WHV_REGISTER_VALUE* values);
    static HRESULT CALLBACK OnSetRegisters(
        VOID* ctx, const WHV_REGISTER_NAME* names,
        UINT32 count, const WHV_REGISTER_VALUE* values);
    static HRESULT CALLBACK OnTranslateGva(
        VOID* ctx, WHV_GUEST_VIRTUAL_ADDRESS gva,
        WHV_TRANSLATE_GVA_FLAGS flags,
        WHV_TRANSLATE_GVA_RESULT_CODE* result,
        WHV_GUEST_PHYSICAL_ADDRESS* gpa);

    WHV_PARTITION_HANDLE partition_ = nullptr;
    uint32_t vp_index_ = 0;
    AddressSpace* addr_space_ = nullptr;
    WHV_EMULATOR_HANDLE emulator_ = nullptr;
};

} // namespace whvp
