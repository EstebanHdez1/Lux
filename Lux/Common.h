#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER       0x0A00
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dbt.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <objbase.h>
#include <gdiplus.h>
#include <wininet.h>
#include <urlmon.h>
#include <highlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <wbemidl.h>
#include <comdef.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <map>
#include <memory>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "urlmon.lib")

// DWM Constants
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

// Identificadores de mensajes y controles
constexpr UINT WM_TRAYICON            = WM_APP + 1;
constexpr UINT ID_TRAY_ICON           = 1;
constexpr UINT WM_UPDATE_CHECK_DONE   = WM_APP + 10;

constexpr int IDM_TRAY_OPEN           = 1001;
constexpr int IDM_TRAY_SETTINGS       = 1002;
constexpr int IDM_TRAY_EXIT           = 1003;

constexpr int IDC_TOGGLE_AUTOSTART    = 2001;
constexpr int IDC_TOGGLE_TRAYICON     = 2002;
constexpr int IDC_COMBO_THEME         = 2003;
constexpr int IDC_BTN_UPDATE          = 2004;

// Nombres de aplicación, versiones y clases de ventana
inline const wchar_t* const APP_NAME          = L"Lux";
inline const wchar_t* const APP_VERSION       = L"v1.0.0";
inline const wchar_t* const GITHUB_API_LATEST = L"https://api.github.com/repos/EstebanHdez1/Lux/releases/latest";
inline const wchar_t* const GITHUB_REPO_URL   = L"https://github.com/EstebanHdez1/Lux";
inline const wchar_t* const MSGWND_CLASS      = L"Lux_MsgWnd";
inline const wchar_t* const PANELWND_CLASS    = L"Lux_Panel";
inline const wchar_t* const SETTINGSWND_CLASS = L"Lux_Settings";

// Modos de tema
enum class ThemeMode {
    Light = 0,
    Dark  = 1,
    Auto  = 2
};

// Backends de control de brillo
enum class BrightnessBackend {
    Unsupported,
    DDC,
    WMI
};

// Entrada representativa de un monitor
struct MonitorEntry {
    HMONITOR hMonitor = nullptr;
    HANDLE   hPhysical = nullptr;
    std::wstring wmiInstance;
    std::wstring name;
    RECT rect{};
    BrightnessBackend backend = BrightnessBackend::Unsupported;
    int current = 100;
    int minVal  = 0;
    int maxVal  = 100;
};
