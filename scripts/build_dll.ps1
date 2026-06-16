param(
    [string]$MSBuildPath,
    [string]$SolutionPath,
    [string]$Target = "CvGameCoreDLL_Expansion2",
    [string]$Configuration = "Mod",
    [string]$Platform = "Win32",
    [switch]$PreservePath
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

$dllPath = Join-Path $repoRoot "CvGameCoreSource\BuildOutput\VS2013_ModWin32\CvGameCoreDLL_Expansion2Win32Mod.dll"
if (Test-Path -LiteralPath $dllPath) {
    Write-Host "Built DLL: $dllPath"
}

exit 0
