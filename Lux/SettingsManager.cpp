#include "SettingsManager.h"
#include "Utils.h"

SettingsManager::SettingsManager() {
    LoadSettings();
}

bool SettingsManager::IsSystemDarkMode() {
    HKEY hKey;
    bool dark = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 1, size = sizeof(value), type = 0;
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, &type,
                              reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS) {
            dark = (value == 0);
        }
        RegCloseKey(hKey);
    }
    return dark;
}

bool SettingsManager::IsTaskbarLightTheme() {
    HKEY hKey;
    bool light = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 0, size = sizeof(value), type = 0;
        if (RegQueryValueExW(hKey, L"SystemUsesLightTheme", nullptr, &type,
                              reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS) {
            light = (value != 0);
        }
        RegCloseKey(hKey);
    }
    return light;
}

bool SettingsManager::IsDarkEffective() const {
    if (m_themeMode == ThemeMode::Dark) return true;
    if (m_themeMode == ThemeMode::Light) return false;
    return IsSystemDarkMode();
}

bool SettingsManager::ReadAutoStartFromRegistry() {
    HKEY hKey;
    bool exists = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                       0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[MAX_PATH]; DWORD size = sizeof(buf); DWORD type = 0;
        if (RegQueryValueExW(hKey, APP_NAME, nullptr, &type,
                              reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS) {
            exists = true;
        }
        RegCloseKey(hKey);
    }
    return exists;
}

void SettingsManager::SetAutoStart(bool enable) {
    m_autoStart = enable;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                       0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) return;
    if (enable) {
        std::wstring exe = L"\"" + LuxUtils::GetExePath() + L"\"";
        RegSetValueExW(hKey, APP_NAME, 0, REG_SZ,
                        reinterpret_cast<const BYTE*>(exe.c_str()),
                        static_cast<DWORD>((exe.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, APP_NAME);
    }
    RegCloseKey(hKey);
}

void SettingsManager::SetShowTrayIcon(bool show) {
    m_showTrayIcon = show;
}

void SettingsManager::SetThemeMode(ThemeMode mode) {
    m_themeMode = mode;
}

void SettingsManager::LoadSettings() {
    std::wstring ini = LuxUtils::GetIniPath();
    m_autoStart    = GetPrivateProfileIntW(L"General", L"AutoStart", 0, ini.c_str()) != 0;
    m_showTrayIcon = GetPrivateProfileIntW(L"General", L"TrayIcon", 1, ini.c_str()) != 0;
    int theme      = GetPrivateProfileIntW(L"General", L"Theme", 2, ini.c_str());
    m_themeMode    = (theme == 0) ? ThemeMode::Light : (theme == 1) ? ThemeMode::Dark : ThemeMode::Auto;
    m_autoStart    = ReadAutoStartFromRegistry();
}

void SettingsManager::SaveSettings() {
    std::wstring ini = LuxUtils::GetIniPath();
    WritePrivateProfileStringW(L"General", L"AutoStart", m_autoStart ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"General", L"TrayIcon", m_showTrayIcon ? L"1" : L"0", ini.c_str());
    int theme = (m_themeMode == ThemeMode::Light) ? 0 : (m_themeMode == ThemeMode::Dark) ? 1 : 2;
    wchar_t buf[8]; swprintf_s(buf, L"%d", theme);
    WritePrivateProfileStringW(L"General", L"Theme", buf, ini.c_str());
}
