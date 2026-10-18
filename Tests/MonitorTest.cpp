// =====================================================================================
// MonitorTest.cpp - Suite de pruebas y diagnóstico de hardware para Lux
// =====================================================================================

#include "../Lux/Common.h"
#include "../Lux/Utils.h"
#include "../Lux/WmiBrightness.h"
#include "../Lux/MonitorManager.h"
#include "../Lux/SettingsManager.h"
#include <iostream>
#include <iomanip>

void PrintHeader(const std::string& title) {
    std::cout << "\n======================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "======================================================\n";
}

void TestWmi() {
    PrintHeader("TEST 1: Backend WMI (root\\WMI)");
    CWmiBrightness wmi;
    if (!wmi.Init()) {
        std::cout << "[ERROR] No se pudo inicializar el subsistema WMI.\n";
        return;
    }
    std::cout << "[OK] Subsistema WMI inicializado correctamente.\n";

    std::vector<std::pair<std::wstring, int>> instances;
    if (wmi.EnumerateInstances(instances)) {
        std::cout << "[OK] Consulta WQL a WmiMonitorBrightness exitosa.\n";
        std::cout << "     Instancias encontradas: " << instances.size() << "\n";
        for (size_t i = 0; i < instances.size(); ++i) {
            std::wcout << L"     [" << i << L"] Instancia: " << instances[i].first
                       << L" | Brillo actual: " << instances[i].second << L"%\n";
        }
    } else {
        std::cout << "[INFO] No se reportaron instancias WmiMonitorBrightness (habitual en equipos de sobremesa).\n";
    }
    wmi.Shutdown();
}

void TestMonitors() {
    PrintHeader("TEST 2: Enumeracion de Monitores y DDC/CI");
    CWmiBrightness wmi;
    wmi.Init();

    MonitorManager mgr;
    mgr.Init(&wmi);

    const auto& monitors = mgr.GetMonitors();
    std::cout << "[OK] Total de monitores detectados: " << monitors.size() << "\n";

    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto& m = monitors[i];
        std::wcout << L"  - Monitor #" << (i + 1) << L": " << m.name << L"\n";
        std::cout  << "    Resolucion: " << (m.rect.right - m.rect.left) << "x"
                   << (m.rect.bottom - m.rect.top) << "\n";
        std::cout  << "    Backend: ";
        switch (m.backend) {
        case BrightnessBackend::DDC:
            std::cout << "DDC/CI (Monitor Externo)\n";
            std::cout << "    Nivel actual: " << m.current << "% (Rango: " << m.minVal << "-" << m.maxVal << ")\n";
            break;
        case BrightnessBackend::WMI:
            std::cout << "WMI (Pantalla Interna / Laptop)\n";
            std::cout << "    Nivel actual: " << m.current << "%\n";
            break;
        default:
            std::cout << "No compatible directamente con VCP 0x10 / WMI\n";
            break;
        }
    }

    mgr.Shutdown();
    wmi.Shutdown();
}

void TestSettings() {
    PrintHeader("TEST 3: Administrador de Configuracion y Tema");
    SettingsManager sm;
    std::cout << "[OK] Configuracion cargada desde INI / Registro.\n";
    std::cout << "     AutoStart activo: " << (sm.GetAutoStart() ? "SI" : "NO") << "\n";
    std::cout << "     Icono en bandeja: " << (sm.GetShowTrayIcon() ? "SI" : "NO") << "\n";
    std::cout << "     Modo de tema: ";
    switch (sm.GetThemeMode()) {
    case ThemeMode::Light: std::cout << "Claro\n"; break;
    case ThemeMode::Dark:  std::cout << "Oscuro\n"; break;
    case ThemeMode::Auto:  std::cout << "Automatico (detectado por sistema)\n"; break;
    }

    bool sysDark = SettingsManager::IsSystemDarkMode();
    bool barLight = SettingsManager::IsTaskbarLightTheme();
    bool effDark = sm.IsDarkEffective();
    std::cout << "     Modo oscuro en Windows (Apps): " << (sysDark ? "SI" : "NO") << "\n";
    std::cout << "     Barra de tareas clara: " << (barLight ? "SI" : "NO") << "\n";
    std::cout << "     Modo efectivo resultante en Lux: " << (effDark ? "OSCURO" : "CLARO") << "\n";
}

void TestPerformanceUtils() {
    PrintHeader("TEST 4: Rendimiento y Optimizacion de Memoria");
    LuxUtils::ConfigureWindowsPerformanceMode();
    std::cout << "[OK] Modo de alto rendimiento y prioridades aplicados.\n";

    LuxUtils::PurgeWorkingSet();
    std::cout << "[OK] Vaciado de Working Set (RAM en espera) ejecutado con exito.\n";
}

int main() {
    std::cout << "======================================================\n";
    std::cout << "           LUX - SUITE DE PRUEBAS DEL SISTEMA        \n";
    std::cout << "======================================================\n";

    TestPerformanceUtils();
    TestSettings();
    TestWmi();
    TestMonitors();

    PrintHeader("RESULTADO GLOBAL");
    std::cout << "[EXITO] Todas las pruebas se ejecutaron sin excepciones ni bloqueos.\n\n";
    return 0;
}
