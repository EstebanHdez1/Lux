#include "WmiBrightness.h"

CWmiBrightness::~CWmiBrightness() {
    Shutdown();
}

bool CWmiBrightness::Init() {
    std::lock_guard<std::mutex> lock(m_wmiMutex);
    if (m_pSvc) return true; // ya inicializado

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    m_comInitialized = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

    CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                          RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                          nullptr, EOAC_NONE, nullptr);

    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                           IID_IWbemLocator, reinterpret_cast<LPVOID*>(&m_pLoc));
    if (FAILED(hr) || !m_pLoc) return false;

    hr = m_pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr,
                                0, nullptr, nullptr, &m_pSvc);
    if (FAILED(hr) || !m_pSvc) return false;

    CoSetProxyBlanket(m_pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                       RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    return true;
}

void CWmiBrightness::Shutdown() {
    std::lock_guard<std::mutex> lock(m_wmiMutex);
    if (m_pSvc) { m_pSvc->Release(); m_pSvc = nullptr; }
    if (m_pLoc) { m_pLoc->Release(); m_pLoc = nullptr; }
    if (m_comInitialized) { CoUninitialize(); m_comInitialized = false; }
}

bool CWmiBrightness::EnumerateInstances(std::vector<std::pair<std::wstring, int>>& out) {
    std::lock_guard<std::mutex> lock(m_wmiMutex);
    if (!m_pSvc) return false;

    IEnumWbemClassObject* pEnum = nullptr;
    HRESULT hr = m_pSvc->ExecQuery(
        _bstr_t(L"WQL"), _bstr_t(L"SELECT * FROM WmiMonitorBrightness"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnum);
    if (FAILED(hr) || !pEnum) return false;

    IWbemClassObject* pObj = nullptr;
    ULONG got = 0;
    while (pEnum->Next(WBEM_INFINITE, 1, &pObj, &got) == S_OK) {
        VARIANT vInst{}, vBright{};
        VariantInit(&vInst); VariantInit(&vBright);
        pObj->Get(L"InstanceName", 0, &vInst, nullptr, nullptr);
        pObj->Get(L"CurrentBrightness", 0, &vBright, nullptr, nullptr);
        std::wstring inst = (vInst.vt == VT_BSTR && vInst.bstrVal) ? vInst.bstrVal : L"";
        int cur = (vBright.vt == VT_UI1) ? vBright.bVal : 0;
        if (!inst.empty()) out.emplace_back(inst, cur);
        VariantClear(&vInst); VariantClear(&vBright);
        pObj->Release();
    }
    pEnum->Release();
    return true;
}

bool CWmiBrightness::SetBrightness(const std::wstring& instanceName, int value) {
    std::lock_guard<std::mutex> lock(m_wmiMutex);
    if (!m_pSvc) return false;
    std::wstring path = L"WmiMonitorBrightnessMethods.InstanceName=\"" + instanceName + L"\"";

    IWbemClassObject *pClass = nullptr, *pInDef = nullptr, *pIn = nullptr, *pOut = nullptr;
    HRESULT hr = m_pSvc->GetObject(_bstr_t(L"WmiMonitorBrightnessMethods"), 0, nullptr, &pClass, nullptr);
    if (FAILED(hr) || !pClass) return false;

    hr = pClass->GetMethod(L"WmiSetBrightness", 0, &pInDef, nullptr);
    if (FAILED(hr) || !pInDef) { pClass->Release(); return false; }

    pInDef->SpawnInstance(0, &pIn);
    VARIANT vTimeout{}, vBrightness{};
    VariantInit(&vTimeout); vTimeout.vt = VT_I4; vTimeout.lVal = 1;
    VariantInit(&vBrightness); vBrightness.vt = VT_UI1; vBrightness.bVal = static_cast<BYTE>(value);
    pIn->Put(L"Timeout", 0, &vTimeout, 0);
    pIn->Put(L"Brightness", 0, &vBrightness, 0);

    hr = m_pSvc->ExecMethod(_bstr_t(path.c_str()), _bstr_t(L"WmiSetBrightness"), 0, nullptr, pIn, &pOut, nullptr);

    VariantClear(&vTimeout); VariantClear(&vBrightness);
    if (pIn) pIn->Release();
    if (pInDef) pInDef->Release();
    if (pOut) pOut->Release();
    pClass->Release();
    return SUCCEEDED(hr);
}
