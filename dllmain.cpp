#include "stdafx.h"
#include "stdio.h"
#include "includes\injector\injector.hpp"
#include "includes\IniReader.h"

bool bBorderlessWindowed = false;
bool bEnableWindowResize = false;
bool bEnableConsole = false;
bool bAllowMultipleInstances = false;
bool bRelocateWindow = false;

int RelocateX = 0;
int RelocateY = 0;

int DesktopX = 0;
int DesktopY = 0;

HWND GameHWND = NULL;
tagRECT windowRect;

void ReadFullscreenSetting(char* IniPath)
{
    // restore the config read
    *(int*)((*(int*)0x823F60) + 0xD8) = GetPrivateProfileIntA("CONFIG", "Fullscreen", 1, IniPath);
}

int InitConfig()
{
    CIniReader inireader("");
    bBorderlessWindowed = inireader.ReadInteger("TMNTHax", "BorderlessWindowed", 0);
    // breaks mouse positioning... needs aspect ratio recalculation from 16:9
    bEnableWindowResize = inireader.ReadInteger("TMNTHax", "EnableWindowResize", 0);
    bEnableConsole = inireader.ReadInteger("TMNTHax", "EnableConsole", 0);
    bAllowMultipleInstances = inireader.ReadInteger("TMNTHax", "AllowMultipleInstances", 0);
    bRelocateWindow = inireader.ReadInteger("TMNTHax", "RelocateWindow", 0);

    if (bRelocateWindow)
    {
        RelocateX = inireader.ReadInteger("WindowLocation", "X", 0);
        RelocateY = inireader.ReadInteger("WindowLocation", "Y", 0);
    }

    return 0;
}

int __stdcall cave_sprintf(char* buf, const char* Format, ...)
{
    va_list ArgList;
    int Result = 0;

    __crt_va_start(ArgList, Format);
    Result = vsprintf(buf, Format, ArgList);
    __crt_va_end(ArgList);

    ReadFullscreenSetting(buf);

    return Result;
}

int __stdcall cave_sprintf2(char* buf, const char* Format, ...)
{
    va_list ArgList;
    int Result = 0;

    __crt_va_start(ArgList, Format);
    Result = vsprintf(buf, Format, ArgList);
    __crt_va_end(ArgList);

    puts(buf);

    return Result;
}

int cave_vsnprintf(char* buf, const size_t bufcount, const char* Format, va_list ArgList)
{
    int Result = 0;

    Result = vsnprintf(buf, bufcount, Format, ArgList);
    vprintf(Format, ArgList);

    return Result;
}

void __stdcall OutputDebugStringHook(LPCSTR lpOutputString)
{
    printf("%s", lpOutputString);
    return OutputDebugStringA(lpOutputString);
}

int GetDesktopRes(int32_t *DesktopResW, int32_t *DesktopResH)
{
    HMONITOR monitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(monitor, &info);
    *DesktopResW = info.rcMonitor.right - info.rcMonitor.left;
    *DesktopResH = info.rcMonitor.bottom - info.rcMonitor.top;
    return 0;
}

BOOL WINAPI AdjustWindowRect_Hook(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
    DWORD newStyle = 0;

    if (!bBorderlessWindowed)
        newStyle = WS_CAPTION;

    return AdjustWindowRect(lpRect, newStyle, bMenu);
}

HWND WINAPI CreateWindowExA_Hook(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    int WindowPosX = RelocateX;
    int WindowPosY = RelocateY;

    GetDesktopRes(&DesktopX, &DesktopY);

    if (!bRelocateWindow)
    {
        // fix the window to open at the center of the screen...
        WindowPosX = (int)(((float)DesktopX / 2.0f) - ((float)nWidth / 2.0f));
        WindowPosY = (int)(((float)DesktopY / 2.0f) - ((float)nHeight / 2.0f));
    }

    GameHWND = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, 0, WindowPosX, WindowPosY, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    LONG lStyle = GetWindowLong(GameHWND, GWL_STYLE);

    if (bBorderlessWindowed)
        lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    else
    {
        lStyle |= (WS_MINIMIZEBOX | WS_SYSMENU);
        if (bEnableWindowResize)
            lStyle |= (WS_MAXIMIZEBOX | WS_THICKFRAME);
    }

    SetWindowLong(GameHWND, GWL_STYLE, lStyle);
    SetWindowPos(GameHWND, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    
    return GameHWND;
}

BOOL WINAPI GetCursorPos_Hook(LPPOINT lpPoint)
{
    BOOL retval = GetCursorPos(lpPoint);
    GetWindowRect(GameHWND, &windowRect);

    // correct the mouse position on moved window... this needs a lot more detail fixing - when window is resized, this breaks
    lpPoint->x = lpPoint->x - windowRect.left;
    lpPoint->y = lpPoint->y - windowRect.top;

    return retval;
}

bool RetTrue()
{
    return true;
}

int Init()
{
    injector::MakeCALL(0x0040B91B, cave_sprintf, true);
    
    injector::WriteMemory<int>(0x007A6278, (int)&CreateWindowExA_Hook, true);
    injector::WriteMemory<int>(0x007A627C, (int)&AdjustWindowRect_Hook, true);
    injector::WriteMemory<int>(0x007A62A0, (int)&GetCursorPos_Hook, true);

    // console window stuff...
    if (bEnableConsole)
    {
        injector::MakeCALL(0x004B65BA, cave_sprintf2, true);
        injector::MakeCALL(0x004B61B4, cave_sprintf2, true);
        injector::MakeCALL(0x004B61F4, cave_sprintf2, true);
        injector::MakeCALL(0x004B654F, cave_sprintf2, true);
        injector::MakeCALL(0x004181FD, cave_vsnprintf, true);
        injector::WriteMemory<int>(0x007A61E0, (int)&OutputDebugStringHook, true);
        //injector::MakeCALL(0x0041819C, cave_vsnprintf, true);
    }

    // kill Launcher mutex creation so the game can be started directly...
    injector::MakeJMP(0x0041507E, 0x41510B, true);

    // kill instance mutex -- doesn't work for some reason... broken by Launcher mutex kill
    if (bAllowMultipleInstances)
        injector::MakeJMP(0x00414FAF, 0x41510B, true);

    // Disable CD check...
    injector::MakeNOP(0x0040607C, 2, true);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        InitConfig();

        if (bEnableConsole)
        {
            AttachConsole(ATTACH_PARENT_PROCESS);
            AllocConsole();
            freopen("CON", "w", stdout);
            freopen("CON", "w", stderr);
            freopen("CON", "r", stdin);
        }

        Init();
    }
    return TRUE;
}


