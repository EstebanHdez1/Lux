#pragma once
#include "Common.h"
#include "WmiBrightness.h"
#include "MonitorManager.h"
#include "SettingsManager.h"
#include "TrayIcon.h"
#include "SettingsWindow.h"

class LuxApp {
public:
    static LuxApp& Instance() {
        static LuxApp inst;
        return inst;
    }

    bool Init(HINSTANCE hInst);
    int  Run();
    void Shutdown();

    bool IsDarkMode() const { return m_darkMode; }
    HBRUSH GetThemeBrush() const { return m_hbrThemeBg; }

    void ApplyTheme();
    void RefreshMonitors();

    void ShowPanel();
    void HidePanel();
    void TogglePanel();

    void OpenSettings();

private:
    LuxApp() = default;
    ~LuxApp() = default;

    // Callbacks de ventana
    static LRESULT CALLBACK MsgWndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK PanelWndProc(HWND, UINT, WPARAM, LPARAM);

    LRESULT HandleMsgWnd(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandlePanelWnd(HWND, UINT, WPARAM, LPARAM);

    // Métodos del panel flotante
    void PositionPanelNearTray();
    void PaintPanel(HDC hdcOut, const RECT& rc);
    int  CalcPanelHeight() const;
    int  ComputeValueFromX(int x, const RECT& track) const;

    void RebuildThemeBrush();

    // Instancia y ventanas
    HINSTANCE m_hInst = nullptr;
    HWND m_hMsgWnd = nullptr;
    HWND m_hPanelWnd = nullptr;

    // Módulos
    CWmiBrightness   m_wmi;
    MonitorManager   m_monitorMgr;
    SettingsManager  m_settings;
    TrayIcon         m_trayIcon;
    SettingsWindow   m_settingsWnd;

    // Estado de la interfaz
    bool m_darkMode = false;
    HBRUSH m_hbrThemeBg = nullptr;

    std::vector<RECT> m_sliderRects;
    RECT m_rcBtnRefresh{};
    RECT m_rcBtnSettings{};
    int m_draggingIndex = -1;
    int m_hoverBtn = 0; // 0=ninguno, 1=refresh, 2=settings
    ULONGLONG m_lastPanelCloseTime = 0;

    // GDI+ y fuentes
    ULONG_PTR m_gdiplusToken = 0;
    Gdiplus::Font* m_pFontTitle = nullptr;
    Gdiplus::Font* m_pFontSmall = nullptr;

    UINT m_msgTaskbarCreated = 0;
};
