#include "TrayIcon.h"
#include "Utils.h"

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
    Remove();
}

HICON TrayIcon::CreateTablerSunIcon(int size, bool /*darkLines*/) {
    if (size < 16) size = 16;
    if (size > 48) size = 32;

    Gdiplus::Bitmap bmp(size, size, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&bmp);
        LuxUtils::DrawLuxLogo(g, 0.0f, 0.0f, static_cast<float>(size));
    }

    HICON hIcon = nullptr;
    bmp.GetHICON(&hIcon);
    return hIcon;
}

bool TrayIcon::Create(HWND hMsgWnd, bool lightTaskbar) {
    m_hMsgWnd = hMsgWnd;
    if (m_hTrayIcon) {
        DestroyIcon(m_hTrayIcon);
        m_hTrayIcon = nullptr;
    }

    int iconSize = GetSystemMetrics(SM_CXSMICON);
    m_hTrayIcon = CreateTablerSunIcon(iconSize, lightTaskbar);

    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = m_hMsgWnd;
    m_nid.uID = ID_TRAY_ICON;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = m_hTrayIcon;
    wcsncpy_s(m_nid.szTip, APP_NAME, _TRUNCATE);
    BOOL res = Shell_NotifyIconW(NIM_ADD, &m_nid);

    m_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &m_nid);
    return res != FALSE;
}

void TrayIcon::Update(bool lightTaskbar) {
    int iconSize = GetSystemMetrics(SM_CXSMICON);
    if (m_hTrayIcon) DestroyIcon(m_hTrayIcon);
    m_hTrayIcon = CreateTablerSunIcon(iconSize, lightTaskbar);
    m_nid.hIcon = m_hTrayIcon;
    m_nid.uFlags = NIF_ICON;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::Remove() {
    Shell_NotifyIconW(NIM_DELETE, &m_nid);
    if (m_hTrayIcon) {
        DestroyIcon(m_hTrayIcon);
        m_hTrayIcon = nullptr;
    }
}

void TrayIcon::ShowContextMenu(HWND hMsgWnd, bool isDarkMode) {
    LuxUtils::SetPreferredThemeMode(isDarkMode);
    LuxUtils::AllowDarkModeForWindow(hMsgWnd, isDarkMode);
    LuxUtils::FlushMenuThemes();

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, L"Abrir");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SETTINGS, L"Configuraci\u00F3n");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"Salir");

    SetMenuDefaultItem(hMenu, IDM_TRAY_OPEN, FALSE);

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hMsgWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hMsgWnd, nullptr);
    PostMessageW(hMsgWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}
