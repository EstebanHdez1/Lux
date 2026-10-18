#include "Utils.h"

namespace LuxUtils {

    using fnSetPreferredAppMode    = PreferredAppMode(WINAPI*)(PreferredAppMode);
    using fnAllowDarkModeForWindow = BOOL(WINAPI*)(HWND, BOOL);
    using fnFlushMenuThemes        = void(WINAPI*)();

    static fnSetPreferredAppMode    g_pSetPreferredAppMode    = nullptr;
    static fnAllowDarkModeForWindow g_pAllowDarkModeForWindow = nullptr;
    static fnFlushMenuThemes        g_pFlushMenuThemes        = nullptr;

    void InitDarkModeAPIs() {
        HMODULE hUx = GetModuleHandleW(L"uxtheme.dll");
        if (!hUx) hUx = LoadLibraryW(L"uxtheme.dll");
        if (hUx) {
            g_pSetPreferredAppMode    = reinterpret_cast<fnSetPreferredAppMode>(GetProcAddress(hUx, MAKEINTRESOURCEA(135)));
            g_pAllowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>(GetProcAddress(hUx, MAKEINTRESOURCEA(133)));
            g_pFlushMenuThemes        = reinterpret_cast<fnFlushMenuThemes>(GetProcAddress(hUx, MAKEINTRESOURCEA(136)));
        }
    }

    void SetPreferredThemeMode(bool darkMode) {
        if (g_pSetPreferredAppMode) {
            g_pSetPreferredAppMode(darkMode ? ForceDark : ForceLight);
        }
    }

    void AllowDarkModeForWindow(HWND hwnd, bool darkMode) {
        if (g_pAllowDarkModeForWindow && hwnd) {
            g_pAllowDarkModeForWindow(hwnd, darkMode ? TRUE : FALSE);
        }
    }

    void FlushMenuThemes() {
        if (g_pFlushMenuThemes) {
            g_pFlushMenuThemes();
        }
    }

    std::wstring GetExePath() {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return buf;
    }

    std::wstring GetIniPath() {
        std::wstring p = GetExePath();
        size_t dot = p.find_last_of(L'.');
        if (dot != std::wstring::npos) p = p.substr(0, dot);
        return p + L".ini";
    }

    int GetDpiScale(HWND hwnd) {
        HMODULE hUser = GetModuleHandleW(L"user32.dll");
        if (hUser) {
            using fnGetDpiForWindow = UINT(WINAPI*)(HWND);
            auto pGetDpiForWindow = reinterpret_cast<fnGetDpiForWindow>(GetProcAddress(hUser, "GetDpiForWindow"));
            if (pGetDpiForWindow && hwnd) {
                UINT dpi = pGetDpiForWindow(hwnd);
                if (dpi != 0) return static_cast<int>(dpi);
            }
        }
        return 96;
    }

    void ConfigureWindowsPerformanceMode() {
        // 1. Elevar prioridad para que la respuesta de UI y atajos sea inmediata
        SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);

        // 2. Desactivar el Power Throttling / limitación de velocidad de Windows 11
        #ifndef PROCESS_POWER_THROTTLING_CURRENT_VERSION
        #define PROCESS_POWER_THROTTLING_CURRENT_VERSION 1
        #endif
        #ifndef PROCESS_POWER_THROTTLING_EXECUTION_SPEED
        #define PROCESS_POWER_THROTTLING_EXECUTION_SPEED 0x1
        #endif

        HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
        if (hKernel) {
            using fnSetProcessInformation = BOOL(WINAPI*)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
            auto pSetProcessInfo = reinterpret_cast<fnSetProcessInformation>(GetProcAddress(hKernel, "SetProcessInformation"));
            if (pSetProcessInfo) {
                struct {
                    ULONG Version;
                    ULONG ControlMask;
                    ULONG StateMask;
                } throttling{};
                throttling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
                throttling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
                throttling.StateMask = 0; // 0 = Sin limitación de velocidad (Alto rendimiento)
                pSetProcessInfo(GetCurrentProcess(), static_cast<PROCESS_INFORMATION_CLASS>(9), &throttling, sizeof(throttling));
            }
        }

        // 3. Registrar en Preferencias de Gráficos de Windows (UserGpuPreferences): "GpuPreference=2;" (Alto Rendimiento)
        std::wstring exePath = GetExePath();
        if (!exePath.empty()) {
            HKEY hKey;
            if (RegCreateKeyExW(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\DirectX\\UserGpuPreferences",
                0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                const wchar_t* pref = L"GpuPreference=2;";
                RegSetValueExW(hKey, exePath.c_str(), 0, REG_SZ,
                    reinterpret_cast<const BYTE*>(pref),
                    static_cast<DWORD>((wcslen(pref) + 1) * sizeof(wchar_t)));
                RegCloseKey(hKey);
            }
        }
    }

    void PurgeWorkingSet() {
        SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
    }

    void DrawLuxLogo(Gdiplus::Graphics& g, float x, float y, float size) {
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

        float scale = size / 200.0f;

        // 1. Estrella exterior curvada con gradiente (#00CFFF a #783BFF)
        Gdiplus::GraphicsPath path;
        Gdiplus::PointF p0(x + 100.0f * scale, y + 18.0f * scale);
        Gdiplus::PointF c1a(x + 106.667f * scale, y + 59.333f * scale);
        Gdiplus::PointF c1b(x + 130.667f * scale, y + 86.667f * scale);
        Gdiplus::PointF p1(x + 172.0f * scale, y + 100.0f * scale);

        Gdiplus::PointF c2a(x + 130.667f * scale, y + 106.667f * scale);
        Gdiplus::PointF c2b(x + 106.667f * scale, y + 134.0f * scale);
        Gdiplus::PointF p2(x + 100.0f * scale, y + 182.0f * scale);

        Gdiplus::PointF c3a(x + 93.333f * scale, y + 134.0f * scale);
        Gdiplus::PointF c3b(x + 69.333f * scale, y + 106.667f * scale);
        Gdiplus::PointF p3(x + 28.0f * scale, y + 100.0f * scale);

        Gdiplus::PointF c4a(x + 69.333f * scale, y + 86.667f * scale);
        Gdiplus::PointF c4b(x + 93.333f * scale, y + 59.333f * scale);

        path.AddBezier(p0, c1a, c1b, p1);
        path.AddBezier(p1, c2a, c2b, p2);
        path.AddBezier(p2, c3a, c3b, p3);
        path.AddBezier(p3, c4a, c4b, p0);
        path.CloseFigure();

        Gdiplus::PointF ptStart(x, y);
        Gdiplus::PointF ptEnd(x + size, y + size);
        Gdiplus::Color colStart(255, 0x00, 0xCF, 0xFF); // #00CFFF
        Gdiplus::Color colEnd(255, 0x78, 0x3B, 0xFF);   // #783BFF
        Gdiplus::LinearGradientBrush gradBrush(ptStart, ptEnd, colStart, colEnd);
        g.FillPath(&gradBrush, &path);

        // 2. Estrella interior blanca
        Gdiplus::PointF innerPts[8] = {
            { x + 100.0f * scale, y + 68.0f * scale },
            { x + 108.0f * scale, y + 92.0f * scale },
            { x + 132.0f * scale, y + 100.0f * scale },
            { x + 108.0f * scale, y + 108.0f * scale },
            { x + 100.0f * scale, y + 132.0f * scale },
            { x + 92.0f * scale,  y + 108.0f * scale },
            { x + 68.0f * scale,  y + 100.0f * scale },
            { x + 92.0f * scale,  y + 92.0f * scale }
        };
        Gdiplus::GraphicsPath innerPath;
        innerPath.AddPolygon(innerPts, 8);
        Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(242, 255, 255, 255));
        g.FillPath(&whiteBrush, &innerPath);
    }

} // namespace LuxUtils
