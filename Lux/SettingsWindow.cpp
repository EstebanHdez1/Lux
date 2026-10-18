#include "SettingsWindow.h"
#include "Utils.h"
#include <thread>

SettingsWindow::SettingsWindow() = default;

SettingsWindow::~SettingsWindow() {
    if (m_hFontUI) {
        DeleteObject(m_hFontUI);
        m_hFontUI = nullptr;
    }
    if (m_hFontSmall) {
        DeleteObject(m_hFontSmall);
        m_hFontSmall = nullptr;
    }
}

bool SettingsWindow::IsVisible() const {
    return m_hSettingsWnd && IsWindowVisible(m_hSettingsWnd);
}

void SettingsWindow::Close() {
    if (m_hSettingsWnd) {
        ShowWindow(m_hSettingsWnd, SW_HIDE);
        LuxUtils::PurgeWorkingSet();
    }
}

void SettingsWindow::UpdateTheme(bool darkMode, HBRUSH hbrThemeBg) {
    m_darkMode = darkMode;
    m_hbrThemeBg = hbrThemeBg;

    if (m_hSettingsWnd) {
        LuxUtils::AllowDarkModeForWindow(m_hSettingsWnd, m_darkMode);
        BOOL dark = m_darkMode ? TRUE : FALSE;
        DwmSetWindowAttribute(m_hSettingsWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        InvalidateRect(m_hSettingsWnd, nullptr, TRUE);
        if (m_hComboTheme) InvalidateRect(m_hComboTheme, nullptr, TRUE);
        if (m_hBtnUpdate) InvalidateRect(m_hBtnUpdate, nullptr, TRUE);
    }
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SettingsWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        pThis = reinterpret_cast<SettingsWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void SettingsWindow::Show(HINSTANCE hInst, SettingsManager* pSettings, bool darkMode, HBRUSH hbrThemeBg,
                          std::function<void()> onThemeChanged,
                          std::function<void(bool)> onTrayIconChanged,
                          std::function<void(bool)> onAutoStartChanged) {
    m_pSettings = pSettings;
    m_darkMode = darkMode;
    m_hbrThemeBg = hbrThemeBg;
    m_onThemeChanged = std::move(onThemeChanged);
    m_onTrayIconChanged = std::move(onTrayIconChanged);
    m_onAutoStartChanged = std::move(onAutoStartChanged);

    if (m_hSettingsWnd) {
        ShowWindow(m_hSettingsWnd, SW_SHOW);
        SetForegroundWindow(m_hSettingsWnd);
        return;
    }

    // Registrar clase de ventana si no existe
    WNDCLASSEXW wcSettings{ sizeof(wcSettings) };
    if (!GetClassInfoExW(hInst, SETTINGSWND_CLASS, &wcSettings)) {
        wcSettings.style = CS_DROPSHADOW;
        wcSettings.lpfnWndProc = &SettingsWindow::WndProc;
        wcSettings.hInstance = hInst;
        wcSettings.lpszClassName = SETTINGSWND_CLASS;
        wcSettings.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wcSettings);
    }

    if (!m_hFontUI) {
        m_hFontUI = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }
    if (!m_hFontSmall) {
        m_hFontSmall = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    std::wstring title = std::wstring(L"Configuraci\u00F3n de ") + APP_NAME;
    m_hSettingsWnd = CreateWindowExW(WS_EX_DLGMODALFRAME, SETTINGSWND_CLASS,
        title.c_str(),
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 480, 390,
        nullptr, nullptr, hInst, this);

    LuxUtils::AllowDarkModeForWindow(m_hSettingsWnd, m_darkMode);
    BOOL dark = m_darkMode ? TRUE : FALSE;
    DwmSetWindowAttribute(m_hSettingsWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DWORD corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hSettingsWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    int y = 24;
    HWND hLbl1 = CreateWindowExW(0, L"STATIC", L"Iniciar con Windows", WS_CHILD | WS_VISIBLE,
                                 28, y + 4, 240, 22, m_hSettingsWnd, nullptr, hInst, nullptr);
    HWND hBtn1 = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                 370, y, 48, 24, m_hSettingsWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TOGGLE_AUTOSTART)), hInst, nullptr);
    SendMessageW(hLbl1, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontUI), TRUE);

    y += 48;
    HWND hLbl2 = CreateWindowExW(0, L"STATIC", L"Mostrar icono en la bandeja", WS_CHILD | WS_VISIBLE,
                                 28, y + 4, 260, 22, m_hSettingsWnd, nullptr, hInst, nullptr);
    HWND hBtn2 = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                 370, y, 48, 24, m_hSettingsWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TOGGLE_TRAYICON)), hInst, nullptr);
    SendMessageW(hLbl2, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontUI), TRUE);

    y += 48;
    HWND hLbl3 = CreateWindowExW(0, L"STATIC", L"Tema de la aplicaci\u00F3n", WS_CHILD | WS_VISIBLE,
                                 28, y + 6, 220, 22, m_hSettingsWnd, nullptr, hInst, nullptr);

    m_hComboTheme = CreateWindowExW(0, L"COMBOBOX", L"",
                                    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
                                    260, y, 160, 220, m_hSettingsWnd,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_COMBO_THEME)), hInst, nullptr);

    SendMessageW(hLbl3, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontUI), TRUE);
    SendMessageW(m_hComboTheme, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontUI), TRUE);

    SetWindowSubclass(m_hComboTheme, &SettingsWindow::ComboBoxSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));

    ComboBox_AddString(m_hComboTheme, L"Claro");
    ComboBox_AddString(m_hComboTheme, L"Oscuro");
    ComboBox_AddString(m_hComboTheme, L"Autom\u00E1tico (sistema)");
    if (m_pSettings) {
        ComboBox_SetCurSel(m_hComboTheme, static_cast<int>(m_pSettings->GetThemeMode()));
    }

    // Sección de Actualizaciones
    y += 62;
    HWND hLblUpdateTitle = CreateWindowExW(0, L"STATIC", L"Actualizaciones", WS_CHILD | WS_VISIBLE,
                                          28, y, 220, 22, m_hSettingsWnd, nullptr, hInst, nullptr);
    SendMessageW(hLblUpdateTitle, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontUI), TRUE);

    std::wstring initialVer = std::wstring(L"Versi\u00F3n instalada: ") + APP_VERSION;
    m_hLblUpdateStatus = CreateWindowExW(0, L"STATIC", initialVer.c_str(), WS_CHILD | WS_VISIBLE,
                                        28, y + 24, 230, 20, m_hSettingsWnd, nullptr, hInst, nullptr);
    SendMessageW(m_hLblUpdateStatus, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSmall), TRUE);

    m_hBtnUpdate = CreateWindowExW(0, L"BUTTON", L"Buscar actualizaciones", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                  260, y + 8, 160, 32, m_hSettingsWnd,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BTN_UPDATE)), hInst, nullptr);

    ShowWindow(m_hSettingsWnd, SW_SHOW);
    SetForegroundWindow(m_hSettingsWnd);
}

LRESULT CALLBACK SettingsWindow::ComboBoxSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR dwRefData) {
    auto* pThis = reinterpret_cast<SettingsWindow*>(dwRefData);
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        bool dark = pThis ? pThis->m_darkMode : false;
        Gdiplus::Color colorBg     = dark ? Gdiplus::Color(255, 36, 36, 36) : Gdiplus::Color(255, 245, 245, 245);
        Gdiplus::Color colorBorder = dark ? Gdiplus::Color(255, 65, 65, 65) : Gdiplus::Color(255, 215, 215, 215);
        Gdiplus::Color colorFg     = dark ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 24, 24, 24);

        Gdiplus::Graphics g(memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        // Fondo del diálogo padre
        Gdiplus::SolidBrush brParent(dark ? Gdiplus::Color(255, 32, 32, 32) : Gdiplus::Color(255, 249, 249, 249));
        g.FillRectangle(&brParent, 0, 0, w, h);

        // Rectángulo redondeado suave
        float rx = 0.5f, ry = 0.5f, rw = w - 1.0f, rh = h - 1.0f, radius = 5.0f;
        Gdiplus::GraphicsPath pathBox;
        pathBox.AddArc(rx, ry, radius * 2, radius * 2, 180, 90);
        pathBox.AddArc(rx + rw - radius * 2, ry, radius * 2, radius * 2, 270, 90);
        pathBox.AddArc(rx + rw - radius * 2, ry + rh - radius * 2, radius * 2, radius * 2, 0, 90);
        pathBox.AddArc(rx, ry + rh - radius * 2, radius * 2, radius * 2, 90, 90);
        pathBox.CloseFigure();

        Gdiplus::SolidBrush brBox(colorBg);
        g.FillPath(&brBox, &pathBox);
        Gdiplus::Pen penBorder(colorBorder, 1.0f);
        g.DrawPath(&penBorder, &pathBox);

        // Texto seleccionado
        int sel = ComboBox_GetCurSel(hwnd);
        if (sel != CB_ERR) {
            wchar_t text[128]{};
            ComboBox_GetLBText(hwnd, sel, text);
            Gdiplus::FontFamily fontFam(L"Segoe UI");
            Gdiplus::Font fontCombo(&fontFam, 9.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush brFg(colorFg);
            Gdiplus::StringFormat sf;
            sf.SetAlignment(Gdiplus::StringAlignmentNear);
            sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            Gdiplus::RectF rcText(10.0f, 0.0f, rw - 34.0f, static_cast<float>(h));
            g.DrawString(text, -1, &fontCombo, rcText, &sf, &brFg);
        }

        // Flecha desplegable en chevron redondeado
        float ax = w - 16.0f;
        float ay = h / 2.0f;
        Gdiplus::Pen penArr(colorFg, 1.8f);
        penArr.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
        penArr.SetLineJoin(Gdiplus::LineJoinRound);
        Gdiplus::PointF arrPts[3] = {
            { ax - 4.5f, ay - 2.0f },
            { ax,        ay + 2.5f },
            { ax + 4.5f, ay - 2.0f }
        };
        g.DrawLines(&penArr, arrPts, 3);

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void SettingsWindow::HandleUpdateCheckDone(const UpdateCheckResult& result) {
    m_lastUpdateResult = result;
    if (!result.success) {
        SetWindowTextW(m_hLblUpdateStatus, result.errorMessage.c_str());
        m_updateState = UpdateActionState::Check;
    } else if (result.updateAvailable) {
        std::wstring msg = L"\u00A1Nueva versi\u00F3n " + result.latestVersion + L" disponible!";
        SetWindowTextW(m_hLblUpdateStatus, msg.c_str());
        m_updateState = UpdateActionState::Download;
    } else {
        std::wstring msg = L"\u2713 Tienes la versi\u00F3n m\u00E1s reciente.";
        SetWindowTextW(m_hLblUpdateStatus, msg.c_str());
        m_updateState = UpdateActionState::Check;
    }
    if (m_hBtnUpdate) InvalidateRect(m_hBtnUpdate, nullptr, TRUE);
}

LRESULT SettingsWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, m_hbrThemeBg);
        return 1;
    }
    case WM_CTLCOLORDLG:
        return reinterpret_cast<LRESULT>(m_hbrThemeBg);

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = reinterpret_cast<HDC>(wp);
        SetTextColor(hdcStatic, m_darkMode ? RGB(255, 255, 255) : RGB(24, 24, 24));
        SetBkMode(hdcStatic, TRANSPARENT);
        return reinterpret_cast<LRESULT>(m_hbrThemeBg);
    }
    case WM_CTLCOLORBTN:
        return reinterpret_cast<LRESULT>(m_hbrThemeBg);

    case WM_CTLCOLORLISTBOX: {
        HDC hdcList = reinterpret_cast<HDC>(wp);
        SetBkColor(hdcList, m_darkMode ? RGB(32, 32, 32) : RGB(250, 250, 250));
        SetTextColor(hdcList, m_darkMode ? RGB(255, 255, 255) : RGB(20, 20, 20));
        return reinterpret_cast<LRESULT>(m_hbrThemeBg);
    }

    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT mis = reinterpret_cast<LPMEASUREITEMSTRUCT>(lp);
        if (mis->CtlID == IDC_COMBO_THEME) {
            mis->itemHeight = 28;
            return TRUE;
        }
        break;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lp);
        if (dis->CtlID == IDC_COMBO_THEME) {
            if (dis->itemID == -1) return TRUE;

            bool isSelected = (dis->itemState & ODS_SELECTED) != 0;
            COLORREF itemBg = isSelected
                ? (m_darkMode ? RGB(56, 56, 56) : RGB(225, 225, 225))
                : (m_darkMode ? RGB(32, 32, 32) : RGB(255, 255, 255));
            COLORREF itemFg = m_darkMode ? RGB(255, 255, 255) : RGB(20, 20, 20);

            HBRUSH hb = CreateSolidBrush(itemBg);
            FillRect(dis->hDC, &dis->rcItem, hb);
            DeleteObject(hb);

            wchar_t text[128]{};
            ComboBox_GetLBText(dis->hwndItem, dis->itemID, text);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, itemFg);

            RECT textRc = dis->rcItem;
            textRc.left += 10;
            DrawTextW(dis->hDC, text, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            return TRUE;
        }

        if (dis->CtlID == IDC_BTN_UPDATE) {
            RECT rc = dis->rcItem;
            Gdiplus::Graphics g(dis->hDC);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

            Gdiplus::Color btnBg;
            if (m_updateState == UpdateActionState::Download) {
                btnBg = m_darkMode ? Gdiplus::Color(255, 96, 205, 255) : Gdiplus::Color(255, 0, 103, 192);
            } else {
                btnBg = m_darkMode ? Gdiplus::Color(255, 48, 48, 48) : Gdiplus::Color(255, 230, 230, 230);
            }
            Gdiplus::Color btnFg = (m_updateState == UpdateActionState::Download)
                ? (m_darkMode ? Gdiplus::Color(255, 20, 20, 20) : Gdiplus::Color(255, 255, 255, 255))
                : (m_darkMode ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 20, 20, 20));

            float w = static_cast<float>(rc.right - rc.left);
            float h = static_cast<float>(rc.bottom - rc.top);
            float radius = 5.0f;

            Gdiplus::GraphicsPath path;
            path.AddArc(0.5f, 0.5f, radius * 2, radius * 2, 180, 90);
            path.AddArc(w - radius * 2 - 1.0f, 0.5f, radius * 2, radius * 2, 270, 90);
            path.AddArc(w - radius * 2 - 1.0f, h - radius * 2 - 1.0f, radius * 2, radius * 2, 0, 90);
            path.AddArc(0.5f, h - radius * 2 - 1.0f, radius * 2, radius * 2, 90, 90);
            path.CloseFigure();

            Gdiplus::SolidBrush br(btnBg);
            g.FillPath(&br, &path);

            Gdiplus::Pen penBorder(m_darkMode ? Gdiplus::Color(255, 68, 68, 68) : Gdiplus::Color(255, 210, 210, 210), 1.0f);
            g.DrawPath(&penBorder, &path);

            std::wstring label;
            if (m_updateState == UpdateActionState::Busy) label = L"Procesando...";
            else if (m_updateState == UpdateActionState::Download) label = L"Actualizar ahora";
            else label = L"Buscar actualizaciones";

            Gdiplus::FontFamily fontFam(L"Segoe UI");
            Gdiplus::Font fontBtn(&fontFam, 9.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush brText(btnFg);
            Gdiplus::StringFormat sf;
            sf.SetAlignment(Gdiplus::StringAlignmentCenter);
            sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::RectF rcF(0.0f, 0.0f, w, h);
            g.DrawString(label.c_str(), -1, &fontBtn, rcF, &sf, &brText);
            return TRUE;
        }

        // Interruptor toggle moderno con GDI+ antialiased
        bool checked = false;
        if (m_pSettings) {
            checked = (dis->CtlID == IDC_TOGGLE_AUTOSTART) ? m_pSettings->GetAutoStart()
                    : (dis->CtlID == IDC_TOGGLE_TRAYICON)  ? m_pSettings->GetShowTrayIcon() : false;
        }
        RECT rc = dis->rcItem;

        Gdiplus::Graphics g(dis->hDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        Gdiplus::Color onColor  = m_darkMode ? Gdiplus::Color(255, 96, 205, 255) : Gdiplus::Color(255, 0, 103, 192);
        Gdiplus::Color offColor = m_darkMode ? Gdiplus::Color(255, 60, 60, 60) : Gdiplus::Color(255, 220, 220, 220);

        float pw = static_cast<float>(rc.right - rc.left);
        float ph = static_cast<float>(rc.bottom - rc.top);
        float px = static_cast<float>(rc.left);
        float py = static_cast<float>(rc.top);

        Gdiplus::GraphicsPath pill;
        pill.AddArc(px, py, ph, ph, 90, 180);
        pill.AddArc(px + pw - ph, py, ph, ph, 270, 180);
        pill.CloseFigure();

        Gdiplus::SolidBrush brPill(checked ? onColor : offColor);
        g.FillPath(&brPill, &pill);

        // Pomo circular
        float kd = ph - 6.0f;
        float kx = checked ? px + pw - kd - 3.0f : px + 3.0f;
        float ky = py + 3.0f;

        Gdiplus::SolidBrush brKnob(checked ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 200, 200, 200));
        g.FillEllipse(&brKnob, kx, ky, kd, kd);

        return TRUE;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDC_TOGGLE_AUTOSTART && HIWORD(wp) == BN_CLICKED && m_pSettings) {
            bool nextState = !m_pSettings->GetAutoStart();
            m_pSettings->SetAutoStart(nextState);
            m_pSettings->SaveSettings();
            if (m_onAutoStartChanged) m_onAutoStartChanged(nextState);
            InvalidateRect(GetDlgItem(hwnd, IDC_TOGGLE_AUTOSTART), nullptr, TRUE);
        } else if (id == IDC_TOGGLE_TRAYICON && HIWORD(wp) == BN_CLICKED && m_pSettings) {
            bool nextState = !m_pSettings->GetShowTrayIcon();
            m_pSettings->SetShowTrayIcon(nextState);
            m_pSettings->SaveSettings();
            if (m_onTrayIconChanged) m_onTrayIconChanged(nextState);
            InvalidateRect(GetDlgItem(hwnd, IDC_TOGGLE_TRAYICON), nullptr, TRUE);
        } else if (id == IDC_COMBO_THEME && HIWORD(wp) == CBN_SELCHANGE && m_pSettings) {
            int sel = ComboBox_GetCurSel(reinterpret_cast<HWND>(lp));
            m_pSettings->SetThemeMode(static_cast<ThemeMode>(sel));
            m_pSettings->SaveSettings();
            if (m_onThemeChanged) m_onThemeChanged();
            InvalidateRect(hwnd, nullptr, TRUE);
            if (m_hComboTheme) InvalidateRect(m_hComboTheme, nullptr, TRUE);
        } else if (id == IDC_BTN_UPDATE && HIWORD(wp) == BN_CLICKED) {
            if (m_updateState == UpdateActionState::Check) {
                m_updateState = UpdateActionState::Busy;
                SetWindowTextW(m_hLblUpdateStatus, L"Buscando actualizaciones en GitHub...");
                InvalidateRect(m_hBtnUpdate, nullptr, TRUE);

                std::thread([hwnd]() {
                    UpdateCheckResult res = Updater::CheckForUpdate();
                    auto* pAlloc = new UpdateCheckResult(res);
                    PostMessageW(hwnd, WM_UPDATE_CHECK_DONE, 0, reinterpret_cast<LPARAM>(pAlloc));
                }).detach();
            } else if (m_updateState == UpdateActionState::Download) {
                m_updateState = UpdateActionState::Busy;
                SetWindowTextW(m_hLblUpdateStatus, L"Descargando e instalando actualizaci\u00F3n...");
                InvalidateRect(m_hBtnUpdate, nullptr, TRUE);

                std::wstring downloadUrl = m_lastUpdateResult.downloadUrl;
                std::thread([hwnd, downloadUrl]() {
                    std::wstring err;
                    bool ok = Updater::ApplyUpdate(downloadUrl, err);
                    if (!ok) {
                        auto* pAlloc = new UpdateCheckResult();
                        pAlloc->success = false;
                        pAlloc->errorMessage = err;
                        PostMessageW(hwnd, WM_UPDATE_CHECK_DONE, 1, reinterpret_cast<LPARAM>(pAlloc));
                    }
                }).detach();
            }
        }
        return 0;
    }

    case WM_UPDATE_CHECK_DONE: {
        auto* pRes = reinterpret_cast<UpdateCheckResult*>(lp);
        if (pRes) {
            if (wp == 1) { // Error al descargar/aplicar
                SetWindowTextW(m_hLblUpdateStatus, pRes->errorMessage.c_str());
                m_updateState = UpdateActionState::Check;
            } else {
                HandleUpdateCheckDone(*pRes);
            }
            delete pRes;
        }
        if (m_hBtnUpdate) InvalidateRect(m_hBtnUpdate, nullptr, TRUE);
        return 0;
    }

    case WM_CLOSE:
        Close();
        return 0;

    case WM_DESTROY:
        m_hSettingsWnd = nullptr;
        m_hComboTheme = nullptr;
        m_hLblUpdateStatus = nullptr;
        m_hBtnUpdate = nullptr;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}
