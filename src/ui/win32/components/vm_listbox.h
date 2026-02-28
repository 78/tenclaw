#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <vector>

struct VmRecord;

// Owner-draw ListBox showing VMs as multi-line cards.
class VmListBox {
public:
    static constexpr UINT kControlId = 2003;
    static constexpr int kItemHeight = 80;

    VmListBox() = default;
    ~VmListBox() = default;

    void Create(HWND parent, HINSTANCE hinst, HFONT ui_font);
    HWND handle() const { return hwnd_; }

    // Repopulate the list from the records vector.
    void Populate(const std::vector<VmRecord>& records, int selected_index);

    // Handle WM_MEASUREITEM. Returns true if handled.
    bool HandleMeasureItem(MEASUREITEMSTRUCT* mis);

    // Handle WM_DRAWITEM. Returns true if handled.
    bool HandleDrawItem(DRAWITEMSTRUCT* dis, const std::vector<VmRecord>& records, HFONT ui_font);

private:
    HWND hwnd_ = nullptr;
};
