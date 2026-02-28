#include "ui/win32/components/vm_listbox.h"
#include "manager/manager_service.h"
#include "ui/common/i18n.h"

static const char* StateText(VmPowerState s) {
    using S = i18n::S;
    switch (s) {
    case VmPowerState::kRunning:  return i18n::tr(S::kStateRunning);
    case VmPowerState::kStarting: return i18n::tr(S::kStateStarting);
    case VmPowerState::kStopping: return i18n::tr(S::kStateStopping);
    case VmPowerState::kCrashed:  return i18n::tr(S::kStateCrashed);
    default:                      return i18n::tr(S::kStateStopped);
    }
}

void VmListBox::Create(HWND parent, HINSTANCE hinst, HFONT ui_font) {
    hwnd_ = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL |
        LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
        0, 0, 0, 0, parent,
        reinterpret_cast<HMENU>(kControlId), hinst, nullptr);
    SendMessageA(hwnd_, LB_SETITEMHEIGHT, 0, MAKELPARAM(kItemHeight, 0));
    SendMessage(hwnd_, WM_SETFONT,
        reinterpret_cast<WPARAM>(ui_font), FALSE);
}

void VmListBox::Populate(const std::vector<VmRecord>& records, int selected_index) {
    SendMessageA(hwnd_, WM_SETREDRAW, FALSE, 0);
    SendMessageA(hwnd_, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < static_cast<int>(records.size()); ++i) {
        SendMessageA(hwnd_, LB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(records[i].spec.name.c_str()));
    }
    if (selected_index >= 0 && selected_index < static_cast<int>(records.size())) {
        SendMessageA(hwnd_, LB_SETCURSEL, selected_index, 0);
    }
    SendMessageA(hwnd_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

bool VmListBox::HandleMeasureItem(MEASUREITEMSTRUCT* mis) {
    if (mis->CtlID == kControlId) {
        mis->itemHeight = kItemHeight;
        return true;
    }
    return false;
}

bool VmListBox::HandleDrawItem(DRAWITEMSTRUCT* dis, const std::vector<VmRecord>& records, HFONT ui_font) {
    if (dis->CtlID != kControlId || dis->itemID == static_cast<UINT>(-1))
        return false;

    int idx = static_cast<int>(dis->itemID);
    if (idx < 0 || idx >= static_cast<int>(records.size()))
        return false;

    const auto& rec = records[idx];

    bool selected = (dis->itemState & ODS_SELECTED) != 0;
    COLORREF card_bg = selected ? RGB(229, 241, 255) : RGB(248, 248, 248);
    COLORREF fg  = selected ? RGB(20, 20, 20) : GetSysColor(COLOR_WINDOWTEXT);
    COLORREF dim = selected ? RGB(80, 80, 80) : GetSysColor(COLOR_GRAYTEXT);

    HBRUSH wnd_br = GetSysColorBrush(COLOR_WINDOW);
    FillRect(dis->hDC, &dis->rcItem, wnd_br);

    RECT card = dis->rcItem;
    card.left   += 4;
    card.right  -= 4;
    card.top    += 3;
    card.bottom -= 3;

    HBRUSH card_br = CreateSolidBrush(card_bg);
    HPEN border_pen = CreatePen(PS_SOLID, 1,
        selected ? RGB(100, 160, 230) : RGB(232, 232, 232));
    HPEN old_pen = static_cast<HPEN>(SelectObject(dis->hDC, border_pen));
    HBRUSH old_br = static_cast<HBRUSH>(SelectObject(dis->hDC, card_br));
    RoundRect(dis->hDC, card.left, card.top, card.right, card.bottom, 6, 6);
    SelectObject(dis->hDC, old_br);
    SelectObject(dis->hDC, old_pen);
    DeleteObject(card_br);
    DeleteObject(border_pen);

    SetBkMode(dis->hDC, TRANSPARENT);

    HFONT old_font = static_cast<HFONT>(SelectObject(dis->hDC, ui_font));

    TEXTMETRICA tm{};
    GetTextMetricsA(dis->hDC, &tm);
    int line_h = tm.tmHeight + tm.tmExternalLeading;

    int x = card.left + 12;
    int y = card.top + 8;

    SetTextColor(dis->hDC, fg);
    auto name_w = i18n::to_wide(rec.spec.name);
    TextOutW(dis->hDC, x, y, name_w.c_str(), static_cast<int>(name_w.size()));

    SIZE name_sz{};
    GetTextExtentPoint32W(dis->hDC, name_w.c_str(),
        static_cast<int>(name_w.size()), &name_sz);

    auto state_w = i18n::to_wide(StateText(rec.state));
    COLORREF state_color;
    if (rec.state == VmPowerState::kRunning)
        state_color = RGB(0, 128, 0);
    else if (rec.state == VmPowerState::kCrashed)
        state_color = RGB(200, 0, 0);
    else
        state_color = dim;
    SetTextColor(dis->hDC, state_color);
    TextOutW(dis->hDC, x + name_sz.cx + 12, y, state_w.c_str(),
             static_cast<int>(state_w.size()));

    y += line_h + 2;
    SelectObject(dis->hDC, ui_font);
    SetTextColor(dis->hDC, dim);
    auto detail = i18n::fmt(i18n::S::kDetailVcpuRam,
        static_cast<unsigned>(rec.spec.cpu_count), static_cast<unsigned>(rec.spec.memory_mb));
    auto detail_w = i18n::to_wide(detail);
    TextOutW(dis->hDC, x, y, detail_w.c_str(), static_cast<int>(detail_w.size()));

    SelectObject(dis->hDC, old_font);
    return true;
}
