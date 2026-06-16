@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install_latest_civ5_dll.ps1" %*
pause
