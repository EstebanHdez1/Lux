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

} // namespace LuxUtils
