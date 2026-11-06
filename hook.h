// C++, Windows 11 x86, MinGW/MSVC, GoldSrc CS 1.6
// File: src/hook.h
#pragma once
#include <windows.h>
#include <GL/gl.h>

namespace Core {
    void Initialize(HMODULE hMod);
    void Shutdown();
    
    extern bool g_MenuOpen;
    extern bool g_ESP;
    extern bool g_Chams;
    extern bool g_Bhop;
    extern bool g_NoRecoil;
}

