#pragma once
#include "Common.h"
#include "SettingsManager.h"
#include <functional>

class SettingsWindow {
public:
    SettingsWindow();
    ~SettingsWindow();

    void Show(HINSTANCE hInst, SettingsManager* pSettings, bool darkMode, HBRUSH hbrThemeBg,
              std::function<void()> onThemeChanged,
              std::function<void(bool)> onTrayIconChanged,
              std::function<void(bool)> onAutoStartChanged);

    void UpdateTheme(bool darkMode, HBRUSH hbrThemeBg);
    bool IsVisible() const;
    void Close();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK ComboBoxSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HWND m_hSettingsWnd = nullptr;
    HWND m_hComboTheme = nullptr;
    HFONT m_hFontUI = nullptr;
    HBRUSH m_hbrThemeBg = nullptr;
    bool m_darkMode = false;

    SettingsManager* m_pSettings = nullptr;
    std::function<void()> m_onThemeChanged;
    std::function<void(bool)> m_onTrayIconChanged;
    std::function<void(bool)> m_onAutoStartChanged;
};
