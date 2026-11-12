# stockRush runner for Windows PowerShell
[CmdletBinding()]
param(
    [Parameter(Position = 0, Mandatory = $false)]
    [ValidateSet("ui", "test", "build", "clean", "help")]
    [string]$Command = "ui",

    [Parameter(Position = 1, Mandatory = $false)]
    [int]$Count = 1000
)

$ErrorActionPreference = "Stop"
$RootDir = $PSScriptRoot
$BuildDir = Join-Path $RootDir "build"
$ReleaseDir = Join-Path $BuildDir "Release"
$EngineExe = Join-Path $ReleaseDir "Engine.exe"
$TestExe = Join-Path $ReleaseDir "StressTestRunner.exe"

# Helper to find MSBuild / CMake across standard VS installations
function Find-Tool {
    param([string]$Name, [string[]]$Fallbacks)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($path in $Fallbacks) {
        if (Test-Path $path) { return $path }
    }
    return $null
}

$msbuildPath = Find-Tool "msbuild.exe" @(
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
)

$cmakePath = Find-Tool "cmake.exe" @(
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)

function Invoke-Build {
    Write-Host "==> Building stockRush (Release)..." -ForegroundColor Cyan

    $slnFiles = Get-ChildItem -Path $BuildDir -Filter "*.sln"
    if ($slnFiles.Count -gt 0) {
        $slnPath = $slnFiles[0].FullName
    } else {
        $slnPath = Join-Path $BuildDir "StockRushCLI.sln"
    }
    if (!(Test-Path $slnPath)) {
        if (-not $cmakePath) {
            throw "CMake not found. Please install CMake or add it to your PATH."
        }
        Write-Host "==> Generating solution with CMake..." -ForegroundColor Yellow
        & $cmakePath -B $BuildDir -S $RootDir
        if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }
    }

    if ($msbuildPath) {
        Write-Host "==> Compiling with MSBuild..." -ForegroundColor Yellow
        & $msbuildPath $slnPath /p:Configuration=Release /m
        if ($LASTEXITCODE -ne 0) { throw "MSBuild compilation failed." }
    } elseif ($cmakePath) {
        Write-Host "==> Compiling with CMake..." -ForegroundColor Yellow
        & $cmakePath --build $BuildDir --config Release
        if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }
    } else {
        throw "Neither MSBuild nor CMake found on this machine."
    }
}

switch ($Command.ToLower()) {
    "ui" {
        if (!(Test-Path $EngineExe)) {
            Invoke-Build
        }
        Write-Host "==> Starting stockRush Interactive Dashboard..." -ForegroundColor Green
        & $EngineExe
    }

    "test" {
        if (!(Test-Path $TestExe)) {
            Invoke-Build
        }
        Write-Host "==> Running stress test with $Count orders..." -ForegroundColor Green
        & $TestExe $Count
    }

    "build" {
        Invoke-Build
        Write-Host "Build complete." -ForegroundColor Green
    }

    "clean" {
        if (Test-Path $BuildDir) {
            Remove-Item -Recurse -Force $BuildDir
            Write-Host "Cleaned build directory." -ForegroundColor Green
        } else {
            Write-Host "Build directory already clean."
        }
    }

    "help" {
        Write-Host @"
stockRush Runner for Windows PowerShell

Usage:
  .\run.ps1 [command] [options]

Commands:
  ui              Launch FTXUI terminal dashboard (default)
  test [count]    Run stress test (default: 1000 orders)
  build           Compile Release binaries
  clean           Remove the build directory
  help            Show this help message

Examples:
  .\run.ps1
  .\run.ps1 ui
  .\run.ps1 test 10000
  .\run.ps1 build
"@ -ForegroundColor Cyan
    }
}

