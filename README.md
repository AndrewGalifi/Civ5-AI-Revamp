# Strategic Planning Layer for Civilization V AI

An in-progress revamp of Civ5's AI, most notably making a strategy directive from a turn-by-turn game-state summary.


The core idea is:

1. Build a compact `GameStateSummary` from the current turn.
2. Select a `StrategyDirective` such as expansion, development, military response, treasury recovery, or happiness recovery.
3. Apply scoped biases and/or hard pivots to existing AI subsystems (production, food, science, etc).


# Repository Layout

- `CvGameCoreSource/` contains the Civ V SDK source tree.
- `CvGameCoreSource/CvGameCoreDLL_Expansion2/` contains the main modified C++ code.
- `scripts/build_dll.ps1` builds the BNW DLL with MSBuild.
- `scripts/install_latest_civ5_dll.ps1` copies the latest built DLL into a local Civ V install.
- `sample_logs/` contains small sample CSVs of AI behavior.


## Building

Copy the local path template:

```powershell
Copy-Item local.paths.example.ps1 local.paths.ps1
```

Edit `local.paths.ps1` if your Visual Studio/MSBuild or Civ V install path differs.

Build the DLL:

```powershell
.\buildDLL.cmd
```

The expected output is:

```text
CvGameCoreSource\BuildOutput\VS2013_ModWin32\CvGameCoreDLL_Expansion2Win32Mod.dll
```

## Installing Locally

Close Civ V and FireTuner, then run:

```powershell
.\install_latest_civ5_dll.cmd
```

The install script copies the latest built DLL into the local Civ V BNW DLL path and writes a timestamped backup to the Desktop.

