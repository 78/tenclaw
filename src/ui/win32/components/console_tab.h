#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <string>

// Console Tab component: output display, input field and send button.
// Also manages per-VM ANSI-filtered text state.
class ConsoleTab {
public:
    static constexpr UINT kConsoleId  = 2004;
    static constexpr UINT kInputId   = 2005;
    static constexpr UINT kSendBtnId = 2006;

    ConsoleTab() = default;
    ~ConsoleTab() = default;

    void Create(HWND parent, HINSTANCE hinst, HFONT mono_font, HFONT ui_font);
    void Show(bool visible);
    void Layout(int px, int py, int pw, int ph);
    void SetEnabled(bool enabled);

    HWND input_handle() const { return console_in_; }

    // Set the full text in the output area (used when switching VMs).
    void SetText(const char* text);

    // Append already-filtered text to the output control, trimming if needed.
    void AppendText(const std::string& text);

    // ── Per-VM text state management ──

    struct TextState {
        std::string text;
        enum class AnsiPhase { kNormal, kEsc, kCsi } ansi = AnsiPhase::kNormal;
    };

    // Filter raw console data through ANSI stripper, append to state,
    // and return the displayable portion that was added.
    static std::string FilterAndAppend(TextState& state, const std::string& raw);

    static void ResetState(TextState& state);

private:
    static constexpr size_t kMaxLen  = 32 * 1024;
    static constexpr size_t kTrimAt  = 24 * 1024;

    HWND console_    = nullptr;
    HWND console_in_ = nullptr;
    HWND send_btn_   = nullptr;

    static LRESULT CALLBACK InputSubclass(
        HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
        UINT_PTR id, DWORD_PTR ref);
};
