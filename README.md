# Lux ☀️

> Aplicación nativa y ultraligera para Windows (C++17 / Win32 / GDI+) diseñada para controlar el brillo de monitores externos e internos desde la bandeja del sistema, con estética moderna de Windows 11.

---

## 📋 Tabla de Contenidos

- [Características Principales](#-características-principales)
- [Estructura del Proyecto](#-estructura-del-proyecto)
- [Arquitectura Modular](#-arquitectura-modular)
- [Requisitos](#-requisitos)
- [Compilación y Construcción](#-compilación-y-construcción)
  - [Opción 1: Visual Studio 2022](#opción-1-visual-studio-2022-recomendado)
  - [Opción 2: MSBuild (Línea de Comandos)](#opción-2-msbuild-línea-de-comandos)
- [Ejecución y Pruebas](#-ejecución-y-pruebas)
- [Descargas y Releases](#-descargas-y-releases)
- [Licencia](#-licencia)

---

## ✨ Características Principales

- **Cero frameworks pesados**: Desarrollado en **C++17 puro con Win32 y GDI+**. Sin Electron, sin .NET, sin dependencias externas complejas.
- **Doble backend de brillo**:
  - **DDC/CI**: Control de brillo por hardware para monitores externos conectados por DisplayPort / HDMI mediante llamadas nativas VCP (código `0x10`).
  - **WMI (`root\WMI`)**: Control para pantallas integradas en computadoras portátiles (`WmiMonitorBrightness`).
- **Respuesta instantánea (Cero Delay)**: Hilo de comunicación de hardware en segundo plano con colas no bloqueantes.
- **Diseño Windows 11 Fluent**:
  - Esquinas redondeadas nativas mediante API DWM (`DWMWA_WINDOW_CORNER_PREFERENCE`).
  - Modo oscuro y claro sincronizado con el sistema o configurable manualmente.
  - Subclasificación oscura completa para menús nativos y controles desplegables (ComboBox).
  - Iconos vectoriales dibujados en tiempo de ejecución (icono del sol estilo Tabler en bandeja y controles).
- **Consumo mínimo de memoria RAM**: Optimización automática mediante vaciado de páginas en espera (`PurgeWorkingSet`), consumiendo habitualmente menos de 3 MB de memoria en segundo plano.
- **Ajustes rápidos**: Control de brillo mediante rueda del ratón (`MouseWheel`) o arrastre del deslizador.

---

## 📁 Estructura del Proyecto

El repositorio está estructurado en módulos claros y desacoplados:

```text
├── Lux.sln                 # Solución completa de Visual Studio 2022
├── .gitignore              # Configuración de exclusión para Git y VS
├── README.md               # Documentación principal del proyecto
│
├── Lux/                    # Código fuente de la aplicación principal
│   ├── Lux.vcxproj         # Proyecto C++ de la aplicación (Win32 GUI)
│   ├── Common.h            # Constantes, estructuras y tipos comunes
│   ├── Utils.h / .cpp      # Utilidades del sistema, DPI y APIs de modo oscuro
│   ├── WmiBrightness.h/.cpp # Controlador de brillo WMI para pantallas internas
│   ├── MonitorManager.h/.cpp # Detección EDID, DDC/CI y worker thread asíncrono
│   ├── SettingsManager.h/.cpp # Gestión de .ini, autoinicio en registro y tema
│   ├── TrayIcon.h / .cpp   # Icono vectorial procedural y menú contextual
│   ├── SettingsWindow.h/.cpp # Ventana modal de ajustes y controles owner-draw
│   ├── LuxApp.h / .cpp     # Coordinador principal de la app y panel flotante
│   └── main.cpp            # Punto de entrada (wWinMain) y mutex de instancia única
│
├── Releases/               # Directorio para binarios estables listos para usar
│   ├── Lux.exe             # Ejecutable compilado en modo Release (x64)
│   └── README.md           # Guía de uso portátil
│
└── Tests/                  # Suite de pruebas y diagnósticos de hardware
    ├── Tests.vcxproj       # Proyecto de consola para pruebas
    ├── MonitorTest.cpp     # Diagnóstico de monitores, WMI y persistencia
    └── RunTests.ps1        # Script PowerShell para compilar y ejecutar pruebas
```

---

## 🏛️ Arquitectura Modular

La aplicación ha sido desglosada en clases con responsabilidades únicas:

| Clase / Módulo | Responsabilidad |
|---|---|
| `LuxApp` | Orquestador central. Gestiona el bucle de mensajes Win32, la ventana oculta de eventos y el renderizado antialiased del panel flotante. |
| `MonitorManager` | Enumeración física de pantallas, lectura de nombres EDID desde el registro, comunicación DDC/CI y cola en segundo plano (`std::thread`). |
| `CWmiBrightness` | Conexión con `ROOT\WMI`, consulta de instancias WQL y ejecución del método `WmiSetBrightness`. |
| `SettingsManager` | Persistencia en archivo `Lux.ini`, consulta/escritura de autoarranque en `HKCU\...\Run` y detección de modo oscuro/claro del sistema. |
| `TrayIcon` | Generación procedural en mapa de bits del icono vectorial del sol y control de notificaciones en la bandeja (`Shell_NotifyIcon`). |
| `SettingsWindow` | Ventana de ajustes con interruptores tipo píldora (owner-draw) y subclasificación de combobox para modo oscuro nativo. |
| `LuxUtils` | Funciones auxiliares para escalado DPI (`GetDpiForWindow`), APIs no documentadas de `uxtheme.dll` y vaciado de memoria (`PurgeWorkingSet`). |

---

## 💻 Requisitos

- **Sistema Operativo**: Windows 10 (versión 2004 o superior) o Windows 11 (recomendado para esquinas redondeadas y temas avanzados).
- **Herramientas de Compilación**:
  - Visual Studio 2022 (Community, Professional o Enterprise) con el paquete *Desarrollo para el escritorio con C++* (v143).
  - Windows 10 / 11 SDK.

---

## 🔨 Compilación y Construcción

### Opción 1: Visual Studio 2022 (Recomendado)

1. Abre el archivo `Lux.sln` en Visual Studio 2022.
2. Selecciona la configuración **`Release`** y plataforma **`x64`**.
3. Presiona `Ctrl + Shift + B` (o menú *Compilar > Compilar solución*).

### Opción 2: MSBuild (Línea de Comandos)

Desde una consola de comandos con entorno de Visual Studio:

```bat
MSBuild Lux.sln /p:Configuration=Release /p:Platform=x64
```

---

## 🧪 Ejecución y Pruebas

Para verificar que el sistema operativo y tus monitores soportan DDC/CI y WMI sin necesidad de abrir la interfaz gráfica:

Ejecuta el script incluido en la carpeta `Tests`:

```powershell
powershell -ExecutionPolicy Bypass -File Tests\RunTests.ps1
```

El test imprimirá un informe detallado con:
- Rendimiento y vaciado de memoria.
- Estado de temas y registro de configuración.
- Diagnóstico del subsistema WMI.
- Nombre EDID, resolución y backend compatible de cada monitor detectado.

---

## 📦 Descargas y Releases

En la carpeta [`Releases/`](Releases/) se incluye el binario precompilado `Lux.exe`. Es 100% portable y no requiere instalación ni privilegios de administrador.
