@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\civ5_dll.ps1" -Install %*
exit /b %ERRORLEVEL%
