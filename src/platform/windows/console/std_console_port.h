#pragma once

#include "common/ports.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mutex>

class StdConsolePort final : public ConsolePort {
public:
    StdConsolePort();
    ~StdConsolePort() override;

    void Write(const uint8_t* data, size_t size) override;
    size_t Read(uint8_t* out, size_t size) override;

private:
    HANDLE stdin_ = INVALID_HANDLE_VALUE;
    HANDLE stdout_ = INVALID_HANDLE_VALUE;
    bool is_console_in_ = false;
    bool is_console_out_ = false;
    DWORD old_in_mode_ = 0;
    DWORD old_out_mode_ = 0;
    UINT old_input_cp_ = 0;
    UINT old_output_cp_ = 0;
    std::mutex write_mutex_;
};
