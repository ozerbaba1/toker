// GoldSrc CS 1.6 SDK — CSDuragi Protocol 48 non-steam
// Structs derived from ValveSoftware/halflife SDK source
#pragma once
#include <cstring>
#include <cmath>

typedef float vec3_t[3];
struct color24 { unsigned char r, g, b; };

#define FL_ONGROUND     (1 << 9)
#define IN_JUMP         (1 << 1)
#define IN_ATTACK       (1 << 0)
#define IN_DUCK         (1 << 2)
#define MOVETYPE_NONE   0
#define TEAM_TERRORIST  1
#define TEAM_CT         2

#define DEG2RAD 0.01745329251994f
#define RAD2DEG 57.29577951308232f

// ============================================================================
// entity_state_t — 340 bytes
// ============================================================================
struct entity_state_t {
    int entityType; int number; float msg_time; int messagenum;
    vec3_t origin; vec3_t angles;
    int modelindex; int sequence; float frame; int colormap;
    short skin; short solid; int effects; float scale;
    unsigned char eflags; unsigned char _p0[3];
    int rendermode; int renderamt; color24 rendercolor; unsigned char _p1;
    int renderfx; int movetype; float animtime; float framerate;
    int body; unsigned char controller[4]; unsigned char blending[4];
    vec3_t velocity; vec3_t mins; vec3_t maxs;
    int aiment; int owner; float friction; float gravity;
    int team; int playerclass; int health; int spectator;
    int weaponmodel; int gaitsequence; vec3_t basevelocity;
    int usehull; int oldbuttons; int onground; int iStepLeft;
    float flFallVelocity; float fov; int weaponanim;
    vec3_t startpos; vec3_t endpos; float impacttime; float starttime;
    int iuser1; int iuser2; int iuser3; int iuser4;
    float fuser1; float fuser2; float fuser3; float fuser4;
    vec3_t vuser1; vec3_t vuser2; vec3_t vuser3; vec3_t vuser4;
};

// ============================================================================
// cl_entity_t — partial, enough for our use
// ============================================================================
struct cl_entity_t {
    int index; int player;
    entity_state_t baseline; entity_state_t prevstate; entity_state_t curstate;
};

// ============================================================================
// playermove_t — from pm_defs.h, x86 layout
// Offset 0..
//   0: player_index (int)
//   4: server (qboolean = int)
//   8: multiplayer (qboolean = int)
//  12: time (float)
//  16: frametime (float)
//  20: forward (vec3_t = 12)
//  32: right (vec3_t)
//  44: up (vec3_t)
//  56: origin (vec3_t)
//  68: angles (vec3_t)
//  80: oldangles (vec3_t)
//  92: velocity (vec3_t)
// 104: movedir (vec3_t)
// 116: basevelocity (vec3_t)
// 128: view_ofs (vec3_t)
// 140: duck_time (float)
// 144: in_duck (qboolean)
// 148: time_step_sound (int)
// 152: step_left (int)
// 156: fall_velocity (float)
// 160: punchangle (vec3_t)  <<< THIS
// 172: swimtime (float)
// 176: next_primary_attack (float)
// 180: effects (int)
// 184: flags (int)
// 188: usehull (int)
// 192: gravity (float)
// 196: friction (float)
// 200: oldbuttons (int)
// 204: waterjumptime (float)
// 208: dead (qboolean)
// ...
// ============================================================================
struct playermove_t {
    int player_index;       // 0
    int server;             // 4
    int multiplayer;        // 8
    float time;             // 12
    float frametime;        // 16
    vec3_t forward;         // 20
    vec3_t right;           // 32
    vec3_t up;              // 44
    vec3_t origin;          // 56
    vec3_t angles;          // 68
    vec3_t oldangles;       // 80
    vec3_t velocity;        // 92
    vec3_t movedir;         // 104
    vec3_t basevelocity;    // 116
    vec3_t view_ofs;        // 128
    float duck_time;        // 140
    int in_duck;            // 144
    int time_step_sound;    // 148
    int step_left;          // 152
    float fall_velocity;    // 156
    vec3_t punchangle;      // 160
    float swimtime;         // 172
    float next_primary_attack; // 176
    int effects;            // 180
    int flags;              // 184
    int usehull;            // 188
    float gravity;          // 192
    float friction;         // 196
    int oldbuttons;         // 200
    float waterjumptime;    // 204
    int dead;               // 208
    int movetype;           // 212
    int onground;           // 216
    int waterlevel;         // 220
    int watertype;          // 224
    int oldwaterlevel;      // 228
    char sztexturename[256];// 232
    char chtexturetype;     // 488
    float maxspeed;         // 492 (aligned to 4)
    float clientmaxspeed;   // 496
    // ... more fields we don't need
};

// ============================================================================
// usercmd_t
// ============================================================================
struct usercmd_t {
    short lerp_msec; unsigned char msec;
    vec3_t viewangles;
    float forwardmove; float sidemove; float upmove;
    unsigned char lightlevel;
    unsigned short buttons;
    unsigned char impulse; unsigned char weaponselect;
    int impact_index; vec3_t impact_position;
};

// ============================================================================
// ref_params_t — from ref_params.h
// Used by V_CalcRefdef
// ============================================================================
struct ref_params_t {
    // output
    vec3_t vieworg;         // 0
    vec3_t viewangles;      // 12
    vec3_t forward;         // 24
    vec3_t right;           // 36
    vec3_t up;              // 48
    // client parameters
    float frametime;        // 60
    float time;             // 64
    int intermission;       // 68
    int paused;             // 72
    int spectator;          // 76
    int onground;           // 80
    int waterlevel;         // 84
    vec3_t simvel;          // 88
    vec3_t simorg;          // 100
    vec3_t viewheight;      // 112
    float idealpitch;       // 124
    vec3_t cl_viewangles;   // 128
    int health;             // 140
    vec3_t crosshairangle;  // 144
    float viewsize;         // 156
    vec3_t punchangle;      // 160  <<< ALSO HERE
    int maxclients;         // 172
    int viewentity;         // 176
    int playernum;          // 180
    int max_entities;       // 184
    int demoplayback;       // 188
    int hardware;           // 192
    int smoothing;          // 196
    // ptr to usercmd_t
    void* cmd;              // 200
    // ptr to movevars_t
    void* movevars;         // 204
    int viewport[4];        // 208
    int nextView;           // 224
    int onlyClientDraw;     // 228
};

// ============================================================================
// hud_player_info_t
// ============================================================================
struct hud_player_info_t {
    char* name; short ping;
    unsigned char thisplayer; unsigned char spectator; unsigned char packetloss;
    char* model; short topcolor; short bottomcolor;
};

// ============================================================================
// r_studio_interface_t
// ============================================================================
struct r_studio_interface_t {
    int version;
    int (*StudioDrawModel)(int flags);
    int (*StudioDrawPlayer)(int flags, entity_state_t* pplayer);
};

// ============================================================================
// pmtrace_t
// ============================================================================
struct pmtrace_t {
    int allsolid;       // 0
    int startsolid;     // 4
    int inopen;         // 8
    int inwater;        // 12
    float fraction;     // 16: 0.0 = start in solid, 1.0 = no hit
    vec3_t endpos;      // 20
    // plane_t plane
    vec3_t plane_normal;// 32
    float plane_dist;   // 44
    // rest
    int hitgroup;       // 48 — HITGROUP for body part detection
    int ent;            // 52 — physent index
    vec3_t deltavelocity;// 56
};

// ============================================================================
// screenfade_t
// ============================================================================
struct screenfade_t {
    float fadeSpeed;    // duration
    float fadeEnd;      // holdtime
    float fadeTotalEnd;
    float fadeReset;
    unsigned char fader, fadeg, fadeb, fadealpha;
    int fadeFlags;
};

// ============================================================================
// triangleapi_s — pTriAPI
// ============================================================================
struct triangleapi_s {
    int version;
    void (*RenderMode)(int mode);
    void (*Begin)(int primitiveCode);
    void (*End)(void);
    void (*Color4f)(float r, float g, float b, float a);
    void (*Color4ub)(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
    void (*TexCoord2f)(float u, float v);
    void (*Vertex3fv)(float* worldPnt);
    void (*Vertex3f)(float x, float y, float z);
    void (*Brightness)(float brightness);
    void (*CullFace)(int mode);
    int  (*SpriteTexture)(void* pSpriteModel, int frame);
    int  (*WorldToScreen)(float* world, float* screen); // returns 1 if behind
    void (*Fog)(float flFogColor[3], float flStart, float flEnd, int bOn);
    void (*ScreenToWorld)(float* screen, float* world);
    void (*GetMatrix)(const int pname, float* matrix);
    int  (*BoxInPVS)(float* mins, float* maxs);
    void (*LightAtPoint)(float* pos, float* value);
    void (*Color4fRendermode)(float r, float g, float b, float a, int rendermode);
    void (*FogParams)(float flDensity, int iFogSkybox);
};

// hitgroup constants
#define HITGROUP_GENERIC   0
#define HITGROUP_HEAD      1
#define HITGROUP_CHEST     2
#define HITGROUP_STOMACH   3
#define HITGROUP_LEFTARM   4
#define HITGROUP_RIGHTARM  5
#define HITGROUP_LEFTLEG   6
#define HITGROUP_RIGHTLEG  7

// cl_enginefunc_t indices (from cdll_int.h, counted from 0)
// These are function pointer indices in the engine function table
// The table is an array of void* (function pointers), filled by engine
// Indices verified against HL SDK cdll_int.h:
//  0: pfnSPR_Load
//  1: pfnSPR_Frames
//  2: pfnSPR_Height
//  3: pfnSPR_Width
//  4: pfnSPR_Set
//  5: pfnSPR_Draw
//  6: pfnSPR_DrawHoles
//  7: pfnSPR_DrawAdditive
//  8: pfnSPR_EnableScissor
//  9: pfnSPR_DisableScissor
// 10: pfnSPR_GetList
// 11: pfnFillRGBA
// 12: pfnGetScreenInfo
// 13: pfnSetCrosshair
// 14: pfnRegisterVariable
// 15: pfnGetCvarFloat
// 16: pfnGetCvarString
// 17: pfnAddCommand
// 18: pfnHookUserMsg
// 19: pfnServerCmd
// 20: pfnClientCmd
// 21: pfnGetPlayerInfo
// 22: pfnPlaySoundByName
// 23: pfnPlaySoundByIndex
// 24: pfnAngleVectors
// 25: pfnTextMessageGet
// 26: pfnDrawCharacter
// 27: pfnDrawConsoleString
// 28: pfnDrawSetTextColor
// 29: pfnDrawConsoleStringLen
// 30: pfnConsolePrint
// 31: pfnCenterPrint
// 32: GetWindowCenterX
// 33: GetWindowCenterY
// 34: GetViewAngles
// 35: SetViewAngles
// 36: GetMaxClients
// 37: Cvar_SetValue
// 38: Cmd_Argc
// 39: Cmd_Argv
// 40: Con_Printf
// 41: Con_DPrintf
// 42: Con_NPrintf
// 43: Con_NXPrintf
// 44: PhysInfo_ValueForKey
// 45: ServerInfo_ValueForKey
// 46: GetClientMaxspeed
// 47: CheckParm
// 48: Key_Event
// 49: GetMousePosition
// 50: IsNoClipping
// 51: GetLocalPlayer
// 52: GetViewModel
// 53: GetEntityByIndex
// 54: GetClientTime
// 55: V_CalcShake
// 56: V_ApplyShake
// 57: PM_PointContents
// 58: PM_WaterEntity
// 59: PM_TraceLine
// 60: CL_LoadModel
// 61: CL_CreateVisibleEntity
// 62: GetSpritePointer
// 63: pfnPlaySoundByNameAtLocation
// 64: pfnPrecacheEvent
// 65: pfnPlaybackEvent
// 66: pfnWeaponAnim
// 67: pfnRandomFloat
// 68: pfnRandomLong
// 69: pfnHookEvent
// 70: Con_IsVisible
// 71: pfnGetGameDirectory
// 72: pfnGetCvarPointer
// 73: Key_LookupBinding
// 74: pfnGetLevelName
// 75: pfnGetScreenFade
// 76: pfnSetScreenFade
// 77: VGui_GetPanel
// 78: VGui_ViewportPaintBackground
// 79: COM_FileBase
// 80: COM_ParseFile
// 81: COM_FreeFile
// 82-86: more triangle/efx stuff
// pTriAPI is at a FIXED offset — it's NOT in the func table directly
// In the engine, pTriAPI is typically stored at enginefuncs + some offset
// But in cdll_int.h, it's a separate struct member of cl_clientfunc_t
// For GoldSrc, we access pTriAPI through the engine module export

// For accessing pTriAPI, we scan hw.dll for the global pointer
// or use the enginefuncs approach where it's index ~82-84 area
// Actually: in the cl_enginefunc_t struct, after the function pointers,
// there are struct pointers: pTriAPI, pEfxAPI, pEventAPI, pDemoAPI, pNetAPI
// These are at the END of the function table.
// In HL SDK, cl_enginefunc_t has exactly 100 function pointers,
// then 5 struct pointers: pTriAPI at index 100, pEfxAPI at 101, etc.

#define ENG_SPR_LOAD         0
#define ENG_SPR_SET          4
#define ENG_SPR_DRAW         5
#define ENG_SPR_DRAWHOLES    6
#define ENG_SPR_DRAWADDITIVE 7
#define ENG_FILLRGBA        11
#define ENG_GETSCREENINFO   12
#define ENG_GETPLAYERINFO   21
#define ENG_DRAWCHARACTER   26
#define ENG_DRAWCONSOLESTR  27
#define ENG_DRAWSETTEXTCOL  28
#define ENG_GETVIEWANGLES   34
#define ENG_SETVIEWANGLES   35
#define ENG_GETMAXCLIENTS   36
#define ENG_CVAR_SETVALUE   37
#define ENG_CON_PRINTF      40
#define ENG_GETLOCALPLAYER  51
#define ENG_GETVIEWMODEL    52
#define ENG_GETENTBYINDEX   53
#define ENG_PM_TRACELINE    59
#define ENG_GETCVARPOINTER  72
#define ENG_GETSCREENFADE   75
#define ENG_SETSCREENFADE   76
// struct pointers at the end — after ~100 function pointers
// HL SDK cdll_int.h has exactly 100 entries before pTriAPI
// But some builds have 99 or 101 — we'll scan for it
#define ENG_TRI_API        100
#define ENG_EFX_API        101
#define ENG_EVENT_API      102
