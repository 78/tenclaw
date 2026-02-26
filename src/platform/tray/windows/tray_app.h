#pragma once

#include <functional>
#include <string>
#include <windows.h>

class WindowsTrayApp {
public:
    WindowsTrayApp(std::string tooltip,
                   std::function<void()> on_open_main_window,
                   std::function<void()> on_exit_all);
    ~WindowsTrayApp();

    bool Init();
    int Run();
    void UpdateTooltip(const std::string& tooltip);
    void RequestExit();
    DWORD InitErrorCode() const { return init_error_code_; }
    const std::string& InitErrorStage() const { return init_error_stage_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void ShowContextMenu();

    HWND hwnd_ = nullptr;
    UINT tray_msg_id_ = WM_APP + 1;
    bool icon_added_ = false;
    DWORD init_error_code_ = ERROR_SUCCESS;
    std::string init_error_stage_;
    std::string tooltip_;
    std::function<void()> on_open_main_window_;
    std::function<void()> on_exit_all_;
};
