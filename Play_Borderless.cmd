@echo off
rem Launch PW_Game in windowed mode and apply the borderless helper.
rem Requires: Profiles/default.cfg -> setvar gfx_fullscreen = 0
setlocal
set "ROOT=%~dp0"
set "BIN=%ROOT%pw\branches\r1117\Bin"
set "AHK=C:\Program Files\AutoHotkey\v1.1.37.02\AutoHotkeyU64.exe"

start "" /D "%BIN%" "%BIN%\PW_Game.exe"

if exist "%AHK%" (
    start "" "%AHK%" "%ROOT%borderless.ahk"
) else (
    echo [!] AutoHotkey not found at "%AHK%" - window will keep its border.
)
endlocal
