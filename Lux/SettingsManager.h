#pragma once
#include "Common.h"

class SettingsManager {
public:
    SettingsManager();

    void LoadSettings();
    void SaveSettings();

    bool GetAutoStart() const { return m_autoStart; }
    void SetAutoStart(bool enable);

    bool GetShowTrayIcon() const { return m_showTrayIcon; }
    void SetShowTrayIcon(bool show);

    ThemeMode GetThemeMode() const { return m_themeMode; }
    void SetThemeMode(ThemeMode mode);

    bool IsDarkEffective() const;
    static bool IsSystemDarkMode();
    static bool IsTaskbarLightTheme();

private:
    bool ReadAutoStartFromRegistry();

    bool m_autoStart = false;
    bool m_showTrayIcon = true;
    ThemeMode m_themeMode = ThemeMode::Auto;
};
