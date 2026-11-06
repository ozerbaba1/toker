// C++17, x86 — Toker CS 1.6 internal: shared config + disk persistence
#pragma once
#include <windows.h>
#include <cstdio>
#include <cstring>

namespace C {
    bool menu = true;
    bool watermark = true;
    int  menuKey = VK_INSERT;

    // interface
    bool glass = true;          // real backdrop blur behind the menu
    bool reduceMotion = false;  // shorter, calmer animations
    float glassOpacity = .82f;  // dark glass coat strength
    int glassQuality = 1;       // 0 = performance, 1 = balanced, 2 = smooth

    // esp
    bool esp = true; int espStyle = 1;                 // 0 = box, 1 = corner
    bool espBox = true;
    bool espName = true, espDist = true, espSnap = false;
    bool espTrail = false, espGlow = true, espIgnoreTeam = false;
    float espAlpha = 1.f, espMaxDist = 0.f;            // alpha 0..1, max meters (0 = unlimited)
    float ecT[3] = { 1.f, .4f, 0.f }, ecCT[3] = { 0.f, .47f, 1.f };

    // radar
    bool radar = false, radarEnemies = true;
    float radarRange = 1500.f, radarSize = 140.f;

    // chams: 0 = off, 1 = flat, 2 = wire, 3 = glow, 4 = rainbow
    int chMode = 1; float chamsAlpha = 1.f;
    float chTV[3] = { 1.f, .55f, 0.f }, chTW[3] = { 1.f, .2f, 0.f };
    float chCV[3] = { 0.f, .55f, 1.f }, chCW[3] = { 0.f, .2f, 1.f };

    // viewmodel
    bool hand = false; int handM = 1; float handC[3] = { 0.f, .9f, .4f };
    float handAlpha = 1.f, handLineWidth = 1.5f;
    float vmOfs[3] = { 0.f, 0.f, 0.f };

    // crosshair
    bool xhair = false; float xhSize = 6.f, xhGap = 2.f, xhThick = 1.5f;
    float xhCol[4] = { 0.f, 1.f, .4f, 1.f };
    bool xhDot = false, xhOutline = true;
    bool scopeOvr = false;

    // world
    bool fogOn = false; float fogC[3] = { .05f, .05f, .12f };
    int fogMode = 0; float fogDensity = .025f;
    float fogS = 200.f, fogE = 2500.f;
    bool nightMode = false; float nightAlpha = .4f;
    bool noFlash = false;

    // aim
    bool aim = false, silent = false;
    float aimFov = 10.f, aimSmooth = 5.f;
    int aimBone = 0, aimKey = 0x05;
    bool aimVis = true;

    // trigger
    bool trig = false; float trigFov = 2.5f; int trigDelay = 50, trigPart = 0;

    // recoil
    bool rcs = false; float rcsF = 2.f;

    // misc
    bool bhop = true, autoStrafe = false, dmgFlash = true, autoPistol = false;

    // effects
    bool btrace = false; float btCol[3] = { 1.f, .8f, 0.f };
    float trailCol[3] = { 1.f, .5f, .2f };
}

namespace cfg {
    enum Type { T_BOOL = 0, T_INT = 1, T_F3 = 2, T_F4 = 3 };

    struct Entry { const char* key; int type; void* ptr; };

    inline void dir(char* out, int cap) {
        HMODULE mod = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&cfg::dir), &mod);
        GetModuleFileNameA(mod, out, cap);
        char* slash = strrchr(out, '\\');
        if (slash) *slash = 0;
    }

    inline Entry* table(int& count) {
        static Entry e[] = {
            { "watermark",   T_BOOL, &C::watermark },
            { "menuKey",     T_INT,  &C::menuKey },
            { "glass",       T_BOOL, &C::glass },
            { "reduceMotion",T_BOOL, &C::reduceMotion },
            { "glassOpacity",T_F3,   &C::glassOpacity },
            { "glassQuality",T_INT,  &C::glassQuality },
            { "esp",         T_BOOL, &C::esp },
            { "espStyle",    T_INT,  &C::espStyle },
            { "espBox",      T_BOOL, &C::espBox },
            { "espName",     T_BOOL, &C::espName },
            { "espDist",     T_BOOL, &C::espDist },
            { "espSnap",     T_BOOL, &C::espSnap },
            { "espTrail",    T_BOOL, &C::espTrail },
            { "espGlow",     T_BOOL, &C::espGlow },
            { "espIgnoreTeam",T_BOOL,&C::espIgnoreTeam },
            { "espAlpha",    T_F3,   &C::espAlpha },
            { "espMaxDist",  T_F3,   &C::espMaxDist },
            { "ecT",         T_F3,   C::ecT },
            { "ecCT",        T_F3,   C::ecCT },
            { "radar",       T_BOOL, &C::radar },
            { "radarEnemies",T_BOOL, &C::radarEnemies },
            { "radarRange",  T_F3,   &C::radarRange },
            { "radarSize",   T_F3,   &C::radarSize },
            { "chMode",      T_INT,  &C::chMode },
            { "chamsAlpha",  T_F3,   &C::chamsAlpha },
            { "chTV",        T_F3,   C::chTV },
            { "chTW",        T_F3,   C::chTW },
            { "chCV",        T_F3,   C::chCV },
            { "chCW",        T_F3,   C::chCW },
            { "hand",        T_BOOL, &C::hand },
            { "handM",       T_INT,  &C::handM },
            { "handC",       T_F3,   C::handC },
            { "handAlpha",   T_F3,   &C::handAlpha },
            { "handLineWidth",T_F3,  &C::handLineWidth },
            { "vmOfs",       T_F3,   C::vmOfs },
            { "xhair",       T_BOOL, &C::xhair },
            { "xhSize",      T_F3,   &C::xhSize },   // scalar stored as f3[0]
            { "xhGap",       T_F3,   &C::xhGap },
            { "xhThick",     T_F3,   &C::xhThick },
            { "xhCol",       T_F4,   C::xhCol },
            { "xhDot",       T_BOOL, &C::xhDot },
            { "xhOutline",   T_BOOL, &C::xhOutline },
            { "scopeOvr",    T_BOOL, &C::scopeOvr },
            { "fogOn",       T_BOOL, &C::fogOn },
            { "fogC",        T_F3,   C::fogC },
            { "fogMode",     T_INT,  &C::fogMode },
            { "fogDensity",  T_F3,   &C::fogDensity },
            { "fogS",        T_F3,   &C::fogS },
            { "fogE",        T_F3,   &C::fogE },
            { "nightMode",   T_BOOL, &C::nightMode },
            { "nightAlpha",  T_F3,   &C::nightAlpha },
            { "noFlash",     T_BOOL, &C::noFlash },
            { "aim",         T_BOOL, &C::aim },
            { "silent",      T_BOOL, &C::silent },
            { "aimFov",      T_F3,   &C::aimFov },
            { "aimSmooth",   T_F3,   &C::aimSmooth },
            { "aimBone",     T_INT,  &C::aimBone },
            { "aimKey",      T_INT,  &C::aimKey },
            { "aimVis",      T_BOOL, &C::aimVis },
            { "trig",        T_BOOL, &C::trig },
            { "trigFov",     T_F3,   &C::trigFov },
            { "trigDelay",   T_INT,  &C::trigDelay },
            { "trigPart",    T_INT,  &C::trigPart },
            { "rcs",         T_BOOL, &C::rcs },
            { "rcsF",        T_F3,   &C::rcsF },
            { "bhop",        T_BOOL, &C::bhop },
            { "autoStrafe",  T_BOOL, &C::autoStrafe },
            { "dmgFlash",    T_BOOL, &C::dmgFlash },
            { "autoPistol",  T_BOOL, &C::autoPistol },
            { "btrace",      T_BOOL, &C::btrace },
            { "btCol",       T_F3,   C::btCol },
            { "trailCol",    T_F3,   C::trailCol },
        };
        count = (int)(sizeof(e) / sizeof(e[0]));
        return e;
    }

    // F3 entries whose pointer is a lone float (scalar-in-vec hack) need special handling.
    inline bool scalar_f3(const char* key) {
        return !strcmp(key, "xhSize") || !strcmp(key, "xhGap") ||
               !strcmp(key, "glassOpacity") || !strcmp(key, "handAlpha") ||
               !strcmp(key, "handLineWidth") || !strcmp(key, "fogDensity") ||
               !strcmp(key, "xhThick") || !strcmp(key, "fogS") ||
               !strcmp(key, "fogE") || !strcmp(key, "nightAlpha") ||
               !strcmp(key, "aimFov") || !strcmp(key, "aimSmooth") ||
               !strcmp(key, "trigFov") || !strcmp(key, "rcsF") ||
               !strcmp(key, "espAlpha") || !strcmp(key, "espMaxDist") ||
               !strcmp(key, "chamsAlpha") || !strcmp(key, "radarRange") ||
               !strcmp(key, "radarSize");
    }

    inline const char* path() {
        static char p[MAX_PATH] = { 0 };
        if (p[0]) return p;
        char dir[MAX_PATH] = { 0 };
        cfg::dir(dir, MAX_PATH);
        snprintf(p, MAX_PATH, "%s\\toker_cs16.cfg", dir);
        return p;
    }

    inline bool save() {
        FILE* f = nullptr;
        if (fopen_s(&f, path(), "w") != 0 || !f) return false;
        int n = 0; Entry* e = table(n);
        for (int i = 0; i < n; i++) {
            switch (e[i].type) {
            case T_BOOL: fprintf(f, "b %s %d\n", e[i].key, *(bool*)e[i].ptr ? 1 : 0); break;
            case T_INT:  fprintf(f, "i %s %d\n", e[i].key, *(int*)e[i].ptr); break;
            case T_F3: {
                float* v = (float*)e[i].ptr;
                if (scalar_f3(e[i].key)) fprintf(f, "f %s %.6f\n", e[i].key, *v);
                else fprintf(f, "f3 %s %.6f %.6f %.6f\n", e[i].key, v[0], v[1], v[2]);
                break;
            }
            case T_F4: {
                float* v = (float*)e[i].ptr;
                fprintf(f, "f4 %s %.6f %.6f %.6f %.6f\n", e[i].key, v[0], v[1], v[2], v[3]);
                break;
            }
            }
        }
        fclose(f);
        return true;
    }

    inline bool load() {
        FILE* f = nullptr;
        if (fopen_s(&f, path(), "r") != 0 || !f) return false;
        int n = 0; Entry* e = table(n);
        char type[8] = { 0 }, key[64] = { 0 };
        while (fscanf(f, "%7s %63s", type, key) == 2) {
            Entry* hit = nullptr;
            for (int i = 0; i < n; i++) if (!strcmp(e[i].key, key)) { hit = &e[i]; break; }
            if (!hit) { char junk[256]; fgets(junk, sizeof(junk), f); continue; }
            if (!strcmp(type, "b")) { int v = 0; fscanf(f, "%d", &v); *(bool*)hit->ptr = v != 0; }
            else if (!strcmp(type, "i")) { fscanf(f, "%d", (int*)hit->ptr); }
            else if (!strcmp(type, "f")) { float* v = (float*)hit->ptr; fscanf(f, "%f", v); }
            else if (!strcmp(type, "f3")) {
                float* v = (float*)hit->ptr;
                if (scalar_f3(hit->key)) fscanf(f, "%f", v);
                else fscanf(f, "%f %f %f", &v[0], &v[1], &v[2]);
            }
            else if (!strcmp(type, "f4")) {
                float* v = (float*)hit->ptr;
                fscanf(f, "%f %f %f %f", &v[0], &v[1], &v[2], &v[3]);
            }
        }
        fclose(f);
        if (C::menuKey <= 0 || C::menuKey > 255) C::menuKey = VK_INSERT;
        if (C::glassQuality < 0 || C::glassQuality > 2) C::glassQuality = 1;
        if (C::glassOpacity < .45f) C::glassOpacity = .45f;
        if (C::glassOpacity > 1.f) C::glassOpacity = 1.f;
        if (C::fogMode < 0 || C::fogMode > 2) C::fogMode = 0;
        if (C::fogDensity < .001f) C::fogDensity = .001f;
        if (C::fogDensity > .15f) C::fogDensity = .15f;
        if (C::handAlpha < .05f) C::handAlpha = .05f;
        if (C::handAlpha > 1.f) C::handAlpha = 1.f;
        if (C::handLineWidth < .5f) C::handLineWidth = .5f;
        if (C::handLineWidth > 5.f) C::handLineWidth = 5.f;
        return true;
    }

    inline void reset() {
        C::watermark = true; C::menuKey = VK_INSERT;
        C::glass = true; C::reduceMotion = false;
        C::glassOpacity = .82f; C::glassQuality = 1;
        C::esp = true; C::espStyle = 1;
        C::espBox = true;
        C::espName = true; C::espDist = true; C::espSnap = false;
        C::espTrail = false; C::espGlow = true; C::espIgnoreTeam = false;
        C::espAlpha = 1.f; C::espMaxDist = 0.f;
        C::ecT[0] = 1.f; C::ecT[1] = .4f; C::ecT[2] = 0.f;
        C::ecCT[0] = 0.f; C::ecCT[1] = .47f; C::ecCT[2] = 1.f;
        C::radar = false; C::radarEnemies = true;
        C::radarRange = 1500.f; C::radarSize = 140.f;
        C::chMode = 1; C::chamsAlpha = 1.f;
        C::chTV[0] = 1.f; C::chTV[1] = .55f; C::chTV[2] = 0.f;
        C::chTW[0] = 1.f; C::chTW[1] = .2f; C::chTW[2] = 0.f;
        C::chCV[0] = 0.f; C::chCV[1] = .55f; C::chCV[2] = 1.f;
        C::chCW[0] = 0.f; C::chCW[1] = .2f; C::chCW[2] = 1.f;
        C::hand = false; C::handM = 1; C::handC[0] = 0.f; C::handC[1] = .9f; C::handC[2] = .4f;
        C::handAlpha = 1.f; C::handLineWidth = 1.5f;
        C::vmOfs[0] = C::vmOfs[1] = C::vmOfs[2] = 0.f;
        C::xhair = false; C::xhSize = 6.f; C::xhGap = 2.f; C::xhThick = 1.5f;
        C::xhCol[0] = 0.f; C::xhCol[1] = 1.f; C::xhCol[2] = .4f; C::xhCol[3] = 1.f;
        C::xhDot = false; C::xhOutline = true;
        C::scopeOvr = false;
        C::fogOn = false; C::fogC[0] = .05f; C::fogC[1] = .05f; C::fogC[2] = .12f;
        C::fogMode = 0; C::fogDensity = .025f;
        C::fogS = 200.f; C::fogE = 2500.f;
        C::nightMode = false; C::nightAlpha = .4f; C::noFlash = false;
        C::aim = false; C::silent = false; C::aimFov = 10.f; C::aimSmooth = 5.f;
        C::aimBone = 0; C::aimKey = 0x05; C::aimVis = true;
        C::trig = false; C::trigFov = 2.5f; C::trigDelay = 50; C::trigPart = 0;
        C::rcs = false; C::rcsF = 2.f;
        C::bhop = true; C::autoStrafe = false; C::dmgFlash = true; C::autoPistol = false;
        C::btrace = false; C::btCol[0] = 1.f; C::btCol[1] = .8f; C::btCol[2] = 0.f;
        C::trailCol[0] = 1.f; C::trailCol[1] = .5f; C::trailCol[2] = .2f;
    }
}
