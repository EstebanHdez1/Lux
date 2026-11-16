#include "MonitorManager.h"

MonitorManager::MonitorManager() = default;

MonitorManager::~MonitorManager() {
    Shutdown();
}

bool MonitorManager::Init(CWmiBrightness* pWmi) {
    m_pWmi = pWmi;
    RefreshMonitors();

    // Hilo de comunicación con el hardware en segundo plano
    m_hwRunning = true;
    m_hwThread = std::thread(&MonitorManager::HardwareWorkerThread, this);
    return true;
}

void MonitorManager::Shutdown() {
    m_hwRunning = false;
    m_hwCv.notify_all();
    if (m_hwThread.joinable()) {
        m_hwThread.join();
    }
    FreeMonitors();
}

bool MonitorManager::SetGammaBrightness(const std::wstring& deviceName, int value) {
    if (deviceName.empty()) return false;
    HDC hdc = CreateDCW(deviceName.c_str(), nullptr, nullptr, nullptr);
    if (!hdc) return false;

    value = std::clamp(value, 0, 100);
    // Factor de atenuación: valor 0 -> 4% (para mantener visible el cursor), valor 100 -> 100%
    float factor = static_cast<float>(value) / 100.0f;
    factor = std::clamp(factor, 0.04f, 1.0f);

    WORD ramp[3][256];
    for (int i = 0; i < 256; ++i) {
        float raw = static_cast<float>(i * 257) * factor;
        DWORD v = static_cast<DWORD>(std::round(raw));
        if (v > 65535) v = 65535;
        ramp[0][i] = static_cast<WORD>(v);
        ramp[1][i] = static_cast<WORD>(v);
        ramp[2][i] = static_cast<WORD>(v);
    }

    BOOL ok = SetDeviceGammaRamp(hdc, ramp);
    DeleteDC(hdc);
    return (ok != FALSE);
}

void MonitorManager::RestoreGamma(const std::wstring& deviceName) {
    if (deviceName.empty()) return;
    HDC hdc = CreateDCW(deviceName.c_str(), nullptr, nullptr, nullptr);
    if (!hdc) return;

    WORD ramp[3][256];
    for (int i = 0; i < 256; ++i) {
        WORD v = static_cast<WORD>(i * 257);
        ramp[0][i] = v;
        ramp[1][i] = v;
        ramp[2][i] = v;
    }
    SetDeviceGammaRamp(hdc, ramp);
    DeleteDC(hdc);
}

HWND MonitorManager::CreateOverlayWindow(const RECT& rc) {
    static bool s_reg = false;
    const wchar_t* const CLS_NAME = L"Lux_DimmingOverlay";
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    if (!s_reg) {
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = hInst;
        wc.lpszClassName = CLS_NAME;
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
        s_reg = true;
    }

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        CLS_NAME, L"", WS_POPUP,
        rc.left, rc.top, w, h,
        nullptr, nullptr, hInst, nullptr
    );

    if (hwnd) {
        SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
    }
    return hwnd;
}

void MonitorManager::SetOverlayAlpha(HWND hwnd, int brightness) {
    if (!hwnd || !IsWindow(hwnd)) return;
    brightness = std::clamp(brightness, 0, 100);
    if (brightness >= 100) {
        ShowWindow(hwnd, SW_HIDE);
    } else {
        int alpha = static_cast<int>((100 - brightness) * 2.2f);
        alpha = std::clamp(alpha, 0, 220);
        SetLayeredWindowAttributes(hwnd, 0, static_cast<BYTE>(alpha), LWA_ALPHA);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }
}

void MonitorManager::DestroyOverlayWindow(HWND& hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
}

std::wstring MonitorManager::GetEDIDMonitorName(HMONITOR hMonitor) {
    MONITORINFOEXW mi{}; mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMonitor, &mi)) return L"";

    DISPLAY_DEVICEW dd{}; dd.cb = sizeof(dd);
    for (DWORD i = 0; EnumDisplayDevicesW(mi.szDevice, i, &dd, EDD_GET_DEVICE_INTERFACE_NAME); ++i) {
        std::wstring devId = dd.DeviceID;
        size_t p1 = devId.find(L"DISPLAY#");
        if (p1 == std::wstring::npos) continue;
        size_t start = p1 + 8;
        size_t p2 = devId.find(L'#', start);
        if (p2 == std::wstring::npos) continue;
        std::wstring hwid = devId.substr(start, p2 - start);
        size_t p3 = devId.find(L'#', p2 + 1);
        std::wstring inst = (p3 != std::wstring::npos) ? devId.substr(p2 + 1, p3 - p2 - 1) : L"";
        if (inst.empty()) continue;

        std::wstring regPath = L"SYSTEM\\CurrentControlSet\\Enum\\DISPLAY\\" + hwid + L"\\" + inst + L"\\Device Parameters";
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            BYTE edid[512]; DWORD size = sizeof(edid); DWORD type = 0;
            if (RegQueryValueExW(hKey, L"EDID", nullptr, &type, edid, &size) == ERROR_SUCCESS) {
                for (int b = 0; b < 4; ++b) {
                    int off = 54 + b * 18;
                    if (off + 18 > static_cast<int>(size)) break;
                    if (edid[off] == 0 && edid[off + 1] == 0 && edid[off + 2] == 0 && edid[off + 3] == 0xFC) {
                        std::string name;
                        for (int c = 5; c < 18; ++c) {
                            char ch = static_cast<char>(edid[off + c]);
                            if (ch == '\n' || ch == '\0') break;
                            name += ch;
                        }
                        RegCloseKey(hKey);
                        if (!name.empty()) return std::wstring(name.begin(), name.end());
                    }
                }
            }
            RegCloseKey(hKey);
        }
    }
    return L"";
}

BOOL CALLBACK MonitorManager::MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lp) {
    auto* v = reinterpret_cast<std::vector<HMONITOR>*>(lp);
    v->push_back(hMonitor);
    return TRUE;
}

void MonitorManager::FreeMonitors() {
    for (auto& m : m_monitors) {
        if (m.backend == BrightnessBackend::DDC && m.hPhysical) {
            DestroyPhysicalMonitor(m.hPhysical);
            m.hPhysical = nullptr;
        } else if (m.backend == BrightnessBackend::Gamma) {
            RestoreGamma(m.deviceName);
        } else if (m.backend == BrightnessBackend::SoftwareOverlay && m.hOverlay) {
            DestroyOverlayWindow(m.hOverlay);
        }
    }
    m_monitors.clear();
}

void MonitorManager::RefreshMonitors() {
    FreeMonitors();

    std::vector<HMONITOR> hmons;
    EnumDisplayMonitors(nullptr, nullptr, &MonitorManager::MonitorEnumProc, reinterpret_cast<LPARAM>(&hmons));

    std::vector<std::pair<std::wstring, int>> wmiList;
    if (m_pWmi) {
        m_pWmi->EnumerateInstances(wmiList);
    }

    int idx = 0;
    for (HMONITOR hm : hmons) {
        MonitorEntry entry;
        entry.hMonitor = hm;

        MONITORINFOEXW mi{}; mi.cbSize = sizeof(mi);
        GetMonitorInfoW(hm, &mi);
        entry.rect = mi.rcMonitor;
        entry.deviceName = mi.szDevice;

        std::wstring edidName = GetEDIDMonitorName(hm);
        if (edidName.empty()) {
            DISPLAY_DEVICEW ddMon{}; ddMon.cb = sizeof(ddMon);
            if (EnumDisplayDevicesW(mi.szDevice, 0, &ddMon, 0) && ddMon.DeviceString[0] != L'\0') {
                std::wstring devStr = ddMon.DeviceString;
                if (devStr != L"Default Monitor") {
                    edidName = devStr;
                }
            }
        }

        // 1. DDC/CI Hardware (monitores externos con DisplayPort / HDMI compatible)
        DWORD numPhys = 0;
        if (GetNumberOfPhysicalMonitorsFromHMONITOR(hm, &numPhys) && numPhys > 0) {
            std::vector<PHYSICAL_MONITOR> pms(numPhys);
            if (GetPhysicalMonitorsFromHMONITOR(hm, numPhys, pms.data())) {
                HANDLE hFirst = pms[0].hPhysicalMonitor;
                MC_VCP_CODE_TYPE vt;
                DWORD cur = 0, maxv = 100;
                if (GetVCPFeatureAndVCPFeatureReply(hFirst, 0x10, &vt, &cur, &maxv)) {
                    entry.backend   = BrightnessBackend::DDC;
                    entry.hPhysical = hFirst;
                    entry.current   = static_cast<int>(maxv > 0 ? (cur * 100) / maxv : cur);
                    entry.minVal    = 0;
                    entry.maxVal    = static_cast<int>(maxv > 0 ? maxv : 100);
                    for (DWORD k = 1; k < numPhys; ++k) DestroyPhysicalMonitor(pms[k].hPhysicalMonitor);
                } else {
                    for (DWORD k = 0; k < numPhys; ++k) DestroyPhysicalMonitor(pms[k].hPhysicalMonitor);
                }
            }
        }

        // 2. WMI (pantallas integradas de computadoras portatiles)
        if (entry.backend == BrightnessBackend::Unsupported && !wmiList.empty()) {
            std::wstring matched;
            if (!edidName.empty()) {
                for (auto& kv : wmiList) {
                    if (kv.first.find(edidName) != std::wstring::npos) { matched = kv.first; break; }
                }
            }
            if (matched.empty() && wmiList.size() == 1) matched = wmiList[0].first;
            if (!matched.empty()) {
                entry.backend = BrightnessBackend::WMI;
                entry.wmiInstance = matched;
                entry.minVal = 0;
                entry.maxVal = 100;
                for (auto& kv : wmiList) {
                    if (kv.first == matched) { entry.current = kv.second; break; }
                }
            }
        }

        // 3. GPU Gamma Ramp (Televisores HDMI, Proyectores y Monitores sin DDC/CI)
        if (entry.backend == BrightnessBackend::Unsupported) {
            HDC hdcTest = CreateDCW(entry.deviceName.c_str(), nullptr, nullptr, nullptr);
            if (hdcTest) {
                WORD testRamp[3][256];
                if (GetDeviceGammaRamp(hdcTest, testRamp)) {
                    entry.backend = BrightnessBackend::Gamma;
                    entry.minVal  = 0;
                    entry.maxVal  = 100;
                    entry.current = 100;
                }
                DeleteDC(hdcTest);
            }
        }

        // 4. Software Dimming Overlay (Fallback universal para pantallas virtuales / DisplayLink)
        if (entry.backend == BrightnessBackend::Unsupported) {
            entry.backend = BrightnessBackend::SoftwareOverlay;
            entry.minVal  = 0;
            entry.maxVal  = 100;
            entry.current = 100;
            entry.hOverlay = CreateOverlayWindow(entry.rect);
        }

        if (!edidName.empty()) {
            entry.name = edidName;
        } else {
            wchar_t buf[64];
            swprintf_s(buf, L"Pantalla %d", idx + 1);
            entry.name = buf;
        }

        m_monitors.push_back(entry);
        ++idx;
    }
}

void MonitorManager::ApplyBrightness(size_t index, int value) {
    if (index >= m_monitors.size()) return;
    value = std::clamp(value, 0, 100);

    // Actualización inmediata en memoria para respuesta gráfica instantánea
    m_monitors[index].current = value;

    // Encolar al hilo de hardware
    {
        std::lock_guard<std::mutex> lock(m_hwMutex);
        m_pendingBrightness[index] = value;
    }
    m_hwCv.notify_one();
}

void MonitorManager::HardwareWorkerThread() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInit = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

    while (m_hwRunning) {
        std::map<size_t, int> targets;
        {
            std::unique_lock<std::mutex> lock(m_hwMutex);
            m_hwCv.wait(lock, [this]() {
                return !m_hwRunning || !m_pendingBrightness.empty();
            });
            if (!m_hwRunning) break;
            targets = std::move(m_pendingBrightness);
            m_pendingBrightness.clear();
        }

        for (const auto& [idx, val] : targets) {
            HANDLE hPhys = nullptr;
            std::wstring wmiInst;
            std::wstring devName;
            BrightnessBackend backend = BrightnessBackend::Unsupported;
            int maxVal = 100;
            HWND hOverlay = nullptr;
            RECT rc{};

            if (idx < m_monitors.size()) {
                backend = m_monitors[idx].backend;
                hPhys = m_monitors[idx].hPhysical;
                wmiInst = m_monitors[idx].wmiInstance;
                devName = m_monitors[idx].deviceName;
                maxVal = m_monitors[idx].maxVal;
                hOverlay = m_monitors[idx].hOverlay;
                rc = m_monitors[idx].rect;
            }

            if (backend == BrightnessBackend::DDC && hPhys) {
                DWORD hwValue = static_cast<DWORD>((static_cast<long long>(val) * maxVal) / 100);
                if (!SetVCPFeature(hPhys, 0x10, hwValue)) {
                    // Fallback dinámico a Gamma si la televisión/adaptador no procesa el código VCP
                    SetGammaBrightness(devName, val);
                }
            } else if (backend == BrightnessBackend::WMI && !wmiInst.empty() && m_pWmi) {
                m_pWmi->SetBrightness(wmiInst, val);
            } else if (backend == BrightnessBackend::Gamma && !devName.empty()) {
                SetGammaBrightness(devName, val);
            } else if (backend == BrightnessBackend::SoftwareOverlay) {
                if (!hOverlay) {
                    m_monitors[idx].hOverlay = CreateOverlayWindow(rc);
                    hOverlay = m_monitors[idx].hOverlay;
                }
                SetOverlayAlpha(hOverlay, val);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    if (comInit) CoUninitialize();
}
