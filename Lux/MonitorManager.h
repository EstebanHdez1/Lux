#pragma once
#include "Common.h"
#include "WmiBrightness.h"

class MonitorManager {
public:
    MonitorManager();
    ~MonitorManager();

    bool Init(CWmiBrightness* pWmi);
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

private:
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lp);
    void HardwareWorkerThread();

    CWmiBrightness* m_pWmi = nullptr;
    std::vector<MonitorEntry> m_monitors;

    // Hilo asíncrono de hardware (cero retraso en la interfaz)
    std::thread m_hwThread;
    std::mutex m_hwMutex;
    std::condition_variable m_hwCv;
    std::map<size_t, int> m_pendingBrightness;
    std::atomic<bool> m_hwRunning{ false };
};
