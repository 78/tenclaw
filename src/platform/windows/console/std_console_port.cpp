#include "platform/windows/console/std_console_port.h"

#include <algorithm>
#include <cstring>

StdConsolePort::StdConsolePort() {
    stdin_ = GetStdHandle(STD_INPUT_HANDLE);
    stdout_ = GetStdHandle(STD_OUTPUT_HANDLE);

    is_console_in_ = (stdin_ != INVALID_HANDLE_VALUE) &&
                     GetConsoleMode(stdin_, &old_in_mode_);
    is_console_out_ = (stdout_ != INVALID_HANDLE_VALUE) &&
                      GetConsoleMode(stdout_, &old_out_mode_);

    old_input_cp_ = GetConsoleCP();
    old_output_cp_ = GetConsoleOutputCP();

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    if (is_console_in_) {
        SetConsoleMode(stdin_, ENABLE_WINDOW_INPUT);
    }
    if (is_console_out_) {
        SetConsoleMode(stdout_,
            old_out_mode_ | ENABLE_PROCESSED_OUTPUT |
            ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}

StdConsolePort::~StdConsolePort() {
    if (is_console_in_) {
        SetConsoleMode(stdin_, old_in_mode_);
    }
    if (is_console_out_) {
        SetConsoleMode(stdout_, old_out_mode_);
    }
    SetConsoleCP(old_input_cp_);
    SetConsoleOutputCP(old_output_cp_);
}

void StdConsolePort::Write(const uint8_t* data, size_t size) {
    if (!data || size == 0 || stdout_ == INVALID_HANDLE_VALUE) {
        return;
    }

    std::lock_guard<std::mutex> lock(write_mutex_);
    DWORD written = 0;
    WriteFile(stdout_, data, static_cast<DWORD>(size), &written, nullptr);
}

size_t StdConsolePort::Read(uint8_t* out, size_t size) {
    if (!out || size == 0 || stdin_ == INVALID_HANDLE_VALUE) {
        return 0;
    }

    if (is_console_in_) {
        DWORD avail = 0;
        if (!GetNumberOfConsoleInputEvents(stdin_, &avail) || avail == 0) {
            Sleep(16);
            return 0;
        }

        INPUT_RECORD rec{};
        DWORD read_count = 0;
        if (!ReadConsoleInput(stdin_, &rec, 1, &read_count) || read_count == 0) {
            return 0;
        }

        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) {
            return 0;
        }

        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        const char* seq = nullptr;
        switch (vk) {
        case VK_UP:     seq = "\x1b[A"; break;
        case VK_DOWN:   seq = "\x1b[B"; break;
        case VK_RIGHT:  seq = "\x1b[C"; break;
        case VK_LEFT:   seq = "\x1b[D"; break;
        case VK_HOME:   seq = "\x1b[H"; break;
        case VK_END:    seq = "\x1b[F"; break;
        case VK_INSERT: seq = "\x1b[2~"; break;
        case VK_DELETE: seq = "\x1b[3~"; break;
        case VK_PRIOR:  seq = "\x1b[5~"; break;
        case VK_NEXT:   seq = "\x1b[6~"; break;
        case VK_F1:     seq = "\x1bOP"; break;
        case VK_F2:     seq = "\x1bOQ"; break;
        case VK_F3:     seq = "\x1bOR"; break;
        case VK_F4:     seq = "\x1bOS"; break;
        case VK_F5:     seq = "\x1b[15~"; break;
        case VK_F6:     seq = "\x1b[17~"; break;
        case VK_F7:     seq = "\x1b[18~"; break;
        case VK_F8:     seq = "\x1b[19~"; break;
        case VK_F9:     seq = "\x1b[20~"; break;
        case VK_F10:    seq = "\x1b[21~"; break;
        case VK_F11:    seq = "\x1b[23~"; break;
        case VK_F12:    seq = "\x1b[24~"; break;
        default:
            break;
        }

        if (seq) {
            size_t seq_len = std::strlen(seq);
            size_t n = (size < seq_len) ? size : seq_len;
            std::memcpy(out, seq, n);
            return n;
        }

        char ch = rec.Event.KeyEvent.uChar.AsciiChar;
        if (ch == 0) {
            return 0;
        }
        out[0] = static_cast<uint8_t>(ch);
        return 1;
    }

    DWORD bytes_read = 0;
    if (ReadFile(stdin_, out, static_cast<DWORD>(size), &bytes_read, nullptr) &&
        bytes_read > 0) {
        return static_cast<size_t>(bytes_read);
    }

    Sleep(16);
    return 0;
}
