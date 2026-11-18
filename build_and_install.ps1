# DRIPMIX - build + install for Windows (Ableton Live, VST3)
# Run via INSTALL.bat (double-click). Safe to run again any time to rebuild.

$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot

function Say($msg, $color = 'Cyan') { Write-Host $msg -ForegroundColor $color }
function Fail($msg) {
    Write-Host ""
    Write-Host "  X  $msg" -ForegroundColor Red
    Write-Host ""
    Read-Host "Press Enter to close"
    exit 1
}

Say ""
Say "  ____  ____  ___ ____  __  __ _____  __" 'Magenta'
Say " |  _ \|  _ \|_ _|  _ \|  \/  |_ _\ \/ /" 'Magenta'
Say " | | | | |_) || || |_) | |\/| || | \  / " 'Magenta'
Say " | |_| |  _ < | ||  __/| |  | || | /  \ " 'Magenta'
Say " |____/|_| \_\___|_|   |_|  |_|___/_/\_\" 'Magenta'
Say "        trap mix coach - installer" 'Magenta'
Say ""

if ($PSScriptRoot.Length -gt 80) {
    Say "Heads up: this folder path is long. If the build fails with 'path too long', move the DripMix folder to C:\DripMix and run again." 'Yellow'
}

# ---------------------------------------------------------------------------
# 1. Find Visual Studio C++ build tools
# ---------------------------------------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}

if (-not $vsPath) {
    Say "[1/4] You need Microsoft's free C++ build tools (one-time, ~3-6 GB)." 'Yellow'
    $answer = Read-Host "      Install them now with winget? (Y/N)"
    if ($answer -notmatch '^[Yy]') {
        Fail "Install 'Build Tools for Visual Studio' with the 'Desktop development with C++' workload, then run INSTALL.bat again: https://visualstudio.microsoft.com/downloads/"
    }
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        Fail "winget isn't available. Install 'Build Tools for Visual Studio' manually (Desktop development with C++) from https://visualstudio.microsoft.com/downloads/ then run INSTALL.bat again."
    }
    Say "      Installing... a Visual Studio Installer window will show progress. This takes a while." 'Yellow'
    winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-package-agreements --accept-source-agreements `
        --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }
    if (-not $vsPath) { Fail "Build tools still not found. Restart your PC and run INSTALL.bat again." }
}
Say "[1/4] C++ build tools: $vsPath" 'Green'

# ---------------------------------------------------------------------------
# 2. Find CMake (the one bundled with Visual Studio first)
# ---------------------------------------------------------------------------
$cmake = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) {
    $c = Get-Command cmake -ErrorAction SilentlyContinue
    if ($c) { $cmake = $c.Source }
    else { Fail "CMake not found. Open 'Visual Studio Installer' > Modify > tick 'C++ CMake tools for Windows', then run INSTALL.bat again." }
}
Say "[2/4] CMake: $cmake" 'Green'

# ---------------------------------------------------------------------------
# 3. Build (first run downloads JUCE, ~5 min; later runs are quick)
# ---------------------------------------------------------------------------
Say "[3/4] Building DRIPMIX (first time takes a few minutes)..." 'Yellow'
$buildDir = Join-Path $PSScriptRoot 'build'

& $cmake -S $PSScriptRoot -B $buildDir -A x64
if ($LASTEXITCODE -ne 0) {
    # A leftover build folder from a different Visual Studio version can confuse CMake: start fresh once.
    Say "      Retrying with a clean build folder..." 'Yellow'
    Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue
    & $cmake -S $PSScriptRoot -B $buildDir -A x64
    if ($LASTEXITCODE -ne 0) { Fail "CMake configure failed (scroll up for the error). Check your internet connection - the first build downloads JUCE." }
}

& $cmake --build $buildDir --config Release --target DripMix_VST3 --parallel
if ($LASTEXITCODE -ne 0) { Fail "Build failed (scroll up for the first 'error')." }

$built = Join-Path $buildDir 'DripMix_artefacts\Release\VST3\DRIPMIX.vst3'
if (-not (Test-Path $built)) { Fail "Build finished but DRIPMIX.vst3 wasn't found at $built" }
Say "      Built: $built" 'Green'

# ---------------------------------------------------------------------------
# 4. Install to the standard VST3 folder (needs admin, you'll get a prompt)
# ---------------------------------------------------------------------------
$dest = Join-Path $env:CommonProgramFiles 'VST3'
Say "[4/4] Installing to $dest ..." 'Yellow'

$live = Get-Process -Name 'Ableton Live*' -ErrorAction SilentlyContinue
if ($live) {
    Say "      Ableton is open. Close it first so the old plugin file isn't locked." 'Yellow'
    Read-Host "      Close Ableton, then press Enter"
}

$qDest  = $dest.Replace("'", "''")
$qBuilt = $built.Replace("'", "''")
$copyCmd = "New-Item -ItemType Directory -Force -Path '$qDest' | Out-Null; " +
           "if (Test-Path '$qDest\DRIPMIX.vst3') { Remove-Item -Recurse -Force '$qDest\DRIPMIX.vst3' }; " +
           "Copy-Item -Recurse -Force -LiteralPath '$qBuilt' -Destination '$qDest'"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
try {
    if ($isAdmin) {
        Invoke-Expression $copyCmd
    } else {
        $p = Start-Process powershell -Verb RunAs -Wait -PassThru -WindowStyle Hidden `
                 -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', $copyCmd)
        if ($p.ExitCode -ne 0) { throw "copy exited with code $($p.ExitCode)" }
    }
} catch {
    Fail "Couldn't copy into $dest ($_). Copy the DRIPMIX.vst3 folder there yourself from: $built"
}

if (-not (Test-Path (Join-Path $dest 'DRIPMIX.vst3'))) { Fail "Install check failed - copy $built into $dest manually." }

Say ""
Say "  DRIPMIX installed." 'Green'
Say ""
Say "  In Ableton:" 'White'
Say "   1. Options > Preferences (Ctrl + ,) > Plug-ins tab" 'White'
Say "   2. Turn ON 'Use VST3 Plug-in System Folders', then hit Rescan" 'White'
Say "   3. Browser > Plug-ins > VST3 > DripLabs > DRIPMIX" 'White'
Say "   4. Drop it LAST on Kick, 808, Vox, Hats, Melody... and on your Master" 'White'
Say ""
Read-Host "Press Enter to close"
