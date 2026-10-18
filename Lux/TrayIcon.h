#pragma once
#include "Common.h"

class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    bool Create(HWND hMsgWnd, bool lightTaskbar);
    void Update(bool lightTaskbar);
    void Remove();

    void ShowContextMenu(HWND hMsgWnd, bool isDarkMode);

    static HICON CreateTablerSunIcon(int size, bool darkLines);

private:
    NOTIFYICONDATAW m_nid{};
    HICON m_hTrayIcon = nullptr;
    HWND m_hMsgWnd = nullptr;
};
