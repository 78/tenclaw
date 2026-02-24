cmake_minimum_required(VERSION 3.20)
project(TenClaw LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.cpp
    src/vmm/vm.cpp
    src/vmm/address_space.cpp
    src/hypervisor/whvp_platform.cpp
    src/hypervisor/whvp_vm.cpp
    src/hypervisor/whvp_vcpu.cpp
    src/arch/x86_64/boot.cpp
    src/device/serial/uart_16550.cpp
    src/device/timer/i8254_pit.cpp
    src/device/rtc/cmos_rtc.cpp
    src/device/irq/ioapic.cpp
    src/arch/x86_64/acpi.cpp
)

add_executable(TenClaw ${SOURCES})
target_include_directories(TenClaw PRIVATE src/)

target_link_libraries(TenClaw PRIVATE WinHvPlatform WinHvEmulation)
