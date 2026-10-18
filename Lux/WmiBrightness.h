#pragma once
#include "Common.h"

class CWmiBrightness {
public:
    CWmiBrightness() = default;
    ~CWmiBrightness();

    bool Init();
    void Shutdown();

    bool EnumerateInstances(std::vector<std::pair<std::wstring, int>>& out);
    bool SetBrightness(const std::wstring& instanceName, int value);

private:
    std::mutex     m_wmiMutex;
    IWbemLocator*  m_pLoc = nullptr;
    IWbemServices* m_pSvc = nullptr;
    bool           m_comInitialized = false;
};
