## Strategic Planning Layer for Civilization V AI

An in-progress revamp of Civ5's AI, most notably making a strategy directive from a turn-by-turn game-state summary.


The core idea is:

1. Build a compact `GameStateSummary` from the current turn.
2. Select a `StrategyDirective` such as expansion, development, military response, treasury recovery, or happiness recovery.
3. Apply scoped biases and/or hard pivots to existing AI subsystems (production, food, science, etc).

Note: current source modifications only affect player 1 for testing purposes, and for now are only designed for Quick game speed. 

## Repository Layout

- `CvGameCoreSource/` contains the Civ V SDK source tree.
- `CvGameCoreSource/CvGameCoreDLL_Expansion2/` contains the main modified C++ code.
- `dev_build_install.cmd` is the main local build/install entrypoint.
- `scripts/civ5_dll.ps1` contains the underlying build/install implementation.
- `sample_logs/` contains small sample CSVs of AI behavior.
- `Actions -> Civ5 Build -> specific run -> Artifacts` to download a specific build's DLL.


## Building

Copy the local path template:

```powershell
Copy-Item local.paths.example.ps1 local.paths.ps1
```

Edit `local.paths.ps1` if your Visual Studio/MSBuild or Civ V install path differs.

Build the DLL:

```powershell
.\scripts\civ5_dll.ps1
```

The expected output is:

```text
CvGameCoreSource\BuildOutput\VS2013_ModWin32\CvGameCoreDLL_Expansion2Win32Mod.dll
```

## Installing Locally

Close Civ V and FireTuner, then run:

```powershell
.\dev_build_install.cmd
```

The install step copies the latest built DLL into the local Civ V BNW DLL path.

## Local Test Loop

Build and install in one step:

```powershell
.\dev_build_install.cmd
```

To only build through the same wrapper:

```powershell
.\scripts\civ5_dll.ps1
```

## GitHub Actions

`.github/workflows/civ5-local-runner.yml` is for a Windows self-hosted GitHub Actions runner that has Visual Studio/MSBuild installed.

Recommended runner labels:

```text
self-hosted
Windows
civ5
```

The workflow builds on pushes to `main` or `dev` and uploads the DLL artifact. It can also be run manually from GitHub Actions. Installing into the local Civ V folder is intentionally kept out of GitHub Actions; use `.\dev_build_install.cmd` for local build/install testing.
