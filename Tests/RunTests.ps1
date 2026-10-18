# Script para compilar y ejecutar las pruebas de Lux

$ErrorActionPreference = "Stop"

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msbuild = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"

Write-Host "Compilando Tests.vcxproj con MSBuild..." -ForegroundColor Cyan
& $msbuild (Join-Path $PSScriptRoot "Tests.vcxproj") /p:Configuration=Release /p:Platform=x64 /nologo

$testExe = Join-Path $PSScriptRoot "x64\Release\Tests.exe"
if (Test-Path $testExe) {
    Write-Host "`nEjecutando suite de pruebas de Lux...`n" -ForegroundColor Green
    & $testExe
} else {
    Write-Error "No se encontró el ejecutable de pruebas."
}
