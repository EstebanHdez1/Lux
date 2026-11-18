#include "LuxApp.h"

// ---------------------------------------------------------------------------
// Punto de entrada de la aplicación
// ---------------------------------------------------------------------------
int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmdLine, int) {
    // Soporte para DPI por monitor de alta definición en Windows 10/11
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    bool isRestart = (lpCmdLine && (wcsstr(lpCmdLine, L"--restart") != nullptr || wcsstr(lpCmdLine, L"--updated") != nullptr));

    // Evitar múltiples instancias en ejecución simultánea
    SetLastError(0);
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\Lux_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (isRestart) {
            // Si la aplicación se reinició tras una actualización, la instancia previa
            // está saliendo; reintentar adquirir el mutex durante hasta 3 segundos.
            for (int i = 0; i < 30; ++i) {
                Sleep(100);
                if (hMutex) CloseHandle(hMutex);
                SetLastError(0);
                hMutex = CreateMutexW(nullptr, TRUE, L"Local\\Lux_SingleInstance");
                if (GetLastError() != ERROR_ALREADY_EXISTS) {
                    break;
                }
            }
        }
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            if (hMutex) CloseHandle(hMutex);
            return 0;
        }
    }

    int ret = 0;
    if (LuxApp::Instance().Init(hInst)) {
        ret = LuxApp::Instance().Run();
    }

    if (hMutex) CloseHandle(hMutex);
    return ret;
}
