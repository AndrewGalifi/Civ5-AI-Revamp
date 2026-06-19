param(
    [switch]$Install,
    [string]$MSBuildPath,
    [string]$SolutionPath,
    [string]$Target = "CvGameCoreDLL_Expansion2",
    [string]$Configuration = "Mod",
    [string]$Platform = "Win32",
    [switch]$PreservePath,
    [string]$SourceDll,
    [string]$GameRoot,
    [string]$GameDll,
    [switch]$NoAdminRelaunch
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$localPaths = Join-Path $repoRoot "local.paths.ps1"
if (Test-Path -LiteralPath $localPaths) {
    . $localPaths
}

function Resolve-FirstExistingPath {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

function Quote-ProcessArgument {
    param([string]$Argument)
    return '"' + $Argument.Replace('"', '\"') + '"'
}

if (-not $MSBuildPath) {
    $MSBuildPath = Resolve-FirstExistingPath @(
        $env:CIV5_MSBUILD_PATH,
        $Civ5MSBuildPath,
        "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\17\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
    )
}

if (-not $MSBuildPath) {
    throw "MSBuild.exe not found. Set CIV5_MSBUILD_PATH or copy local.paths.example.ps1 to local.paths.ps1."
}

if (-not $SolutionPath) {
    $SolutionPath = Join-Path $repoRoot "CvGameCoreSource\CvGameCoreDLL.vs2013.sln"
}

if (-not (Test-Path -LiteralPath $SolutionPath)) {
    throw "Solution not found: $SolutionPath"
}

if ($Install) {
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
        $arguments = @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", (Quote-ProcessArgument $PSCommandPath),
            "-Install",
            "-MSBuildPath", (Quote-ProcessArgument $MSBuildPath),
            "-SolutionPath", (Quote-ProcessArgument $SolutionPath),
            "-Target", (Quote-ProcessArgument $Target),
            "-Configuration", (Quote-ProcessArgument $Configuration),
            "-Platform", (Quote-ProcessArgument $Platform),
            "-GameRoot", (Quote-ProcessArgument $GameRoot),
            "-GameDll", (Quote-ProcessArgument $GameDll),
            "-NoAdminRelaunch"
        )

        if ($SourceDll) { $arguments += @("-SourceDll", (Quote-ProcessArgument $SourceDll)) }
        if ($PreservePath) { $arguments += "-PreservePath" }

        Write-Host "Restarting as administrator..."
        Start-Process powershell.exe -Verb RunAs -ArgumentList ($arguments -join " ")
        exit
    }
}

$oldPath = $env:PATH
try {
    if (-not $PreservePath) {
        $env:PATH = ""
    }

    & $MSBuildPath $SolutionPath "/t:$Target" "/p:Configuration=$Configuration" "/p:Platform=$Platform" /m
    $exitCode = $LASTEXITCODE
}
finally {
    $env:PATH = $oldPath
}

if ($exitCode -ne 0) {
    exit $exitCode
}

if (-not $SourceDll) {
    $SourceDll = Join-Path $repoRoot "CvGameCoreSource\BuildOutput\VS2013_ModWin32\CvGameCoreDLL_Expansion2Win32Mod.dll"
}

if (Test-Path -LiteralPath $SourceDll) {
    Write-Host "Built DLL: $SourceDll"
}

if (-not $Install) {
    exit 0
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

Copy-Item -LiteralPath $SourceDll -Destination $GameDll -Force

Write-Host "Installed Civ V DLL."
Write-Host "Source: $SourceDll"
Write-Host "Target: $GameDll"
