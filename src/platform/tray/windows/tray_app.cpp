#include "platform/tray/windows/tray_app.h"

#include <windows.h>
#include <shellapi.h>
#include <cstring>

namespace {

constexpr UINT kMenuOpenMainWindow = 1001;
constexpr UINT kMenuExitAll = 1002;

}  // namespace

WindowsTrayApp::WindowsTrayApp(std::string tooltip,
                               std::function<void()> on_open_main_window,
                               std::function<void()> on_exit_all)
    : tooltip_(std::move(tooltip)),
      on_open_main_window_(std::move(on_open_main_window)),
      on_exit_all_(std::move(on_exit_all)) {}

WindowsTrayApp::~WindowsTrayApp() {
    if (icon_added_) {
        NOTIFYICONDATAA nid{};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd_;
        nid.uID = 1;
        Shell_NotifyIconA(NIM_DELETE, &nid);
    }
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool WindowsTrayApp::Init() {
    init_error_code_ = ERROR_SUCCESS;
    init_error_stage_.clear();

    WNDCLASSA wc{};
    wc.lpfnWndProc = &WindowsTrayApp::WndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "TenBoxManagerTrayWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&wc);

    hwnd_ = CreateWindowExA(
        0,
        wc.lpszClassName,
        "TenBoxTray",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr,
        nullptr,
        wc.hInstance,
        this);
    if (!hwnd_) {
        init_error_stage_ = "CreateWindowExA";
        init_error_code_ = GetLastError();
        return false;
    }

    NOTIFYICONDATAA nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = tray_msg_id_;
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    std::strncpy(nid.szTip, tooltip_.c_str(), sizeof(nid.szTip) - 1);
    if (!Shell_NotifyIconA(NIM_ADD, &nid)) {
        init_error_stage_ = "Shell_NotifyIconA(NIM_ADD)";
        init_error_code_ = GetLastError();
        return false;
    }
    icon_added_ = true;
    return true;
}

int WindowsTrayApp::Run() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void WindowsTrayApp::UpdateTooltip(const std::string& tooltip) {
    tooltip_ = tooltip;
    if (!icon_added_) return;
    NOTIFYICONDATAA nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = 1;
    nid.uFlags = NIF_TIP;
    std::strncpy(nid.szTip, tooltip_.c_str(), sizeof(nid.szTip) - 1);
    Shell_NotifyIconA(NIM_MODIFY, &nid);
}

void WindowsTrayApp::RequestExit() {
    if (hwnd_) {
        PostMessageA(hwnd_, WM_CLOSE, 0, 0);
    }
}

LRESULT CALLBACK WindowsTrayApp::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    WindowsTrayApp* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = static_cast<WindowsTrayApp*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<WindowsTrayApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProc(hwnd, msg, wp, lp);
    return self->OnMessage(hwnd, msg, wp, lp);
}

LRESULT WindowsTrayApp::OnMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == tray_msg_id_) {
        if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) {
            ShowContextMenu();
        } else if (lp == WM_LBUTTONDBLCLK) {
            if (on_open_main_window_) on_open_main_window_();
        }
        return 0;
    }

    if (msg == WM_COMMAND) {
        switch (LOWORD(wp)) {
        case kMenuOpenMainWindow:
            if (on_open_main_window_) on_open_main_window_();
            return 0;
        case kMenuExitAll:
            if (on_exit_all_) on_exit_all_();
            RequestExit();
            return 0;
        default:
            return 0;
        }
    }

    if (msg == WM_DESTROY) {
        hwnd_ = nullptr;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void WindowsTrayApp::ShowContextMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuA(menu, MF_STRING, kMenuOpenMainWindow, "Open Main Window");
    AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(menu, MF_STRING, kMenuExitAll, "Exit All And Quit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}
