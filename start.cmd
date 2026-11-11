@echo off
rem Build (if needed) and start the handme server on Windows.
rem Thin wrapper around start.ps1: it works when double-clicked from Explorer
rem and is not blocked by the PowerShell execution policy.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0start.ps1" %*
exit /b %ERRORLEVEL%
