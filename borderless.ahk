; borderless.ahk  (AutoHotkey v1.1)
; Turns the windowed PW_Game frame window into a borderless window that fills
; the primary monitor -> "fullscreen windowed" without recompiling the engine.
;
; The engine uses two top-level windows:
;   - "Prime World ..."  -> the frame/input window (has the caption we strip)
;   - "D3DProxyWindow"    -> the actual render surface (already desktop-sized)
; We target the FRAME by its title and the proxy follows.
;
; Requires Profiles/default.cfg: setvar gfx_fullscreen = 0  (windowed mode)

#NoEnv
#SingleInstance, Force
#Persistent
SetTitleMatchMode, 2          ; substring title match
DetectHiddenWindows, On
SetWinDelay, -1

; WS_CAPTION 0x00C00000 | WS_THICKFRAME 0x00040000 = 0x00C40000
StripBits := 0x00C40000
frame := "Prime World ahk_exe PW_Game.exe"

; Wait for the game to start (up to 120s)
WinWait, ahk_exe PW_Game.exe,, 120
if ErrorLevel
    ExitApp

; Keep the frame borderless+maximized for as long as the game runs.
Loop {
    if !WinExist("ahk_exe PW_Game.exe")
        break                                    ; game closed -> quit helper

    if WinExist(frame) {
        WinGet, st, Style, %frame%
        WinGetPos, , , ww, wh, %frame%
        ; (re)apply when caption is present, size drifted, or window minimized
        if ((st & 0xC00000) or (ww != A_ScreenWidth) or (wh != A_ScreenHeight) or (st & 0x20000000)) {
            WinRestore, %frame%
            WinShow, %frame%
            WinSet, Style, % "-" StripBits, %frame%
            WinMove, %frame%, , 0, 0, A_ScreenWidth, A_ScreenHeight
        }
    }
    Sleep, 1000
}
ExitApp
