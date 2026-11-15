param(
    [string]$OutputDir = "release\Lux-v1.0.2-Portable",
    [string]$ZipName = "release\Lux-v1.0.2-Windows-Portable.zip",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Continue"
$ProgressPreference = "SilentlyContinue"

# Asegurar que el directorio de trabajo sea la raíz del proyecto
$ProjectRoot = Split-Path -Parent $PSScriptRoot
if ($ProjectRoot) {
    Set-Location $ProjectRoot
}

Write-Host "==> Preparando empaquetado portable de Lux..." -ForegroundColor Cyan

# 1. Compilar versión final si no se omitió
if (-not $SkipBuild) {
    Write-Host "==> Compilando versión final (Release x64)..." -ForegroundColor Yellow
    cmd.exe /c "build.bat --no-run"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Fallo en la compilación de Lux."
        exit 1
    }
}

# 2. Verificar existencia del ejecutable compilado
$sourceExe = "x64\Release\Lux.exe"
if (-not (Test-Path $sourceExe)) {
    Write-Error "No se encontró el ejecutable en '$sourceExe'. Verifica la compilación."
    exit 1
}

# 3. Limpiar y crear directorio de release
if (Test-Path $OutputDir) {
    Remove-Item -Recurse -Force $OutputDir
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

# 4. Copiar ejecutable principal
Write-Host "==> Copiando ejecutable..." -ForegroundColor Yellow
Copy-Item $sourceExe -Destination "$OutputDir\Lux.exe"

# 5. Crear archivo LEEME en el directorio portable
$readmeContent = @"
=====================================================
Lux - Control Nativo de Brillo de Pantalla (Versión Portable)
=====================================================

Esta es la versión portable de Lux.
No requiere instalación ni componentes adicionales (.NET, Electron, etc.).
Solo haz doble clic en Lux.exe para iniciar.

Características:
- Control de brillo para monitores externos por hardware (DDC/CI vía HDMI/DisplayPort).
- Control de brillo para pantallas internas de laptops (WMI / WmiMonitorBrightness).
- Soporte para Hotkeys multimedia estándar de brillo de Windows (VK_BRIGHTNESS_UP / DOWN),
  compatible con teclados de laptops y teclados externos que cuenten con estas teclas.
- Panel flotante moderno con diseño Windows 11 Fluent y esquinas redondeadas nativas.
- Sincronización automática con modo claro/oscuro de Windows.
- Consumo ultra bajo de memoria RAM (< 3 MB).
- 100% portable y autocontenido.
"@

Set-Content -Path "$OutputDir\LEEME.txt" -Value $readmeContent -Encoding UTF8

# 6. Comprimir en archivo ZIP usando .NET Compression
Write-Host "==> Comprimiendo versión portable en $ZipName..." -ForegroundColor Yellow
$zipParent = Split-Path $ZipName -Parent
if ($zipParent -and -not (Test-Path $zipParent)) {
    New-Item -ItemType Directory -Path $zipParent -Force | Out-Null
}
if (Test-Path $ZipName) {
    Remove-Item -Force $ZipName
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$absOutDir = (Resolve-Path $OutputDir).Path
$absZipPath = [System.IO.Path]::GetFullPath($ZipName)
[System.IO.Compression.ZipFile]::CreateFromDirectory($absOutDir, $absZipPath)

Write-Host "==> Paquete portable completado exitosamente!" -ForegroundColor Green
Write-Host "    Ubicación: $ZipName" -ForegroundColor Cyan
