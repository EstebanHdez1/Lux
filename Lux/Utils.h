#pragma once
#include "Common.h"

namespace LuxUtils {

    // APIs no documentadas de uxtheme para modo oscuro
    enum PreferredAppMode {
        Default    = 0,
        AllowDark  = 1,
        ForceDark  = 2,
        ForceLight = 3,
        Max        = 4
    };

    void InitDarkModeAPIs();
    void SetPreferredThemeMode(bool darkMode);
    void AllowDarkModeForWindow(HWND hwnd, bool darkMode);
    void FlushMenuThemes();

    // Rutas de ejecución y configuración
    std::wstring GetExePath();
    std::wstring GetIniPath();

    // DPI y escala
    int GetDpiScale(HWND hwnd);

    // Ajustes de rendimiento y recursos
    void ConfigureWindowsPerformanceMode();
    void PurgeWorkingSet();

} // namespace LuxUtils
