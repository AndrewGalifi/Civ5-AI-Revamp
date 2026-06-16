param(
    [string]$SourceDll,
    [string]$GameRoot,
    [string]$GameDll,
    [string]$DesktopCopyName = "CvGameCore_Expansion2_project_latest.dll",
    [switch]$NoBackup,
    [switch]$NoDesktopCopy,
    [switch]$NoAdminRelaunch
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$localPaths = Join-Path $repoRoot "local.paths.ps1"
if (Test-Path -LiteralPath $localPaths) {
    . $localPaths
}

if (-not $SourceDll) {
    $SourceDll = Join-Path $repoRoot "CvGameCoreSource\BuildOutput\VS2013_ModWin32\CvGameCoreDLL_Expansion2Win32Mod.dll"
}

if (-not $GameRoot) {
    if ($env:CIV5_GAME_ROOT) {
        $GameRoot = $env:CIV5_GAME_ROOT
    }
    elseif ($Civ5GameRoot) {
        $GameRoot = $Civ5GameRoot
    }
    else {
        $GameRoot = "C:\Program Files (x86)\Steam\steamapps\common\Sid Meier's Civilization V"
    }
}

if (-not $GameDll) {
    $GameDll = Join-Path $GameRoot "Assets\DLC\Expansion2\CvGameCore_Expansion2.dll"
}

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin -and -not $NoAdminRelaunch) {
    function Quote-ProcessArgument {
        param([string]$Argument)
        return '"' + $Argument.Replace('"', '\"') + '"'
    }

    $arguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", (Quote-ProcessArgument $PSCommandPath),
        "-SourceDll", (Quote-ProcessArgument $SourceDll),
        "-GameRoot", (Quote-ProcessArgument $GameRoot),
        "-GameDll", (Quote-ProcessArgument $GameDll),
        "-DesktopCopyName", (Quote-ProcessArgument $DesktopCopyName),
        "-NoAdminRelaunch"
    )

    if ($NoBackup) { $arguments += "-NoBackup" }
    if ($NoDesktopCopy) { $arguments += "-NoDesktopCopy" }

    Write-Host "Restarting as administrator..."
    Start-Process powershell.exe -Verb RunAs -ArgumentList ($arguments -join " ")
    exit
}

if (-not (Test-Path -LiteralPath $SourceDll)) {
    throw "Built DLL not found: $SourceDll"
}

if (-not (Test-Path -LiteralPath $GameDll)) {
    throw "Game DLL not found: $GameDll"
}

$runningCiv = Get-Process | Where-Object {
    $_.ProcessName -like "CivilizationV*" -or
    $_.ProcessName -like "Launcher*" -or
    $_.ProcessName -like "FireTuner*"
}

if ($runningCiv) {
    $names = ($runningCiv | Select-Object -ExpandProperty ProcessName -Unique) -join ", "
    throw "Close Civ V / FireTuner first. Running process(es): $names"
}

$desktop = [Environment]::GetFolderPath("Desktop")

if (-not $NoDesktopCopy) {
    $desktopCopy = Join-Path $desktop $DesktopCopyName
    Copy-Item -LiteralPath $SourceDll -Destination $desktopCopy -Force
}

if (-not $NoBackup) {
    $backupDll = Join-Path $desktop ("CvGameCore_Expansion2_backup_{0}.dll" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
    Copy-Item -LiteralPath $GameDll -Destination $backupDll -Force
}

Copy-Item -LiteralPath $SourceDll -Destination $GameDll -Force

Write-Host "Installed Civ V DLL."
Write-Host "Source: $SourceDll"
if ($desktopCopy) { Write-Host "Desktop copy: $desktopCopy" }
if ($backupDll) { Write-Host "Backup: $backupDll" }
Write-Host "Target: $GameDll"
