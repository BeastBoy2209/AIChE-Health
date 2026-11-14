# Build AIChE Health on Windows. Finds CMake even when it is not on PATH
# (for example the copy bundled with Visual Studio). Usage (from the project folder):
#   powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1 [-Run]
param([switch]$Run)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

function Find-CMake {
    $c = Get-Command cmake -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $p = & $vswhere -latest -products * -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' 2>$null | Select-Object -First 1
        if ($p -and (Test-Path $p)) { return $p }
    }
    $roots = @($env:ProgramFiles, ${env:ProgramFiles(x86)}) | Where-Object { $_ }
    foreach ($r in $roots) {
        $hit = Get-ChildItem -Path (Join-Path $r 'Microsoft Visual Studio') -Filter cmake.exe -Recurse -ErrorAction SilentlyContinue |
               Where-Object { $_.FullName -like '*CMake\bin\cmake.exe' } | Select-Object -First 1
        if ($hit) { return $hit.FullName }
        $k = Join-Path $r 'CMake\bin\cmake.exe'
        if (Test-Path $k) { return $k }
    }
    return $null
}

$cmake = Find-CMake
if (-not $cmake) {
    Write-Host 'CMake was not found.' -ForegroundColor Red
    Write-Host 'Install ONE of these, then run this script again:'
    Write-Host '  1) Visual Studio 2022/2026 Community with the workload "Desktop development with C++" (includes CMake and the compiler)'
    Write-Host '  2) CMake from https://cmake.org/download/ (tick "Add CMake to the system PATH") plus Visual Studio Build Tools'
    exit 1
}
Write-Host "Using CMake: $cmake"

& $cmake -S . -B build
if ($LASTEXITCODE -ne 0) { Write-Host 'Configure failed. Is the C++ compiler installed (Visual Studio "Desktop development with C++")?' -ForegroundColor Red; exit 1 }
& $cmake --build build --config Release -j 8
if ($LASTEXITCODE -ne 0) { Write-Host 'Build failed.' -ForegroundColor Red; exit 1 }

$exe = Join-Path (Get-Location) 'build\ui\Release\aiche-health.exe'
Write-Host "`nDone: $exe" -ForegroundColor Green
if ($Run) { Push-Location build; & $exe --profile demo_expedition.json; Pop-Location }
else { Write-Host 'Run it with:  cd build; .\ui\Release\aiche-health.exe   (add --profile demo_expedition.json for the demo)' }
