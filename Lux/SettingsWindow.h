#pragma once
#include "Common.h"
#include "SettingsManager.h"
#include "Updater.h"
#include <functional>

enum class UpdateActionState {
    Check,
    Download,
    Busy
};

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
    void HandleUpdateCheckDone(const UpdateCheckResult& result);

    HWND m_hSettingsWnd = nullptr;
    HWND m_hComboTheme = nullptr;
    HWND m_hLblUpdateStatus = nullptr;
    HWND m_hBtnUpdate = nullptr;
    HFONT m_hFontUI = nullptr;
    HFONT m_hFontSmall = nullptr;
    HBRUSH m_hbrThemeBg = nullptr;
    bool m_darkMode = false;

    SettingsManager* m_pSettings = nullptr;
    std::function<void()> m_onThemeChanged;
    std::function<void(bool)> m_onTrayIconChanged;
    std::function<void(bool)> m_onAutoStartChanged;

    UpdateActionState m_updateState = UpdateActionState::Check;
    UpdateCheckResult m_lastUpdateResult;
};
