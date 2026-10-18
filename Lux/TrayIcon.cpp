#include "TrayIcon.h"
#include "Utils.h"

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
    Remove();
}

HICON TrayIcon::CreateTablerSunIcon(int size, bool darkLines) {
    if (size < 16) size = 16;
    if (size > 48) size = 32;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC hdcScreen = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP hBmpColor = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdcScreen);

    if (!hBmpColor || !bits) return nullptr;

    struct Pixel { BYTE b, g, r, a; };
    Pixel* pBuf = static_cast<Pixel*>(bits);
    memset(pBuf, 0, size * size * sizeof(Pixel));

    float cx = size / 2.0f;
    float cy = size / 2.0f;
    float scale = size / 24.0f;

    float rCircle = 4.0f * scale;
    float rRayStart = 7.2f * scale;
    float rRayEnd = 9.8f * scale;
    float halfStroke = 1.0f * scale;

    float lineVal = darkLines ? 24.0f : 255.0f;

    struct Segment { float x1, y1, x2, y2; };
    Segment rays[8];
    for (int i = 0; i < 8; ++i) {
        float angle = i * 3.14159265f / 4.0f;
        rays[i] = {
            cx + rRayStart * std::cos(angle),
            cy + rRayStart * std::sin(angle),
            cx + rRayEnd   * std::cos(angle),
            cy + rRayEnd   * std::sin(angle)
        };
    }

    auto distToSegment = [](float px, float py, const Segment& s) {
        float dx = s.x2 - s.x1;
        float dy = s.y2 - s.y1;
        float lenSq = dx * dx + dy * dy;
        if (lenSq < 1e-4f) {
            float ex = px - s.x1, ey = py - s.y1;
            return std::sqrt(ex * ex + ey * ey);
        }
        float t = ((px - s.x1) * dx + (py - s.y1) * dy) / lenSq;
        t = std::clamp(t, 0.0f, 1.0f);
        float projX = s.x1 + t * dx;
        float projY = s.y1 + t * dy;
        float ex = px - projX, ey = py - projY;
        return std::sqrt(ex * ex + ey * ey);
    };

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float px = x + 0.5f;
            float py = y + 0.5f;

            float dCenter = std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
            float dCircle = std::abs(dCenter - rCircle);
            float aCircle = std::clamp((halfStroke + 0.5f) - dCircle, 0.0f, 1.0f);

            float aRayMax = 0.0f;
            for (int i = 0; i < 8; ++i) {
                float dRay = distToSegment(px, py, rays[i]);
                float aRay = std::clamp((halfStroke + 0.5f) - dRay, 0.0f, 1.0f);
                if (aRay > aRayMax) aRayMax = aRay;
            }

            float alpha = std::max(aCircle, aRayMax);
            if (alpha > 0.01f) {
                Pixel& p = pBuf[y * size + x];
                p.a = static_cast<BYTE>(std::round(alpha * 255.0f));
                p.r = static_cast<BYTE>(std::round(lineVal * alpha));
                p.g = static_cast<BYTE>(std::round(lineVal * alpha));
                p.b = static_cast<BYTE>(std::round(lineVal * alpha));
            }
        }
    }

    HBITMAP hMask = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = hBmpColor;
    ii.hbmMask = hMask;
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hMask);
    DeleteObject(hBmpColor);
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
