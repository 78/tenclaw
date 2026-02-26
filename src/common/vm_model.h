#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct PortForward {
    uint16_t host_port;
    uint16_t guest_port;
};

enum class VmPowerState : uint8_t {
    kStopped = 0,
    kStarting = 1,
    kRunning = 2,
    kStopping = 3,
    kCrashed = 4,
};

struct VmSpec {
    std::string name;
    std::string vm_id;       // UUID derived from directory name
    std::string vm_dir;      // absolute path to this VM's directory
    std::string kernel_path; // absolute at runtime, relative in vm.json
    std::string initrd_path;
    std::string disk_path;
    std::string cmdline;
    uint64_t memory_mb = 4096;
    uint32_t cpu_count = 4;
    bool nat_enabled = false;
    std::vector<PortForward> port_forwards;
};

struct VmMutablePatch {
    std::optional<std::string> name;
    std::optional<bool> nat_enabled;
    std::optional<std::vector<PortForward>> port_forwards;
    std::optional<uint64_t> memory_mb;
    std::optional<uint32_t> cpu_count;
    bool apply_on_next_boot = false;
};
