#include "LuxApp.h"
#include "Utils.h"
#include "Updater.h"

static const int BASE_PANEL_WIDTH = 340;
static const int BASE_ROW_HEIGHT  = 74;

bool LuxApp::Init(HINSTANCE hInst) {
    m_hInst = hInst;

    // Limpiar restos de actualizaciones anteriores (.old / .new)
    Updater::CleanOldFiles();

    m_msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    // Activar modo de alto rendimiento y prioridad en Windows
    LuxUtils::ConfigureWindowsPerformanceMode();

    // Inicializar GDI+ con antialiasing nativo
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr);

    // Caché de fuentes GDI+
    Gdiplus::FontFamily fontFam(L"Segoe UI");
    m_pFontTitle = new Gdiplus::Font(&fontFam, 10.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    m_pFontSmall = new Gdiplus::Font(&fontFam, 9.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

    LuxUtils::InitDarkModeAPIs();
    m_wmi.Init();
    m_monitorMgr.Init(&m_wmi);

    m_darkMode = m_settings.IsDarkEffective();
    LuxUtils::SetPreferredThemeMode(m_darkMode);
    LuxUtils::FlushMenuThemes();

#include "Resource.h"

    HICON hAppIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));

    // Ventana oculta de mensajes
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = &LuxApp::MsgWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = MSGWND_CLASS;
    wc.hIcon = hAppIcon;
    wc.hIconSm = hAppIcon;
    RegisterClassExW(&wc);

    m_hMsgWnd = CreateWindowExW(0, MSGWND_CLASS, APP_NAME, WS_POPUP,
                                 0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);
    if (!m_hMsgWnd) return false;

    LuxUtils::AllowDarkModeForWindow(m_hMsgWnd, m_darkMode);
    SetWindowTheme(m_hMsgWnd, m_darkMode ? L"DarkMode_Explorer" : L"Explorer", nullptr);

    // Clase del panel flotante
    WNDCLASSEXW wcPanel{ sizeof(wcPanel) };
    wcPanel.style = CS_DROPSHADOW;
    wcPanel.lpfnWndProc = &LuxApp::PanelWndProc;
    wcPanel.hInstance = hInst;
    wcPanel.lpszClassName = PANELWND_CLASS;
    wcPanel.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcPanel.hIcon = hAppIcon;
    wcPanel.hIconSm = hAppIcon;
    RegisterClassExW(&wcPanel);

    m_hPanelWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                   PANELWND_CLASS, L"", WS_POPUP,
                                   0, 0, 340, 200, nullptr, nullptr, hInst, nullptr);

    if (m_hPanelWnd) {
        LuxUtils::AllowDarkModeForWindow(m_hPanelWnd, m_darkMode);
    }

    RebuildThemeBrush();
    ApplyTheme();

    if (m_settings.GetShowTrayIcon()) {
        m_trayIcon.Create(m_hMsgWnd, SettingsManager::IsTaskbarLightTheme());
    }

    LuxUtils::PurgeWorkingSet();
    return true;
}

void LuxApp::Shutdown() {
    m_trayIcon.Remove();
    m_monitorMgr.Shutdown();
    m_wmi.Shutdown();

    if (m_hbrThemeBg) {
        DeleteObject(m_hbrThemeBg);
        m_hbrThemeBg = nullptr;
    }
    if (m_pFontTitle) {
        delete m_pFontTitle;
        m_pFontTitle = nullptr;
    }
    if (m_pFontSmall) {
        delete m_pFontSmall;
        m_pFontSmall = nullptr;
    }
    if (m_gdiplusToken) {
        Gdiplus::GdiplusShutdown(m_gdiplusToken);
        m_gdiplusToken = 0;
    }
}

int LuxApp::Run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    Shutdown();
    return static_cast<int>(msg.wParam);
}

void LuxApp::RebuildThemeBrush() {
    if (m_hbrThemeBg) DeleteObject(m_hbrThemeBg);
    COLORREF bg = m_darkMode ? RGB(0x20, 0x20, 0x20) : RGB(0xF9, 0xF9, 0xF9);
    m_hbrThemeBg = CreateSolidBrush(bg);
}

void LuxApp::ApplyTheme() {
    m_darkMode = m_settings.IsDarkEffective();
    RebuildThemeBrush();

    LuxUtils::SetPreferredThemeMode(m_darkMode);
    LuxUtils::FlushMenuThemes();

    if (m_hMsgWnd) {
        LuxUtils::AllowDarkModeForWindow(m_hMsgWnd, m_darkMode);
        SetWindowTheme(m_hMsgWnd, m_darkMode ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    }

    if (m_hPanelWnd) {
        LuxUtils::AllowDarkModeForWindow(m_hPanelWnd, m_darkMode);
        BOOL dark = m_darkMode ? TRUE : FALSE;
        DwmSetWindowAttribute(m_hPanelWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        InvalidateRect(m_hPanelWnd, nullptr, FALSE);
    }

    m_settingsWnd.UpdateTheme(m_darkMode, m_hbrThemeBg);

    if (m_settings.GetShowTrayIcon()) {
        m_trayIcon.Update(SettingsManager::IsTaskbarLightTheme());
    }

    if (!IsWindowVisible(m_hPanelWnd) && !m_settingsWnd.IsVisible()) {
        LuxUtils::PurgeWorkingSet();
    }
}

void LuxApp::RefreshMonitors() {
    m_monitorMgr.RefreshMonitors();
    if (IsWindowVisible(m_hPanelWnd)) {
        PositionPanelNearTray();
        InvalidateRect(m_hPanelWnd, nullptr, FALSE);
    }
}

int LuxApp::CalcPanelHeight() const {
    int rows = static_cast<int>(m_monitorMgr.GetMonitorCount() == 0 ? 1 : m_monitorMgr.GetMonitorCount());
    int dpi = LuxUtils::GetDpiScale(m_hPanelWnd);
    float scale = (dpi > 0) ? (dpi / 96.0f) : 1.0f;
    int rowH = static_cast<int>(BASE_ROW_HEIGHT * scale);
    int footH = static_cast<int>(34 * scale);
    return static_cast<int>(14 * scale) + rows * rowH + static_cast<int>(10 * scale) + footH + static_cast<int>(12 * scale);
}

int LuxApp::ComputeValueFromX(int x, const RECT& track) const {
    if (track.right <= track.left) return 0;
    int v = static_cast<int>((static_cast<float>(x - track.left) / (track.right - track.left)) * 100.0f);
    return std::clamp(v, 0, 100);
}

void LuxApp::PositionPanelNearTray() {
    NOTIFYICONIDENTIFIER nii{ sizeof(nii) };
    nii.hWnd = m_hMsgWnd;
    nii.uID = ID_TRAY_ICON;
    RECT trayRc{};
    bool hasTrayRect = SUCCEEDED(Shell_NotifyIconGetRect(&nii, &trayRc));

    int dpi = LuxUtils::GetDpiScale(m_hPanelWnd);
    float scale = (dpi > 0) ? (dpi / 96.0f) : 1.0f;
    int w = static_cast<int>(BASE_PANEL_WIDTH * scale);
    int h = CalcPanelHeight();

    POINT pt;
    if (hasTrayRect) {
        pt.x = (trayRc.left + trayRc.right) / 2;
        pt.y = (trayRc.top + trayRc.bottom) / 2;
    } else {
        GetCursorPos(&pt);
    }

    HMONITOR hm = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(hm, &mi);

    int x = pt.x - w / 2;
    int y = pt.y - h - 12;

    if (mi.rcWork.bottom < mi.rcMonitor.bottom) {
        y = static_cast<int>(mi.rcWork.bottom - h - 12);
    } else if (mi.rcWork.top > mi.rcMonitor.top) {
        y = static_cast<int>(mi.rcWork.top + 12);
    } else if (mi.rcWork.left > mi.rcMonitor.left) {
        x = static_cast<int>(mi.rcWork.left + 12);
        y = std::clamp(static_cast<int>(pt.y - h / 2), static_cast<int>(mi.rcWork.top + 12), static_cast<int>(mi.rcWork.bottom - h - 12));
    } else if (mi.rcWork.right < mi.rcMonitor.right) {
        x = static_cast<int>(mi.rcWork.right - w - 12);
        y = std::clamp(static_cast<int>(pt.y - h / 2), static_cast<int>(mi.rcWork.top + 12), static_cast<int>(mi.rcWork.bottom - h - 12));
    }

    x = std::clamp(x, static_cast<int>(mi.rcWork.left + 12), static_cast<int>(mi.rcWork.right - w - 12));
    y = std::clamp(y, static_cast<int>(mi.rcWork.top + 12), static_cast<int>(mi.rcWork.bottom - h - 12));

    SetWindowPos(m_hPanelWnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void LuxApp::PaintPanel(HDC hdcOut, const RECT& rc) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    HDC hdc = CreateCompatibleDC(hdcOut);
    HBITMAP hbmp = CreateCompatibleBitmap(hdcOut, w, h);
    HBITMAP hOld = static_cast<HBITMAP>(SelectObject(hdc, hbmp));

    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    Gdiplus::Color colorBg     = m_darkMode ? Gdiplus::Color(255, 32, 32, 32) : Gdiplus::Color(255, 249, 249, 249);
    Gdiplus::Color colorBorder = m_darkMode ? Gdiplus::Color(255, 56, 56, 56) : Gdiplus::Color(255, 228, 228, 228);
    Gdiplus::Color colorFg     = m_darkMode ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 24, 24, 24);
    Gdiplus::Color colorSub    = m_darkMode ? Gdiplus::Color(255, 160, 160, 160) : Gdiplus::Color(255, 105, 105, 105);
    Gdiplus::Color colorAccent = m_darkMode ? Gdiplus::Color(255, 96, 205, 255) : Gdiplus::Color(255, 0, 103, 192);
    Gdiplus::Color colorTrack  = m_darkMode ? Gdiplus::Color(255, 60, 60, 60) : Gdiplus::Color(255, 220, 220, 220);
    Gdiplus::Color colorHover  = m_darkMode ? Gdiplus::Color(255, 50, 50, 50) : Gdiplus::Color(255, 235, 235, 235);

    // Fondo y borde suave
    Gdiplus::SolidBrush brBg(colorBg);
    g.FillRectangle(&brBg, 0, 0, w, h);

    Gdiplus::Pen penBorder(colorBorder, 1.0f);
    g.DrawRectangle(&penBorder, 0, 0, w - 1, h - 1);

    m_sliderRects.clear();
    int y = 14;

    Gdiplus::Font* fontTitle = m_pFontTitle;
    Gdiplus::SolidBrush brFg(colorFg);
    Gdiplus::SolidBrush brSub(colorSub);
    Gdiplus::SolidBrush brAccent(colorAccent);
    Gdiplus::SolidBrush brTrack(colorTrack);
    Gdiplus::SolidBrush brWhite(Gdiplus::Color(255, 255, 255, 255));

    const auto& monitors = m_monitorMgr.GetMonitors();

    if (monitors.empty()) {
        Gdiplus::StringFormat sfCenter;
        sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfCenter.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF rcEmpty(20.0f, static_cast<float>(y), w - 40.0f, static_cast<float>(BASE_ROW_HEIGHT));
        g.DrawString(L"No se detectaron pantallas compatibles", -1, fontTitle, rcEmpty, &sfCenter, &brSub);
        y += BASE_ROW_HEIGHT;
    } else {
        for (const auto& m : monitors) {
            // Icono Tabler: device-desktop antialiased
            Gdiplus::Pen penDesk(colorFg, 2.0f);
            penDesk.SetLineJoin(Gdiplus::LineJoinRound);
            penDesk.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);

            // Pantalla redondeada
            Gdiplus::GraphicsPath pathDesk;
            float scrX = 20.0f, scrY = static_cast<float>(y + 2), scrW = 22.0f, scrH = 15.0f, scrR = 3.0f;
            pathDesk.AddArc(scrX, scrY, scrR * 2, scrR * 2, 180, 90);
            pathDesk.AddArc(scrX + scrW - scrR * 2, scrY, scrR * 2, scrR * 2, 270, 90);
            pathDesk.AddArc(scrX + scrW - scrR * 2, scrY + scrH - scrR * 2, scrR * 2, scrR * 2, 0.0f, 90.0f);
            pathDesk.AddArc(scrX, scrY + scrH - scrR * 2, scrR * 2, scrR * 2, 90.0f, 90.0f);
            pathDesk.CloseFigure();
            g.DrawPath(&penDesk, &pathDesk);

            // Patas y base
            g.DrawLine(&penDesk, scrX + 7.0f, scrY + scrH, scrX + 5.0f, scrY + scrH + 5.0f);
            g.DrawLine(&penDesk, scrX + 15.0f, scrY + scrH, scrX + 17.0f, scrY + scrH + 5.0f);
            g.DrawLine(&penDesk, scrX + 3.0f, scrY + scrH + 5.0f, scrX + 19.0f, scrY + scrH + 5.0f);

            // Nombre del monitor
            Gdiplus::StringFormat sfLeft;
            sfLeft.SetAlignment(Gdiplus::StringAlignmentNear);
            sfLeft.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            sfLeft.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            Gdiplus::RectF rcName(50.0f, static_cast<float>(y), w - 120.0f, 22.0f);
            g.DrawString(m.name.c_str(), -1, fontTitle, rcName, &sfLeft, &brFg);

            // Porcentaje numérico a la derecha
            wchar_t bufVal[16];
            swprintf_s(bufVal, L"%d%%", m.current);
            Gdiplus::StringFormat sfRight;
            sfRight.SetAlignment(Gdiplus::StringAlignmentFar);
            sfRight.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::RectF rcVal(w - 70.0f, static_cast<float>(y), 50.0f, 22.0f);
            g.DrawString(bufVal, -1, fontTitle, rcVal, &sfRight, &brFg);

            // Mini icono de sol Tabler al inicio del slider
            int sliderY = y + 48;
            float sunCx = 29.0f;
            float sunCy = static_cast<float>(sliderY);
            Gdiplus::Pen penMiniSun(colorSub, 1.4f);
            penMiniSun.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
            g.DrawEllipse(&penMiniSun, sunCx - 3.5f, sunCy - 3.5f, 7.0f, 7.0f);
            for (int i = 0; i < 8; ++i) {
                float a = i * 3.14159265f / 4.0f;
                g.DrawLine(&penMiniSun, sunCx + std::cos(a) * 5.2f, sunCy + std::sin(a) * 5.2f,
                                        sunCx + std::cos(a) * 7.2f, sunCy + std::sin(a) * 7.2f);
            }

            // Slider moderno estilo Windows 11 con antialiasing GDI+
            float sliderLeft = 46.0f;
            float sliderRight = w - 20.0f;
            float trackH = 6.0f;

            // Pista de fondo
            Gdiplus::GraphicsPath pathTrack;
            pathTrack.AddArc(sliderLeft, sliderY - trackH / 2, trackH, trackH, 90, 180);
            pathTrack.AddArc(sliderRight - trackH, sliderY - trackH / 2, trackH, trackH, 270, 180);
            pathTrack.CloseFigure();
            g.FillPath(&brTrack, &pathTrack);

            // Pista de acento llena
            float pct = m.current / 100.0f;
            float fillX = sliderLeft + pct * (sliderRight - sliderLeft);
            if (fillX > sliderLeft + 3.0f) {
                Gdiplus::GraphicsPath pathFill;
                float curW = fillX - sliderLeft;
                if (curW < trackH) curW = trackH;
                pathFill.AddArc(sliderLeft, sliderY - trackH / 2, trackH, trackH, 90, 180);
                pathFill.AddArc(sliderLeft + curW - trackH, sliderY - trackH / 2, trackH, trackH, 270, 180);
                pathFill.CloseFigure();
                g.FillPath(&brAccent, &pathFill);
            }

            // Pomo circular del slider
            float thumbR = 8.0f;
            g.FillEllipse(&brAccent, fillX - thumbR, sliderY - thumbR, thumbR * 2, thumbR * 2);
            float innerR = 4.0f;
            g.FillEllipse(&brWhite, fillX - innerR, sliderY - innerR, innerR * 2, innerR * 2);

            // Rectángulo de colisión
            RECT hitRc{ static_cast<int>(sliderLeft - 6), sliderY - 14, static_cast<int>(sliderRight + 6), sliderY + 14 };
            m_sliderRects.push_back(hitRc);

            y += BASE_ROW_HEIGHT;
        }
    }

    // Línea separadora sutil
    g.DrawLine(&penBorder, 16.0f, static_cast<float>(y + 4), w - 16.0f, static_cast<float>(y + 4));
    y += 12;

    int footY = y;
    int footH = 30;

    // Logo oficial de Lux en el footer
    LuxUtils::DrawLuxLogo(g, 18.0f, footY + (footH - 24.0f) / 2.0f, 24.0f);

    // Nombre de marca "Lux"
    Gdiplus::StringFormat sfFoot;
    sfFoot.SetAlignment(Gdiplus::StringAlignmentNear);
    sfFoot.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::RectF rcFoot(48.0f, static_cast<float>(footY), w - 130.0f, static_cast<float>(footH));
    g.DrawString(APP_NAME, -1, fontTitle, rcFoot, &sfFoot, &brFg);

    m_rcBtnRefresh  = RECT{ w - 74, footY, w - 42, footY + footH };
    m_rcBtnSettings = RECT{ w - 38, footY, w - 6,  footY + footH };

    // Botón Reload
    if (m_hoverBtn == 1) {
        Gdiplus::SolidBrush brHov(colorHover);
        g.FillRectangle(&brHov, m_rcBtnRefresh.left, m_rcBtnRefresh.top, 32, footH);
    }
    {
        float rcx = (m_rcBtnRefresh.left + m_rcBtnRefresh.right) / 2.0f;
        float rcy = (m_rcBtnRefresh.top + m_rcBtnRefresh.bottom) / 2.0f;
        Gdiplus::Pen penRel(colorFg, 2.0f);
        penRel.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
        penRel.SetLineJoin(Gdiplus::LineJoinRound);

        float rArc = 6.8f;
        g.DrawArc(&penRel, rcx - rArc, rcy - rArc, rArc * 2, rArc * 2, 10.0f, 320.0f);

        Gdiplus::PointF arrowPts[3] = {
            { rcx + rArc, rcy - rArc - 2.5f },
            { rcx + rArc, rcy - 2.5f },
            { rcx + rArc - 4.5f, rcy - 2.5f }
        };
        g.DrawLines(&penRel, arrowPts, 3);
    }

    // Botón Settings
    if (m_hoverBtn == 2) {
        Gdiplus::SolidBrush brHov(colorHover);
        g.FillRectangle(&brHov, m_rcBtnSettings.left, m_rcBtnSettings.top, 32, footH);
    }
    {
        float scx = (m_rcBtnSettings.left + m_rcBtnSettings.right) / 2.0f;
        float scy = (m_rcBtnSettings.top + m_rcBtnSettings.bottom) / 2.0f;
        Gdiplus::Pen penSet(colorFg, 1.8f);
        penSet.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
        penSet.SetLineJoin(Gdiplus::LineJoinRound);

        float rHole = 2.8f;
        g.DrawEllipse(&penSet, scx - rHole, scy - rHole, rHole * 2, rHole * 2);

        Gdiplus::PointF gearPts[24];
        for (int i = 0; i < 8; ++i) {
            float a = i * 3.14159265f / 4.0f;
            float rTip = 8.4f;
            float rVal = 6.3f;
            gearPts[i * 3 + 0] = { scx + std::cos(a - 0.16f) * rTip, scy + std::sin(a - 0.16f) * rTip };
            gearPts[i * 3 + 1] = { scx + std::cos(a + 0.16f) * rTip, scy + std::sin(a + 0.16f) * rTip };
            gearPts[i * 3 + 2] = { scx + std::cos(a + 0.39f) * rVal, scy + std::sin(a + 0.39f) * rVal };
        }
        g.DrawClosedCurve(&penSet, gearPts, 24, 0.35f);
    }

    BitBlt(hdcOut, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, hOld);
    DeleteObject(hbmp);
    DeleteDC(hdc);
}

void LuxApp::ShowPanel() {
    m_monitorMgr.RefreshMonitors();
    PositionPanelNearTray();

    BOOL dark = m_darkMode ? TRUE : FALSE;
    DwmSetWindowAttribute(m_hPanelWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DWORD corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hPanelWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    ShowWindow(m_hPanelWnd, SW_SHOW);
    SetForegroundWindow(m_hPanelWnd);
    InvalidateRect(m_hPanelWnd, nullptr, FALSE);
}

void LuxApp::HidePanel() {
    ShowWindow(m_hPanelWnd, SW_HIDE);
    LuxUtils::PurgeWorkingSet();
}

void LuxApp::TogglePanel() {
    if (IsWindowVisible(m_hPanelWnd)) {
        m_lastPanelCloseTime = GetTickCount64();
        HidePanel();
    } else {
        ULONGLONG now = GetTickCount64();
        if (now - m_lastPanelCloseTime > 250) {
            ShowPanel();
        }
    }
}

void LuxApp::OpenSettings() {
    m_settingsWnd.Show(m_hInst, &m_settings, m_darkMode, m_hbrThemeBg,
        [this]() { ApplyTheme(); },
        [this](bool show) {
            if (show) m_trayIcon.Create(m_hMsgWnd, SettingsManager::IsTaskbarLightTheme());
            else m_trayIcon.Remove();
        },
        [this](bool) {
            // Callback de cambio de autostart
        }
    );
}

LRESULT CALLBACK LuxApp::MsgWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return Instance().HandleMsgWnd(hwnd, msg, wp, lp);
}

LRESULT CALLBACK LuxApp::PanelWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return Instance().HandlePanelWnd(hwnd, msg, wp, lp);
}

LRESULT LuxApp::HandleMsgWnd(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == m_msgTaskbarCreated) {
        if (m_settings.GetShowTrayIcon()) {
            m_trayIcon.Create(m_hMsgWnd, SettingsManager::IsTaskbarLightTheme());
        }
        return 0;
    }
    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lp) == WM_LBUTTONUP) {
            TogglePanel();
        } else if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU) {
            m_trayIcon.ShowContextMenu(m_hMsgWnd, m_darkMode);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_TRAY_OPEN:     ShowPanel(); return 0;
        case IDM_TRAY_SETTINGS: OpenSettings(); return 0;
        case IDM_TRAY_EXIT:
            m_trayIcon.Remove();
            PostQuitMessage(0);
            return 0;
        }
        return 0;

    case WM_DISPLAYCHANGE:
        RefreshMonitors();
        return 0;

    case WM_DEVICECHANGE:
        if (wp == DBT_DEVNODES_CHANGED) {
            RefreshMonitors();
        }
        return 0;

    case WM_SETTINGCHANGE:
        ApplyTheme();
        return 0;

    case WM_DESTROY:
        m_trayIcon.Remove();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT LuxApp::HandlePanelWnd(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        PaintPanel(hdc, rc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (PtInRect(&m_rcBtnSettings, pt)) {
            m_lastPanelCloseTime = GetTickCount64();
            HidePanel();
            OpenSettings();
            return 0;
        }
        if (PtInRect(&m_rcBtnRefresh, pt)) {
            RefreshMonitors();
            return 0;
        }
        for (size_t i = 0; i < m_sliderRects.size(); ++i) {
            if (PtInRect(&m_sliderRects[i], pt)) {
                m_draggingIndex = static_cast<int>(i);
                SetCapture(hwnd);
                int val = ComputeValueFromX(pt.x, m_sliderRects[i]);
                m_monitorMgr.ApplyBrightness(i, val);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (m_draggingIndex >= 0 && m_draggingIndex < static_cast<int>(m_monitorMgr.GetMonitorCount())) {
            int val = ComputeValueFromX(pt.x, m_sliderRects[m_draggingIndex]);
            const auto* mon = m_monitorMgr.GetMonitor(m_draggingIndex);
            if (mon && val != mon->current) {
                m_monitorMgr.ApplyBrightness(static_cast<size_t>(m_draggingIndex), val);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        } else {
            int prevHover = m_hoverBtn;
            if (PtInRect(&m_rcBtnRefresh, pt)) m_hoverBtn = 1;
            else if (PtInRect(&m_rcBtnSettings, pt)) m_hoverBtn = 2;
            else m_hoverBtn = 0;

            if (m_hoverBtn != prevHover) {
                InvalidateRect(hwnd, nullptr, FALSE);
                TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (m_hoverBtn != 0) {
            m_hoverBtn = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (m_draggingIndex >= 0) {
            if (m_draggingIndex < static_cast<int>(m_monitorMgr.GetMonitorCount())) {
                POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                int val = ComputeValueFromX(pt.x, m_sliderRects[m_draggingIndex]);
                m_monitorMgr.ApplyBrightness(static_cast<size_t>(m_draggingIndex), val);
            }
            ReleaseCapture();
            m_draggingIndex = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &pt);
        int step = (delta > 0) ? 5 : -5;
        for (size_t i = 0; i < m_monitorMgr.GetMonitorCount(); ++i) {
            int rowTop = 14 + static_cast<int>(i) * BASE_ROW_HEIGHT;
            if (pt.y >= rowTop && pt.y < rowTop + BASE_ROW_HEIGHT) {
                const auto* mon = m_monitorMgr.GetMonitor(i);
                if (mon) {
                    m_monitorMgr.ApplyBrightness(i, mon->current + step);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                break;
            }
        }
        return 0;
    }
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) {
            m_lastPanelCloseTime = GetTickCount64();
            HidePanel();
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
