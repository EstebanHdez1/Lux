#pragma once
#include "Common.h"
#include "WmiBrightness.h"

class SettingsManager;

class MonitorManager {
public:
    MonitorManager();
    ~MonitorManager();

    bool Init(CWmiBrightness* pWmi, SettingsManager* pSettings = nullptr);
    void Shutdown();

    void RefreshMonitors();
    void FreeMonitors();

    void ApplyBrightness(size_t index, int value);

    const std::vector<MonitorEntry>& GetMonitors() const { return m_monitors; }
    size_t GetMonitorCount() const { return m_monitors.size(); }
    const MonitorEntry* GetMonitor(size_t index) const {
        if (index < m_monitors.size()) return &m_monitors[index];
        return nullptr;
    }

    static std::wstring GetEDIDMonitorName(HMONITOR hMonitor);

    // Métodos de control Gamma por GPU (para TVs HDMI, proyectores y monitores sin DDC/CI)
    static bool SetGammaBrightness(const std::wstring& deviceName, int value);
    static void RestoreGamma(const std::wstring& deviceName);

    // Métodos de Software Dimming Overlay (fallback universal)
    static HWND CreateOverlayWindow(const RECT& rc);
    static void SetOverlayAlpha(HWND hwnd, int brightness);
    static void DestroyOverlayWindow(HWND& hwnd);

private:
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lp);
    void HardwareWorkerThread();

    CWmiBrightness* m_pWmi = nullptr;
    class SettingsManager* m_pSettings = nullptr;
    std::vector<MonitorEntry> m_monitors;
    std::map<std::wstring, int> m_savedBrightness;

    // Hilo asíncrono de hardware (cero retraso en la interfaz)
    std::thread m_hwThread;
    std::mutex m_hwMutex;
    std::condition_variable m_hwCv;
    std::map<size_t, int> m_pendingBrightness;
    std::atomic<bool> m_hwRunning{ false };
};
