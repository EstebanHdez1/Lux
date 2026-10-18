#include "LuxApp.h"

// ---------------------------------------------------------------------------
// Punto de entrada de la aplicación
// ---------------------------------------------------------------------------
int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    // Soporte para DPI por monitor de alta definición en Windows 10/11
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Evitar múltiples instancias en ejecución simultánea
    SetLastError(0);
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\Lux_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    int ret = 0;
    if (LuxApp::Instance().Init(hInst)) {
        ret = LuxApp::Instance().Run();
    }

    if (hMutex) CloseHandle(hMutex);
    return ret;
}
