// C++17, x86, Windows â€” Toker UI layer: frameless dark-glass menu, custom widgets.
// ImGui 1.93 WIP. Fonts are loaded from C:\Windows\Fonts with Turkish glyph ranges.
#pragma once
#include <windows.h>
#include <GL/gl.h>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include <unordered_map>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "config.h"
#include "../imgui/imgui.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace ui {

// ---------------------------------------------------------------------------
// palette — monochrome graphite / frosted glass
// ---------------------------------------------------------------------------
constexpr ImU32 ACCENT_A = IM_COL32(232, 236, 242, 255);
constexpr ImU32 ACCENT_B = IM_COL32(184, 190, 200, 255);
constexpr ImU32 TEXT     = IM_COL32(236, 239, 244, 255);
constexpr ImU32 DIM      = IM_COL32(152, 158, 169, 255);
constexpr ImU32 DIM2     = IM_COL32(98, 104, 115, 255);
constexpr ImU32 WHITE    = IM_COL32(255, 255, 255, 255);
constexpr ImU32 BG       = IM_COL32(10, 11, 14, 246);
constexpr ImU32 RAIL_BG  = IM_COL32(14, 15, 19, 255);
constexpr ImU32 CARD_BG  = IM_COL32(26, 28, 33, 126);
constexpr ImU32 CARD_HOV = IM_COL32(34, 37, 43, 150);
constexpr ImU32 STROKE   = IM_COL32(255, 255, 255, 22);
constexpr ImU32 TRACK    = IM_COL32(255, 255, 255, 24);
constexpr ImU32 SHELL    = IM_COL32(9, 10, 13, 130);

inline float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

inline ImU32 mix(ImU32 a, ImU32 b, float t) {
    t = clamp01(t);
    int ar = (int)((a >> IM_COL32_R_SHIFT) & 0xFF), ag = (int)((a >> IM_COL32_G_SHIFT) & 0xFF);
    int ab = (int)((a >> IM_COL32_B_SHIFT) & 0xFF), aa = (int)((a >> IM_COL32_A_SHIFT) & 0xFF);
    int br = (int)((b >> IM_COL32_R_SHIFT) & 0xFF), bg = (int)((b >> IM_COL32_G_SHIFT) & 0xFF);
    int bb = (int)((b >> IM_COL32_B_SHIFT) & 0xFF), ba = (int)((b >> IM_COL32_A_SHIFT) & 0xFF);
    return IM_COL32((int)(ar + (br - ar) * t), (int)(ag + (bg - ag) * t),
                    (int)(ab + (bb - ab) * t), (int)(aa + (ba - aa) * t));
}

inline ImU32 with_a(ImU32 c, float a) {
    return (c & 0x00FFFFFFu) | ((ImU32)(clamp01(a) * 255.f) << IM_COL32_A_SHIFT);
}

inline ImU32 scale_a(ImU32 c, float m) {
    float a = ((c >> IM_COL32_A_SHIFT) & 0xFF) / 255.f;
    return with_a(c, a * m);
}

inline ImVec4 v4(int r, int g, int b, float a = 1.f) {
    return ImVec4(r / 255.f, g / 255.f, b / 255.f, a);
}

// ---------------------------------------------------------------------------
// frosted glass — grabs the frame behind the menu, box-blurs + desaturates it
// on the CPU, uploads a tiny texture and lets ImGui stretch it back up.
// ---------------------------------------------------------------------------
inline unsigned g_backdrop_tex = 0;
inline float g_last_rect[4] = { 0.f, 0.f, 0.f, 0.f };
inline bool g_glass_live = false;

inline int g_backdrop_tex_w = 0;
inline int g_backdrop_tex_h = 0;
inline DWORD g_backdrop_last_capture = 0;

inline void capture_backdrop() {
    // A glass snapshot is intentionally refreshed at a bounded cadence.
    // Readback is synchronous on legacy OpenGL, so doing it every frame can
    // stall the game thread even when the texture itself is tiny.
    if (!C::glass) { g_glass_live = false; return; }

    const int quality = (C::glassQuality < 0 || C::glassQuality > 2) ? 1 : C::glassQuality;
    const DWORD interval = quality == 0 ? 110u : (quality == 2 ? 34u : 66u);
    const DWORD now = GetTickCount();
    if (g_backdrop_last_capture && (DWORD)(now - g_backdrop_last_capture) < interval)
        return;
    g_backdrop_last_capture = now;
    g_glass_live = false;

    if (g_last_rect[2] < 64.f || g_last_rect[3] < 64.f) return;
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    const int sw = (int)ds.x, sh = (int)ds.y;
    if (sw <= 0 || sh <= 0) return;

    int x = (int)g_last_rect[0], y = (int)g_last_rect[1];
    int w = (int)g_last_rect[2], h = (int)g_last_rect[3];
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= sw || y >= sh) return;
    if (x + w > sw) w = sw - x;
    if (y + h > sh) h = sh - y;
    if (w < 64 || h < 64) return;

    const int gy = sh - y - h;
    if (gy < 0 || gy + h > sh) return;

    static std::vector<unsigned char> raw, small, softened;
    const int step = quality == 0 ? 7 : (quality == 2 ? 4 : 5);
    const int dw = w / step, dh = h / step;
    if (dw < 8 || dh < 8) return;

    // Preserve the host game's pixel-store and texture-binding state.
    GLint oldPack = 4, oldUnpack = 4, oldTexture = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &oldPack);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldUnpack);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);

    raw.resize((size_t)w * (size_t)h * 3u);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, gy, w, h, GL_RGB, GL_UNSIGNED_BYTE, raw.data());
    glPixelStorei(GL_PACK_ALIGNMENT, oldPack);

    small.resize((size_t)dw * (size_t)dh * 3u);
    const unsigned int div = (unsigned int)(step * step * 256);
    for (int j = 0; j < dh; ++j) {
        for (int i = 0; i < dw; ++i) {
            unsigned int acc = 0;
            for (int yy = 0; yy < step; ++yy) {
                const unsigned char* row = raw.data() +
                    ((size_t)(j * step + yy) * (size_t)w + (size_t)(i * step)) * 3u;
                for (int xx = 0; xx < step; ++xx) {
                    const unsigned char* px = row + (size_t)xx * 3u;
                    acc += (unsigned)px[0] * 77u + (unsigned)px[1] * 151u +
                           (unsigned)px[2] * 28u;
                }
            }
            int l = (int)(acc / div);
            l = (int)(l * .76f) + 5;
            if (l > 255) l = 255;
            const size_t o = ((size_t)j * (size_t)dw + (size_t)i) * 3u;
            small[o] = (unsigned char)l;
            small[o + 1] = (unsigned char)std::min(255, l + 2);
            small[o + 2] = (unsigned char)std::min(255, l + 4);
        }
    }

    // Small separable-style 3x3 Gaussian pass. It softens block boundaries
    // without requiring a shader or another full-resolution framebuffer.
    softened.resize(small.size());
    for (int j = 0; j < dh; ++j) {
        for (int i = 0; i < dw; ++i) {
            for (int c = 0; c < 3; ++c) {
                unsigned int sum = 0;
                for (int yy = -1; yy <= 1; ++yy) {
                    const int sy = std::max(0, std::min(dh - 1, j + yy));
                    const unsigned int wy = (yy == 0) ? 2u : 1u;
                    for (int xx = -1; xx <= 1; ++xx) {
                        const int sx = std::max(0, std::min(dw - 1, i + xx));
                        const unsigned int wx = (xx == 0) ? 2u : 1u;
                        const size_t at = ((size_t)sy * (size_t)dw + (size_t)sx) * 3u + (size_t)c;
                        sum += (unsigned int)small[at] * wx * wy;
                    }
                }
                const size_t out = ((size_t)j * (size_t)dw + (size_t)i) * 3u + (size_t)c;
                softened[out] = (unsigned char)((sum + 8u) / 16u);
            }
        }
    }

    if (!g_backdrop_tex) glGenTextures(1, &g_backdrop_tex);
    if (!g_backdrop_tex) {
        glBindTexture(GL_TEXTURE_2D, (GLuint)oldTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, oldUnpack);
        return;
    }

    glBindTexture(GL_TEXTURE_2D, g_backdrop_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (g_backdrop_tex_w != dw || g_backdrop_tex_h != dh) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, dw, dh, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, softened.data());
        g_backdrop_tex_w = dw;
        g_backdrop_tex_h = dh;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, dw, dh, GL_RGB,
                        GL_UNSIGNED_BYTE, softened.data());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, (GLuint)oldTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, oldUnpack);
    g_glass_live = true;
}

inline ImTextureRef glass_tex() {
    return ImTextureRef((ImTextureID)(intptr_t)g_backdrop_tex);
}

// the outer window: shadow, blurred scene, darkening coat, sheen, hairlines
inline void glass_shell(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float r) {
    // Layered ambient shadow: broad, quiet falloff rather than a neon halo.
    for (int i = 10; i >= 1; --i) {
        const float spread = 2.5f + i * 3.4f;
        const int alpha = (int)(1.2f + (10 - i) * .48f);
        dl->AddRectFilled(ImVec2(p0.x - spread, p0.y - spread + 3.f),
                          ImVec2(p1.x + spread, p1.y + spread + 7.f),
                          IM_COL32(0, 0, 0, alpha), r + spread);
    }
    if (g_glass_live && g_backdrop_tex) {
        dl->AddImageRounded(glass_tex(), p0, p1, ImVec2(0.f, 1.f), ImVec2(1.f, 0.f),
                            IM_COL32(255, 255, 255, 255), r);
        dl->AddRectFilled(p0, p1, scale_a(SHELL, clamp01(C::glassOpacity)), r);
    } else {
        const int coat = (int)(224.f * clamp01(C::glassOpacity));
        dl->AddRectFilled(p0, p1, IM_COL32(12, 13, 17, coat), r);
    }
    dl->AddRectFilledMultiColor(ImVec2(p0.x + r, p0.y + 1.f), ImVec2(p1.x - r, p0.y + 150.f),
                                IM_COL32(255, 255, 255, 13), IM_COL32(255, 255, 255, 10),
                                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    // Crisp inner hairline and restrained top-edge reflection.
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 38), r, 0, 1.f);
    dl->AddRect(ImVec2(p0.x + 1.f, p0.y + 1.f), ImVec2(p1.x - 1.f, p1.y - 1.f),
                IM_COL32(255, 255, 255, 12), r - 1.f, 0, 1.f);
    dl->AddLine(ImVec2(p0.x + r + 8.f, p0.y + 1.5f),
                ImVec2(p1.x - r - 8.f, p0.y + 1.5f),
                IM_COL32(255, 255, 255, 24), 1.f);
}

// ---------------------------------------------------------------------------
// animation store
// ---------------------------------------------------------------------------
inline std::unordered_map<unsigned int, float>& anims() {
    static std::unordered_map<unsigned int, float> s;
    return s;
}

inline float anim_to(ImGuiID key, float target, float speed) {
    float& v = anims()[(unsigned int)key];
    float dt = ImGui::GetIO().DeltaTime;
    if (dt > 0.1f) dt = 0.1f;
    if (C::reduceMotion) speed *= 3.5f;
    v += (target - v) * std::min(1.f, dt * speed);
    return v;
}

// ---------------------------------------------------------------------------
// fonts
// ---------------------------------------------------------------------------
inline ImFont* f_small = nullptr;
inline ImFont* f_body = nullptr;
inline ImFont* f_bold = nullptr;
inline ImFont* f_title = nullptr;
inline ImFont* f_big = nullptr;
inline ImFont* f_mono = nullptr;

inline ImFont* try_font(const char* file, float px, const ImWchar* ranges) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "C:\\Windows\\Fonts\\%s", file);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return nullptr;
    return ImGui::GetIO().Fonts->AddFontFromFileTTF(path, px, nullptr, ranges);
}

inline void load_fonts() {
    ImGuiIO& io = ImGui::GetIO();
    const ImWchar* tr = nullptr; // 1.92+ dynamic atlas: glyphs bake on demand

    f_body  = try_font("segoeui.ttf", 15.f, tr);
    if (!f_body) f_body = try_font("tahoma.ttf", 14.f, tr);
    f_small = try_font("segoeui.ttf", 12.f, tr);
    f_bold  = try_font("seguisb.ttf", 14.f, tr);
    if (!f_bold) f_bold = try_font("segoeuib.ttf", 14.f, tr);
    f_title = try_font("segoeuib.ttf", 20.f, tr);
    f_big   = try_font("segoeuib.ttf", 24.f, tr);
    f_mono  = try_font("consola.ttf", 12.f, tr);

    if (!f_body) {
        f_body = io.Fonts->AddFontDefault();
        f_small = f_bold = f_title = f_big = f_mono = f_body;
        return;
    }
    if (!f_small) f_small = f_body;
    if (!f_bold)  f_bold = f_body;
    if (!f_title) f_title = f_body;
    if (!f_big)   f_big = f_body;
    if (!f_mono)  f_mono = f_small;
}

// ---------------------------------------------------------------------------
// style
// ---------------------------------------------------------------------------
inline void apply_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    c[ImGuiCol_Text]                 = v4(236, 239, 244);
    c[ImGuiCol_TextDisabled]         = v4(108, 113, 123);
    c[ImGuiCol_WindowBg]             = v4(0, 0, 0, 0.f);
    c[ImGuiCol_ChildBg]              = v4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg]              = v4(16, 17, 21, .98f);
    c[ImGuiCol_Border]               = v4(255, 255, 255, .09f);
    c[ImGuiCol_BorderShadow]         = v4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]              = v4(255, 255, 255, .05f);
    c[ImGuiCol_FrameBgHovered]       = v4(255, 255, 255, .09f);
    c[ImGuiCol_FrameBgActive]        = v4(255, 255, 255, .13f);
    c[ImGuiCol_TitleBg]              = v4(10, 11, 14);
    c[ImGuiCol_TitleBgActive]        = v4(10, 11, 14);
    c[ImGuiCol_MenuBarBg]            = v4(10, 11, 14);
    c[ImGuiCol_ScrollbarBg]          = v4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = v4(255, 255, 255, .17f);
    c[ImGuiCol_ScrollbarGrabHovered] = v4(255, 255, 255, .28f);
    c[ImGuiCol_ScrollbarGrabActive]  = v4(255, 255, 255, .45f);
    c[ImGuiCol_CheckMark]            = v4(238, 241, 246);
    c[ImGuiCol_SliderGrab]           = v4(238, 241, 246);
    c[ImGuiCol_SliderGrabActive]     = v4(255, 255, 255);
    c[ImGuiCol_Button]               = v4(255, 255, 255, .06f);
    c[ImGuiCol_ButtonHovered]        = v4(255, 255, 255, .11f);
    c[ImGuiCol_ButtonActive]         = v4(255, 255, 255, .17f);
    c[ImGuiCol_Header]               = v4(255, 255, 255, .08f);
    c[ImGuiCol_HeaderHovered]        = v4(255, 255, 255, .13f);
    c[ImGuiCol_HeaderActive]         = v4(255, 255, 255, .18f);
    c[ImGuiCol_Separator]            = v4(255, 255, 255, .08f);
    c[ImGuiCol_SeparatorHovered]     = v4(255, 255, 255, .28f);
    c[ImGuiCol_SeparatorActive]      = v4(255, 255, 255, .45f);
    c[ImGuiCol_ResizeGrip]           = v4(255, 255, 255, .05f);
    c[ImGuiCol_ResizeGripHovered]    = v4(255, 255, 255, .25f);
    c[ImGuiCol_ResizeGripActive]     = v4(255, 255, 255, .45f);
    c[ImGuiCol_Tab]                  = v4(255, 255, 255, .04f);
    c[ImGuiCol_TabHovered]           = v4(255, 255, 255, .12f);
    c[ImGuiCol_TabSelected]          = v4(255, 255, 255, .09f);
    c[ImGuiCol_PlotLines]            = v4(238, 241, 246);
    c[ImGuiCol_PlotHistogram]        = v4(238, 241, 246);
    c[ImGuiCol_TextSelectedBg]       = v4(255, 255, 255, .22f);
    c[ImGuiCol_NavHighlight]         = v4(255, 255, 255, .60f);

    s.WindowRounding = 16.f;
    s.ChildRounding = 12.f;
    s.FrameRounding = 8.f;
    s.PopupRounding = 12.f;
    s.ScrollbarRounding = 4.f;
    s.GrabRounding = 8.f;
    s.WindowBorderSize = 0.f;
    s.ChildBorderSize = 0.f;
    s.PopupBorderSize = 1.f;
    s.FramePadding = ImVec2(10, 6);
    s.ItemSpacing = ImVec2(8, 6);
    s.WindowPadding = ImVec2(0, 0);
    s.ScrollbarSize = 5.f;
    s.AntiAliasedLines = true;
    s.AntiAliasedFill = true;
}

// ---------------------------------------------------------------------------
// card
// ---------------------------------------------------------------------------
inline void draw_toggle_impl(ImVec2 p, float w, float h, bool* v, bool enabled, ImGuiID iid);

struct Card {
    ImVec2 p0{};
    float w = 0.f;
    float y = 0.f;
    ImDrawList* dl = nullptr;
    bool enabled = true;
    const char* title = nullptr;

    float x() const { return p0.x + 16.f; }
    float py() const { return y; }
    float cw() const { return w - 32.f; }
    float dim() const { return enabled ? 1.f : .42f; }
};

inline Card card_begin(ImVec2 pos, float w, const char* title, const char* sub, bool* toggle) {
    Card c;
    c.p0 = pos;
    c.w = w;
    c.dl = ImGui::GetWindowDrawList();
    c.title = title;
    c.y = pos.y + (sub ? 56.f : 42.f);
    c.enabled = toggle ? *toggle : true;

    ImGui::PushID(title);
    c.dl->ChannelsSplit(2);
    c.dl->ChannelsSetCurrent(1);

    // accent dot + title
    c.dl->AddCircleFilled(ImVec2(pos.x + 18.f, pos.y + 22.f), 2.5f, with_a(ACCENT_A, c.enabled ? .92f : .28f));
    c.dl->AddCircle(ImVec2(pos.x + 18.f, pos.y + 22.f), 6.f, with_a(ACCENT_A, c.enabled ? .10f : .035f), 20, 1.f);
    c.dl->AddText(f_bold, 14.5f, ImVec2(pos.x + 30.f, pos.y + 14.f), scale_a(TEXT, c.dim()), title);
    if (sub) c.dl->AddText(f_small, 11.f, ImVec2(pos.x + 18.f, pos.y + 33.f), scale_a(DIM2, c.dim()), sub);

    if (toggle) {
        ImGui::SetCursorScreenPos(ImVec2(pos.x + w - 16.f - 40.f, pos.y + 14.f));
        ImGui::PushID("##cardtoggle");
        draw_toggle_impl(ImVec2(pos.x + w - 16.f - 40.f, pos.y + 14.f), 40.f, 20.f,
                         toggle, c.enabled, ImGui::GetID("##tg"));
        ImGui::PopID();
        c.enabled = *toggle;
    }
    return c;
}

inline float card_end(Card& c) {
    float bottom = c.y + 14.f;
    ImVec2 p1(c.p0.x + c.w, bottom);
    bool hov = ImGui::IsMouseHoveringRect(c.p0, p1);

    c.dl->ChannelsSetCurrent(0);
    c.dl->AddRectFilled(c.p0, p1, hov ? CARD_HOV : CARD_BG, 15.f);
    // Frosted card face: soft top sheen, lower glass tint and double hairline.
    c.dl->AddRectFilledMultiColor(ImVec2(c.p0.x + 1.f, c.p0.y + 1.f),
                                  ImVec2(p1.x - 1.f, c.p0.y + 66.f),
                                  IM_COL32(255, 255, 255, 13), IM_COL32(255, 255, 255, 8),
                                  IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    c.dl->AddRectFilledMultiColor(ImVec2(c.p0.x + 1.f, c.p0.y + 66.f),
                                  ImVec2(p1.x - 1.f, p1.y - 1.f),
                                  IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0),
                                  IM_COL32(0, 0, 0, 12), IM_COL32(0, 0, 0, 12));
    c.dl->AddRect(c.p0, p1, hov ? IM_COL32(255, 255, 255, 48) : IM_COL32(255, 255, 255, 25), 15.f, 0, 1.f);
    c.dl->AddRect(ImVec2(c.p0.x + 1.f, c.p0.y + 1.f), ImVec2(p1.x - 1.f, p1.y - 1.f),
                  IM_COL32(255, 255, 255, 7), 14.f, 0, 1.f);
    c.dl->ChannelsMerge();

    ImGui::PopID();
    return bottom;
}

// ---------------------------------------------------------------------------
// widgets
// ---------------------------------------------------------------------------
inline void draw_toggle_impl(ImVec2 p, float w, float h, bool* v, bool enabled, ImGuiID iid) {
    ImGui::SetCursorScreenPos(p);
    ImGui::InvisibleButton("##hit", ImVec2(w, h));
    bool pressed = ImGui::IsItemClicked();
    if (pressed && enabled) *v = !*v;
    bool hov = enabled && ImGui::IsItemHovered();

    float t = anim_to(iid, *v ? 1.f : 0.f, 16.f);
    float hv = anim_to(iid + 1, hov ? 1.f : 0.f, 18.f);
    float fade = enabled ? 1.f : .58f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 on = IM_COL32(228, 232, 238, 255);
    ImU32 bg = mix(IM_COL32(54, 58, 67, 255), on, t);

    dl->AddRectFilled(ImVec2(p.x - 3.f, p.y - 3.f), ImVec2(p.x + w + 3.f, p.y + h + 3.f),
                      with_a(ACCENT_A, (.08f * t + .05f * hv) * fade), h * .5f + 3.f);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), scale_a(bg, fade), h * .5f);
    float r = h * .5f - 3.5f + 1.f;
    float kx = p.x + h * .5f + t * (w - h);
    ImU32 knob = mix(IM_COL32(248, 250, 253, 255), IM_COL32(12, 13, 16, 255), t);
    dl->AddCircleFilled(ImVec2(kx, p.y + h * .5f), r + (hv * .6f), scale_a(knob, fade));
    if (t > .02f) dl->AddCircle(ImVec2(kx, p.y + h * .5f), r + 1.5f, with_a(WHITE, .28f * t * fade), 16, 1.f);
}

inline float check_row(Card& c, const char* label, bool* v, const char* desc = nullptr) {
    float h = desc ? 44.f : 30.f;
    ImVec2 p(c.x(), c.py());
    float w = c.cw();
    ImGui::SetCursorScreenPos(p);
    ImGui::PushID(label);
    ImGui::InvisibleButton("##row", ImVec2(w - 52.f, h));
    bool click = ImGui::IsItemClicked();
    bool hov = c.enabled && ImGui::IsItemHovered();
    if (click && c.enabled) *v = !*v;

    float a = anim_to(ImGui::GetID("##hov"), hov ? 1.f : 0.f, 14.f);
    if (a > .01f)
        c.dl->AddRectFilled(ImVec2(p.x - 8.f, p.y - 1.f), ImVec2(p.x + w + 8.f, p.y + h - 3.f),
                            IM_COL32(255, 255, 255, (int)(5.f * a)), 7.f);

    float oy = desc ? 1.f : (h - 14.f) * .5f;
    c.dl->AddText(f_body, 13.5f, ImVec2(p.x, p.y + oy), scale_a(TEXT, c.dim()), label);
    if (desc) c.dl->AddText(f_small, 10.5f, ImVec2(p.x, p.y + 19.f), scale_a(DIM2, c.dim()), desc);

    ImGui::PushID("##sw");
    draw_toggle_impl(ImVec2(p.x + w - 40.f, p.y + (h - 20.f) * .5f), 40.f, 20.f, v, c.enabled, ImGui::GetID("##tg"));
    ImGui::PopID();
    ImGui::PopID();

    c.y += h + 2.f;
    return h;
}

inline float slider_row(Card& c, const char* label, float* v, float lo, float hi,
                        const char* fmt = "%.1f", const char* unit = nullptr) {
    float h = 46.f;
    ImVec2 p(c.x(), c.py());
    float w = c.cw();
    ImGui::SetCursorScreenPos(p);
    ImGui::PushID(label);
    ImGui::InvisibleButton("##s", ImVec2(w, h));
    bool act = c.enabled && ImGui::IsItemActive();
    bool hov = c.enabled && ImGui::IsItemHovered();
    bool changed = false;
    if (act && hi > lo) {
        float t = clamp01((ImGui::GetIO().MousePos.x - p.x) / w);
        float nv = lo + t * (hi - lo);
        if (nv != *v) { *v = nv; changed = true; }
    }

    float t = hi > lo ? clamp01((*v - lo) / (hi - lo)) : 0.f;
    float tt = anim_to(ImGui::GetID("##fill"), t, 22.f);
    float hv = anim_to(ImGui::GetID("##hov"), hov ? 1.f : 0.f, 16.f);
    float fade = c.dim();

    c.dl->AddText(f_small, 11.f, ImVec2(p.x, p.y + 1.f), scale_a(DIM, fade), label);
    char buf[64];
    snprintf(buf, sizeof(buf), fmt, *v);
    if (unit) { strncat(buf, unit, sizeof(buf) - strlen(buf) - 1); }
    float vw = f_bold->CalcTextSizeA(12.f, FLT_MAX, 0.f, buf).x;
    c.dl->AddText(f_bold, 12.f, ImVec2(p.x + w - vw, p.y), scale_a(TEXT, fade), buf);

    float ty = p.y + 30.f;
    c.dl->AddRectFilled(ImVec2(p.x, ty - 2.5f), ImVec2(p.x + w, ty + 2.5f), TRACK, 3.f);
    float fx = p.x + w * tt;
    if (tt > .004f) {
        c.dl->AddRectFilledMultiColor(ImVec2(p.x, ty - 2.5f), ImVec2(fx, ty + 2.5f),
                                      with_a(ACCENT_A, fade), with_a(mix(ACCENT_A, ACCENT_B, tt), fade),
                                      with_a(mix(ACCENT_A, ACCENT_B, tt), fade), with_a(ACCENT_A, fade));
        c.dl->AddCircleFilled(ImVec2(fx, ty), 2.5f, with_a(mix(ACCENT_A, ACCENT_B, tt), fade));
    }
    float kr = 5.5f + hv * 1.5f;
    c.dl->AddCircleFilled(ImVec2(fx, ty), kr, scale_a(IM_COL32(246, 249, 253, 255), fade));
    c.dl->AddCircle(ImVec2(fx, ty), kr + 1.f, with_a(ACCENT_A, .25f * fade), 20, 1.f);

    ImGui::PopID();
    c.y += h;
    return changed;
}

inline float slider_row_i(Card& c, const char* label, int* v, int lo, int hi) {
    float f = (float)*v;
    bool ch = slider_row(c, label, &f, (float)lo, (float)hi, "%.0f");
    if (ch) *v = (int)(f + .5f);
    return ch;
}

inline bool combo_row(Card& c, const char* label, int* v, const char* const* items, int n) {
    float h = 34.f;
    ImVec2 p(c.x(), c.py());
    float w = c.cw();
    float cw = 138.f, ch = 26.f;
    ImVec2 cp(p.x + w - cw, p.y + (h - ch) * .5f);

    c.dl->AddText(f_body, 13.5f, ImVec2(p.x, p.y + 8.f), scale_a(DIM, c.dim()), label);

    bool changed = false;
    ImGui::PushID(label);
    ImGui::SetCursorScreenPos(cp);
    ImGui::InvisibleButton("##chip", ImVec2(cw, ch));
    bool hov = c.enabled && ImGui::IsItemHovered();
    if (ImGui::IsItemClicked() && c.enabled) ImGui::OpenPopup("##pop");

    float hv = anim_to(ImGui::GetID("##h"), hov ? 1.f : 0.f, 16.f);
    c.dl->AddRectFilled(cp, cp + ImVec2(cw, ch),
                        mix(IM_COL32(24, 27, 36, 255), IM_COL32(30, 34, 45, 255), hv), 7.f);
    c.dl->AddRect(cp, cp + ImVec2(cw, ch), mix(STROKE, ACCENT_A, hv * .45f), 7.f, 0, 1.f);

    const char* txt = (v && *v >= 0 && *v < n) ? items[*v] : "-";
    c.dl->AddText(f_body, 12.5f, cp + ImVec2(10.f, 6.f), scale_a(TEXT, .85f), txt);
    ImVec2 chv(cp.x + cw - 15.f, cp.y + ch * .5f);
    c.dl->AddLine(chv, ImVec2(chv.x + 4.f, chv.y + 4.f), DIM, 1.4f);
    c.dl->AddLine(ImVec2(chv.x + 4.f, chv.y + 4.f), ImVec2(chv.x + 8.f, chv.y), DIM, 1.4f);

    if (ImGui::BeginPopup("##pop")) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 2.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.f, 6.f));
        for (int i = 0; i < n; i++) {
            bool sel = (i == *v);
            ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(.93f, .945f, .965f, 1.f) : v4(206, 212, 224));
            if (ImGui::Selectable(items[i], sel, 0, ImVec2(cw - 10.f, 0.f))) {
                *v = i;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleVar(2);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    c.y += h;
    return changed;
}

inline bool color_edit(ImVec2 pos, const char* id, float* col, bool alpha, bool enabled = true) {
    ImGui::PushID(id);
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton("##sw", ImVec2(22.f, 22.f));
    bool hov = enabled && ImGui::IsItemHovered();
    if (ImGui::IsItemClicked() && enabled) ImGui::OpenPopup("##pick");

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float hv = anim_to(ImGui::GetID("##h"), hov ? 1.f : 0.f, 18.f);
    float fade = enabled ? 1.f : .55f;
    ImU32 c = IM_COL32((int)(col[0] * 255), (int)(col[1] * 255), (int)(col[2] * 255),
                       alpha ? (int)(col[3] * 255) : 255);
    dl->AddRectFilled(ImVec2(pos.x - 2.f, pos.y - 2.f), ImVec2(pos.x + 24.f, pos.y + 24.f),
                      with_a(c, .14f * hv * fade), 8.f);
    dl->AddRectFilled(pos, ImVec2(pos.x + 22.f, pos.y + 22.f), scale_a(c, fade), 6.f);
    dl->AddRect(pos, ImVec2(pos.x + 22.f, pos.y + 22.f), with_a(WHITE, (.10f + .20f * hv) * fade), 6.f, 0, 1.f);

    bool ch = false;
    ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y + 28.f), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("##pick")) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 10.f));
        if (alpha) ch = ImGui::ColorPicker4("##p", col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs);
        else       ch = ImGui::ColorPicker3("##p", col, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs);
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return ch;
}

inline float color_row(Card& c, const char* label, float* col, bool alpha = false) {
    float h = 30.f;
    ImVec2 p(c.x(), c.py());
    c.dl->AddText(f_body, 13.5f, ImVec2(p.x, p.y + 7.f), scale_a(DIM, c.dim()), label);
    color_edit(ImVec2(p.x + c.cw() - 22.f, p.y + 4.f), label, col, alpha, c.enabled);
    c.y += h;
    return h;
}

inline float color_pair(Card& c, const char* la, float* ca, const char* lb, float* cb) {
    float h = 30.f;
    ImVec2 p(c.x(), c.py());
    float w = c.cw();
    float half = w * .5f;
    c.dl->AddText(f_small, 11.5f, ImVec2(p.x, p.y + 8.f), scale_a(DIM, c.dim()), la);
    c.dl->AddText(f_small, 11.5f, ImVec2(p.x + half + 10.f, p.y + 8.f), scale_a(DIM, c.dim()), lb);
    color_edit(ImVec2(p.x + half - 30.f, p.y + 4.f), la, ca, false, c.enabled);
    color_edit(ImVec2(p.x + w - 22.f, p.y + 4.f), lb, cb, false, c.enabled);
    c.y += h;
    return h;
}

inline const char* key_name(int vk, char* buf, int n) {
    if (!vk) return "always";
    switch (vk) {
    case VK_LBUTTON: return "mouse1";
    case VK_RBUTTON: return "mouse2";
    case VK_MBUTTON: return "mouse3";
    case VK_XBUTTON1: return "mouse4";
    case VK_XBUTTON2: return "mouse5";
    case VK_TAB: return "tab";
    case VK_RETURN: return "enter";
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return "shift";
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return "ctrl";
    case VK_MENU: case VK_LMENU: case VK_RMENU: return "alt";
    case VK_SPACE: return "space";
    case VK_CAPITAL: return "caps";
    case VK_ESCAPE: return "esc";
    case VK_INSERT: return "insert";
    case VK_DELETE: return "delete";
    case VK_HOME: return "home";
    case VK_END: return "end";
    case VK_PRIOR: return "pgup";
    case VK_NEXT: return "pgdn";
    case VK_LEFT: return "left";
    case VK_RIGHT: return "right";
    case VK_UP: return "up";
    case VK_DOWN: return "down";
    default: break;
    }
    UINT sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    if (GetKeyNameTextA((LONG)(sc << 16), buf, n) > 0) return buf;
    snprintf(buf, n, "vk 0x%02X", vk);
    return buf;
}

// draw a single line of text truncated with an ellipsis to fit maxw pixels
inline void text_fit(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, float maxw, ImU32 col, const char* s) {
    if (!s) return;
    if (font->CalcTextSizeA(size, FLT_MAX, 0.f, s).x <= maxw) {
        dl->AddText(font, size, pos, col, s);
        return;
    }
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", s);
    int len = (int)strlen(buf);
    while (len > 3) {
        buf[--len] = 0;
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s..", buf);
        if (font->CalcTextSizeA(size, FLT_MAX, 0.f, tmp).x <= maxw) {
            dl->AddText(font, size, pos, col, tmp);
            return;
        }
    }
    dl->AddText(font, size, pos, col, "..");
}

inline bool g_key_down[256] = { false };

// snapshot held keys so arming a bind cannot instantly capture an already-down key
inline void key_sync() {
    for (int k = 0; k < 256; k++)
        g_key_down[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
}

inline bool key_edge(int vk) {
    if (vk < 0 || vk > 255) return false;
    bool d = (GetAsyncKeyState(vk) & 0x8000) != 0;
    bool e = d && !g_key_down[vk];
    g_key_down[vk] = d;
    return e;
}

inline ImGuiID& bind_armed() { static ImGuiID id = 0; return id; }

inline float keybind_row(Card& c, const char* label, int* vk) {
    float h = 32.f;
    ImVec2 p(c.x(), c.py());
    float w = c.cw();
    c.dl->AddText(f_body, 13.5f, ImVec2(p.x, p.y + 8.f), scale_a(DIM, c.dim()), label);

    float cw = 92.f, ch = 24.f;
    ImVec2 cp(p.x + w - cw, p.y + (h - ch) * .5f);
    ImGui::PushID(label);
    ImGui::SetCursorScreenPos(cp);
    ImGui::InvisibleButton("##bind", ImVec2(cw, ch));
    ImGuiID myid = ImGui::GetItemID();
    bool hov = c.enabled && ImGui::IsItemHovered();
    if (ImGui::IsItemClicked() && c.enabled) {
        bind_armed() = (bind_armed() == myid) ? 0 : myid;
        if (bind_armed()) key_sync();
    }

    bool armed = bind_armed() == myid;
    if (armed) {
        static const int mouse_keys[5] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
        bool captured = false;
        for (int i = 0; i < 5 && !captured; i++) {
            int k = mouse_keys[i];
            if (key_edge(k)) { *vk = k; captured = true; }
        }
        if (captured) {
            bind_armed() = 0;
        } else {
            for (int k = 8; k < 256; k++) {
                if (key_edge(k)) {
                    if (k != VK_ESCAPE) *vk = k;
                    bind_armed() = 0;
                    break;
                }
            }
        }
    }

    float hv = anim_to(ImGui::GetID("##h"), hov ? 1.f : 0.f, 16.f);
    c.dl->AddRectFilled(cp, cp + ImVec2(cw, ch),
                        mix(IM_COL32(24, 27, 36, 255), IM_COL32(30, 34, 45, 255), hv), 7.f);
    c.dl->AddRect(cp, cp + ImVec2(cw, ch),
                  armed ? with_a(ACCENT_A, .70f) : mix(STROKE, ACCENT_A, hv * .45f), 7.f, 0, 1.f);
    char kb[48];
    const char* nm = armed ? "press key" : key_name(*vk, kb, sizeof(kb));
    float tw = f_body->CalcTextSizeA(12.f, FLT_MAX, 0.f, nm).x;
    c.dl->AddText(f_body, 12.f, ImVec2(cp.x + (cw - tw) * .5f, cp.y + 5.f),
                  armed ? TEXT : scale_a(TEXT, .82f), nm);
    ImGui::PopID();

    c.y += h;
    return false;
}

inline float drag3_row(Card& c, const char* label, float* v, float lo, float hi, float step) {
    float h = 32.f;
    ImVec2 p(c.x(), c.py());
    float w = c.cw();
    c.dl->AddText(f_body, 13.5f, ImVec2(p.x, p.y + 8.f), scale_a(DIM, c.dim()), label);
    ImGui::SetCursorScreenPos(ImVec2(p.x + w - 178.f, p.y + 2.f));
    ImGui::SetNextItemWidth(178.f);
    ImGui::PushID(label);
    ImGui::DragFloat3("##d", v, step, lo, hi, "%.1f");
    ImGui::PopID();
    c.y += h;
    return h;
}

inline void section_label(Card& c, const char* text) {
    c.y += 8.f;
    c.dl->AddText(f_small, 10.f, ImVec2(c.x(), c.py()), scale_a(DIM2, c.dim()), text);
    c.dl->AddRectFilled(ImVec2(c.x() + f_small->CalcTextSizeA(10.f, FLT_MAX, 0.f, text).x + 8.f, c.y + 6.f),
                        ImVec2(c.x() + c.cw(), c.y + 7.f), IM_COL32(255, 255, 255, 8), 0.f);
    c.y += 16.f;
}

inline bool button_at(Card& c, const char* label, ImVec2 p, float w, bool accent = false) {
    float h = 30.f;
    ImGui::SetCursorScreenPos(p);
    ImGui::PushID(label);
    ImGui::InvisibleButton("##btn", ImVec2(w, h));
    bool hov = c.enabled && ImGui::IsItemHovered();
    bool clicked = c.enabled && ImGui::IsItemClicked();
    float hv = anim_to(ImGui::GetID("##h"), hov ? 1.f : 0.f, 16.f);
    ImU32 base = accent ? mix(IM_COL32(198, 203, 212, 255), IM_COL32(246, 248, 251, 255), hv)
                        : mix(IM_COL32(255, 255, 255, 14), IM_COL32(255, 255, 255, 30), hv);
    c.dl->AddRectFilled(p, p + ImVec2(w, h), base, 8.f);
    if (accent) c.dl->AddRectFilled(p, p + ImVec2(w, h), with_a(WHITE, .10f * hv), 8.f);
    c.dl->AddRect(p, p + ImVec2(w, h), accent ? with_a(WHITE, .55f) : with_a(WHITE, .09f), 8.f, 0, 1.f);
    float tw = f_bold->CalcTextSizeA(12.f, FLT_MAX, 0.f, label).x;
    c.dl->AddText(f_bold, 12.f, ImVec2(p.x + (w - tw) * .5f, p.y + 8.f),
                  accent ? IM_COL32(12, 13, 16, 255) : TEXT, label);
    ImGui::PopID();
    return clicked;
}

inline bool button(Card& c, const char* label, float w, bool accent = false) {
    ImVec2 p(c.x() + (c.cw() - w) * .5f, c.py());
    bool r = button_at(c, label, p, w, accent);
    c.y += 30.f;
    return r;
}

inline void copy_clip(const char* s) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    size_t n = strlen(s) + 1;
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n);
    if (h) {
        void* d = GlobalLock(h);
        if (d) {
            memcpy(d, s, n);
            GlobalUnlock(h);
            SetClipboardData(CF_TEXT, h);
        } else {
            GlobalFree(h);
        }
    }
    CloseClipboard();
}

inline void kv_row(Card& c, const char* k, const char* v) {
    ImVec2 p(c.x(), c.py());
    c.dl->AddText(f_mono, 11.f, p, scale_a(DIM2, c.dim()), k);
    float kw = f_mono->CalcTextSizeA(11.f, FLT_MAX, 0.f, k).x;
    c.dl->AddText(f_mono, 11.f, ImVec2(p.x + kw + 12.f, p.y), scale_a(TEXT, .85f), v);
    c.y += 17.f;
}

// ---------------------------------------------------------------------------
// icons
// ---------------------------------------------------------------------------
inline void icon(ImDrawList* dl, int kind, ImVec2 c, float s, ImU32 col, float th) {
    switch (kind) {
    case 0:
        dl->AddEllipse(c, ImVec2(s * .95f, s * .58f), col, 0.f, 28, th);
        dl->AddCircleFilled(c, s * .24f, col);
        break;
    case 1:
        dl->AddCircle(c, s * .62f, col, 22, th);
        dl->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x - s * .35f, c.y), col, th);
        dl->AddLine(ImVec2(c.x + s * .35f, c.y), ImVec2(c.x + s, c.y), col, th);
        dl->AddLine(ImVec2(c.x, c.y - s), ImVec2(c.x, c.y - s * .35f), col, th);
        dl->AddLine(ImVec2(c.x, c.y + s * .35f), ImVec2(c.x, c.y + s), col, th);
        break;
    case 2:
        dl->AddCircle(c, s * .70f, col, 26, th);
        dl->AddEllipse(c, ImVec2(s * .31f, s * .70f), col, 0.f, 20, th);
        dl->AddLine(ImVec2(c.x - s * .70f, c.y), ImVec2(c.x + s * .70f, c.y), col, th);
        break;
    case 3:
        for (int i = 0; i < 3; i++) {
            float y = c.y + (i - 1) * s * .52f;
            dl->AddLine(ImVec2(c.x - s * .75f, y), ImVec2(c.x + s * .75f, y), col, th);
            float kx = c.x + ((i == 0) ? -.32f : (i == 1 ? .34f : .02f)) * s * 1.3f;
            dl->AddCircleFilled(ImVec2(kx, y), s * .17f, col);
        }
        break;
    default:
        dl->AddCircle(c, s * .70f, col, 24, th);
        dl->AddCircleFilled(c, s * .20f, col);
        break;
    }
}

// ---------------------------------------------------------------------------
// state handed down from main.cpp
// ---------------------------------------------------------------------------
struct State {
    bool eng = false, pm = false, si = false;
    float fps = 0.f;
    int players = 0;
    unsigned eng_addr = 0, pm_addr = 0, si_addr = 0;
    float punch[3] = { 0.f, 0.f, 0.f };
    float vel[3] = { 0.f, 0.f, 0.f };
    float speed = 0.f;
    int flags = 0;
    bool on_ground = false;
    const char* build = "2.0";
};

// ---------------------------------------------------------------------------
// pages
// ---------------------------------------------------------------------------
inline const char* k_glass_quality[] = { "Performance", "Balanced", "Smooth" };
inline const char* k_fog_modes[] = { "Linear", "Exponential", "Exponential squared" };
inline const char* k_esp_styles[] = { "Box", "Corner" };
inline const char* k_chams[] = { "Off", "Flat", "Wire", "Glow", "Rainbow" };
inline const char* k_hand[] = { "Flat", "Wire", "Glass", "Pulse" };
inline const char* k_bone[] = { "Head", "Body" };
inline const char* k_part[] = { "Any", "Head", "Body" };

// fps history shared by the rail and the data center card
inline float* fps_hist() { static float h[48] = { 0.f }; return h; }
inline int& fps_head() { static int i = 0; return i; }
inline void fps_sample(float v) {
    static int t = 0;
    if ((t++ & 3) == 0) {
        float* h = fps_hist();
        h[fps_head()] = v;
        fps_head() = (fps_head() + 1) % 48;
    }
}
inline void fps_graph(ImDrawList* dl, ImVec2 p, float w, float h, ImU32 col) {
    const int n = 48;
    float* hist = fps_hist();
    int head = fps_head();
    float bw = std::max(1.f, w / n - 1.5f);
    for (int i = 0; i < n; i++) {
        int idx = (head + i) % n;
        float t = clamp01(hist[idx] / 240.f);
        float bh = 2.f + t * (h - 2.f);
        float a = .10f + .42f * (float)i / (float)n;
        dl->AddRectFilled(ImVec2(p.x + i * (w / n), p.y + h - bh),
                          ImVec2(p.x + i * (w / n) + bw, p.y + h), with_a(col, a), 1.f);
    }
}

inline void page_visuals(ImVec2 o, float full) {
    const float gap = 10.f;
    const bool stacked = full < 520.f;
    const float colw = stacked ? full : (full - gap) * .5f;
    const float xl = o.x, xr = stacked ? o.x : o.x + colw + gap;
    float yl = o.y, yr = o.y;

    Card a = card_begin(ImVec2(xl, yl), colw, "ESP", "player boxes & info", &C::esp);
    combo_row(a, "Box style", &C::espStyle, k_esp_styles, 2);
    check_row(a, "Box", &C::espBox);
    check_row(a, "Name", &C::espName);
    check_row(a, "Distance", &C::espDist);
    check_row(a, "Snaplines", &C::espSnap);
    check_row(a, "Motion trail", &C::espTrail);
    check_row(a, "Outer glow", &C::espGlow);
    check_row(a, "Ignore teammates", &C::espIgnoreTeam);
    slider_row(a, "Opacity", &C::espAlpha, .15f, 1.f, "%.2f");
    slider_row(a, "Max distance", &C::espMaxDist, 0.f, 250.f, "%.0f", " m");
    section_label(a, "TEAM COLORS");
    color_pair(a, "Terrorist", C::ecT, "Counter", C::ecCT);
    yl = card_end(a) + gap;
    if (stacked) yr = yl;

    bool ch_on = C::chMode > 0;
    Card b = card_begin(ImVec2(xr, yr), colw, "Chams", "player shading", &ch_on);
    if (ch_on != (C::chMode > 0)) C::chMode = ch_on ? 1 : 0;
    combo_row(b, "Mode", &C::chMode, k_chams, 5);
    slider_row(b, "Opacity", &C::chamsAlpha, .1f, 1.f, "%.2f");
    section_label(b, "COLORS");
    color_pair(b, "T visible", C::chTV, "T wall", C::chTW);
    color_pair(b, "CT visible", C::chCV, "CT wall", C::chCW);
    yr = card_end(b) + gap;
    if (stacked) yl = yr;

    Card c = card_begin(ImVec2(xl, yl), colw, "ViewModel", "hands & offset", &C::hand);
    combo_row(c, "Material", &C::handM, k_hand, 4);
    color_row(c, "Tint", C::handC);
    slider_row(c, "Opacity", &C::handAlpha, .05f, 1.f, "%.2f");
    if (C::handM != 0)
        slider_row(c, "Edge width", &C::handLineWidth, .5f, 5.f, "%.1f");
    if (C::handM >= 2) {
        c.dl->AddText(f_small, 10.f, ImVec2(c.x(), c.py()+2.f), DIM2,
                      C::handM==3 ? "soft pulse / dual-pass glass" : "translucent fill / luminous edges");
        c.y += 17.f;
    }
    drag3_row(c, "XYZ offset", C::vmOfs, -5.f, 5.f, .05f);
    yl = card_end(c) + gap;
    if (stacked) yr = yl;

    Card d = card_begin(ImVec2(xr, yr), colw, "Crosshair", "reticle appearance", &C::xhair);
    slider_row(d, "Length", &C::xhSize, 1.f, 20.f, "%.1f");
    slider_row(d, "Gap", &C::xhGap, 0.f, 10.f, "%.1f");
    slider_row(d, "Thickness", &C::xhThick, .5f, 4.f, "%.1f");
    color_row(d, "Color", C::xhCol, true);
    check_row(d, "Center dot", &C::xhDot);
    check_row(d, "Dark outline", &C::xhOutline);
    section_label(d, "LIVE PREVIEW");
    {
        const float pw = d.cw();
        const float ph = 62.f;
        const ImVec2 pp(d.x(), d.py());
        d.dl->AddRectFilled(pp, ImVec2(pp.x + pw, pp.y + ph),
                            IM_COL32(8, 9, 12, 175), 9.f);
        d.dl->AddRect(pp, ImVec2(pp.x + pw, pp.y + ph),
                      IM_COL32(255, 255, 255, 20), 9.f, 0, 1.f);
        const float px = pp.x + pw * .5f, py = pp.y + ph * .5f;
        const float gapPreview = std::min(12.f, C::xhGap * .7f);
        const float lenPreview = std::min(24.f, C::xhSize * 1.15f);
        const float thickPreview = std::max(1.f, C::xhThick);
        const float alphaPreview = clamp01(C::xhCol[3]);
        const ImU32 preview = IM_COL32((int)(C::xhCol[0] * 255),
            (int)(C::xhCol[1] * 255), (int)(C::xhCol[2] * 255),
            (int)(alphaPreview * 255));
        const ImU32 outline = IM_COL32(0, 0, 0, (int)(alphaPreview * 220));
        auto previewLine = [&](ImVec2 a, ImVec2 b) {
            if (C::xhOutline) d.dl->AddLine(a, b, outline, thickPreview + 2.f);
            d.dl->AddLine(a, b, preview, thickPreview);
        };
        previewLine(ImVec2(px-gapPreview-lenPreview,py),ImVec2(px-gapPreview,py));
        previewLine(ImVec2(px+gapPreview,py),ImVec2(px+gapPreview+lenPreview,py));
        previewLine(ImVec2(px,py-gapPreview-lenPreview),ImVec2(px,py-gapPreview));
        previewLine(ImVec2(px,py+gapPreview),ImVec2(px,py+gapPreview+lenPreview));
        if (C::xhDot) {
            if (C::xhOutline) d.dl->AddCircleFilled(ImVec2(px,py),3.f,outline,16);
            d.dl->AddCircleFilled(ImVec2(px,py),std::max(1.f,thickPreview*.7f),preview,16);
        }
        d.y += ph + 9.f;
    }
    yr = card_end(d) + gap;

    ImGui::SetCursorScreenPos(ImVec2(o.x, std::max(yl, yr)));
}

inline void page_aim(ImVec2 o, float full, State& st) {
    const float gap = 10.f;
    const bool stacked = full < 520.f;
    const float colw = stacked ? full : (full - gap) * .5f;
    const float secondX = stacked ? o.x : o.x + colw + gap;
    float y = o.y;

    Card a = card_begin(ImVec2(o.x, y), colw, "Aimbot", "target selection", &C::aim);
    check_row(a, "Silent aim", &C::silent, "writes angles per shot only");
    keybind_row(a, "Activation key", &C::aimKey);
    slider_row(a, "FOV", &C::aimFov, 1.f, 45.f, "%.1f", "\xC2\xB0");
    slider_row(a, "Smoothing", &C::aimSmooth, 1.f, 20.f, "%.1f");
    combo_row(a, "Bone", &C::aimBone, k_bone, 2);
    check_row(a, "Visibility check", &C::aimVis);
    float ba = card_end(a);

    const float secondY = stacked ? ba + gap : y;
    Card b = card_begin(ImVec2(secondX, secondY), colw, "Triggerbot", "auto fire", &C::trig);
    slider_row(b, "FOV", &C::trigFov, .5f, 10.f, "%.1f", "\xC2\xB0");
    slider_row_i(b, "Delay", &C::trigDelay, 0, 200);
    combo_row(b, "Part", &C::trigPart, k_part, 3);
    float bb1 = card_end(b);

    float y2 = bb1 + gap;
    const float thirdY = stacked ? bb1 + gap : y2;
    Card d = card_begin(ImVec2(secondX, thirdY), colw, "Recoil", "punch compensation", &C::rcs);
    slider_row(d, "Factor", &C::rcsF, 0.f, 3.f, "%.2f");
    d.dl->AddText(f_mono, 10.f, ImVec2(d.x(), d.py() + 1.f), scale_a(DIM2, d.dim()), "source  pm->punchangle");
    d.y += 16.f;
    {
        char buf[96];
        snprintf(buf, sizeof(buf), "punch  %.2f  %.2f  %.2f", st.punch[0], st.punch[1], st.punch[2]);
        d.dl->AddText(f_mono, 10.5f, ImVec2(d.x(), d.py() + 2.f), st.pm ? scale_a(DIM, d.dim()) : scale_a(DIM2, d.dim()), buf);
        d.y += 16.f;
    }
    float bb2 = card_end(d);

    y = std::max(ba, std::max(bb1, bb2)) + gap;
    ImGui::SetCursorScreenPos(ImVec2(o.x, y));
}

inline void page_world(ImVec2 o, float full) {
    const float gap = 10.f;
    const bool stacked = full < 520.f;
    const float colw = stacked ? full : (full - gap) * .5f;
    const float secondX = stacked ? o.x : o.x + colw + gap;
    float y = o.y;

    Card a = card_begin(ImVec2(o.x, y), colw, "Environment", "fog & lighting", nullptr);
    check_row(a, "Fog", &C::fogOn);
    color_row(a, "Fog color", C::fogC);
    combo_row(a, "Fog model", &C::fogMode, k_fog_modes, 3);
    if (C::fogMode == 0) {
        slider_row(a, "Start", &C::fogS, 0.f, 2000.f, "%.0f");
        slider_row(a, "End", &C::fogE, 500.f, 8000.f, "%.0f");
    } else {
        slider_row(a, "Density", &C::fogDensity, .001f, .15f, "%.3f");
    }
    section_label(a, "LIGHTING");
    check_row(a, "Night mode", &C::nightMode);
    slider_row(a, "Darkness", &C::nightAlpha, .1f, .8f, "%.2f");
    check_row(a, "No flash", &C::noFlash);
    float ba = card_end(a);

    const float effectsY = stacked ? ba + gap : y;
    Card b = card_begin(ImVec2(secondX, effectsY), colw, "Effects", "tracers & overlays", nullptr);
    check_row(b, "Bullet tracers", &C::btrace);
    color_row(b, "Tracer color", C::btCol);
    section_label(b, "TRAIL");
    color_row(b, "Trail color", C::trailCol);
    section_label(b, "OVERLAY");
    check_row(b, "Scope overlay", &C::scopeOvr);
    check_row(b, "No damage flash", &C::dmgFlash);
    float bb = card_end(b);

    const float radarY = bb + gap;
    Card r = card_begin(ImVec2(secondX, radarY), colw, "Radar", "2d player radar", &C::radar);
    check_row(r, "Enemies only", &C::radarEnemies, "hide teammates on the radar");
    slider_row(r, "Range", &C::radarRange, 400.f, 4000.f, "%.0f");
    slider_row(r, "Size", &C::radarSize, 90.f, 220.f, "%.0f", " px");
    float br = card_end(r);

    y = std::max(ba, std::max(bb, br)) + gap;
    ImGui::SetCursorScreenPos(ImVec2(o.x, y));
}

inline void page_misc(ImVec2 o, float full, State& st) {
    const float gap = 10.f;
    const bool stacked = full < 520.f;
    const float colw = stacked ? full : (full - gap) * .5f;
    const float secondX = stacked ? o.x : o.x + colw + gap;
    float y = o.y;

    Card a = card_begin(ImVec2(o.x, y), colw, "Movement", "input helpers", nullptr);
    check_row(a, "Bunnyhop", &C::bhop, "auto jump on landing");
    check_row(a, "Auto-strafe", &C::autoStrafe, "air strafe assist");
    check_row(a, "Auto pistol", &C::autoPistol, "pulse semi-auto fire");
    float ba = card_end(a);

    const float interfaceY = stacked ? ba + gap : y;
    Card b = card_begin(ImVec2(secondX, interfaceY), colw, "Interface", "overlay options", nullptr);
    check_row(b, "Watermark", &C::watermark);
    check_row(b, "Frosted glass", &C::glass, "blur the scene behind the menu");
    if (C::glass) {
        slider_row(b, "Glass opacity", &C::glassOpacity, .45f, 1.f, "%.2f");
        combo_row(b, "Backdrop quality", &C::glassQuality, k_glass_quality, 3);
    }
    check_row(b, "Reduce motion", &C::reduceMotion, "shorter transitions");
    keybind_row(b, "Menu key", &C::menuKey);
    float bb = card_end(b);

    y = std::max(ba, bb) + gap;

    Card d = card_begin(ImVec2(o.x, y), full, "Data center", "live engine telemetry", nullptr);
    char v[160];
    float half = d.cw() * .5f;
    float y0 = d.py();
    auto kv = [&](const char* k, const char* val) {
        d.dl->AddText(f_mono, 10.5f, ImVec2(d.x(), d.y), scale_a(DIM2, d.dim()), k);
        d.dl->AddText(f_mono, 10.5f, ImVec2(d.x() + 92.f, d.y), scale_a(TEXT, .92f), val);
        d.y += 19.f;
    };
    snprintf(v, sizeof(v), "0x%08X   %s", st.eng_addr, st.eng ? "attached" : "waiting");
    kv("engine", v);
    snprintf(v, sizeof(v), "0x%08X   %s", st.pm_addr, st.pm ? "attached" : "waiting");
    kv("playermove", v);
    snprintf(v, sizeof(v), "0x%08X   %s", st.si_addr, st.si ? "attached" : "waiting");
    kv("studio", v);
    snprintf(v, sizeof(v), "%.0f fps   %d players   %s", st.fps, st.players, st.on_ground ? "ground" : "air");
    kv("runtime", v);
    snprintf(v, sizeof(v), "x %.2f   y %.2f   z %.2f", st.punch[0], st.punch[1], st.punch[2]);
    kv("punch", v);

    char fb[16]; snprintf(fb, sizeof(fb), "%.0f", st.fps);
    d.dl->AddText(f_big, 28.f, ImVec2(d.x() + half, y0 - 4.f), TEXT, fb);
    float fw = f_big->CalcTextSizeA(28.f, FLT_MAX, 0.f, fb).x;
    d.dl->AddText(f_small, 9.5f, ImVec2(d.x() + half + fw + 7.f, y0 + 15.f), DIM2, "FPS / 240");
    fps_graph(d.dl, ImVec2(d.x() + half, y0 + 38.f), d.cw() - half, 38.f, TEXT);
    snprintf(v, sizeof(v), "speed %.0f u/s", st.speed);
    d.dl->AddText(f_mono, 10.f, ImVec2(d.x() + half, y0 + 82.f), scale_a(DIM, d.dim()), v);
    if (d.y < y0 + 102.f) d.y = y0 + 102.f;
    float bd = card_end(d);
    y = bd + gap;
    ImGui::SetCursorScreenPos(ImVec2(o.x, y));
}

inline void page_config(ImVec2 o, float full, State& st, bool* save_click, bool* load_click, bool* reset_click) {
    const float gap = 10.f;
    const bool stacked = full < 520.f;
    const float colw = stacked ? full : (full - gap) * .5f;
    const float secondX = stacked ? o.x : o.x + colw + gap;
    float y = o.y;

    Card a = card_begin(ImVec2(o.x, y), colw, "Persistence", "toker_cs16.cfg", nullptr);
    *save_click = button(a, "Save config", 150.f, true);
    a.y += 6.f;
    *load_click = button(a, "Load config", 150.f, false);
    a.y += 6.f;
    *reset_click = button(a, "Reset to defaults", 150.f, false);
    a.y += 10.f;
    text_fit(a.dl, f_mono, 9.f, ImVec2(a.x(), a.py()), a.cw(), scale_a(DIM2, a.dim()), cfg::path());
    a.y += 16.f;
    float ba = card_end(a);

    const float aboutY = stacked ? ba + gap : y;
    Card b = card_begin(ImVec2(secondX, aboutY), colw, "About", "toker cs 1.6", nullptr);
    kv_row(b, "build", st.build ? st.build : "2.0");
    kv_row(b, "target", "GoldSrc / protocol 48");
    kv_row(b, "engine", st.eng ? "attached" : "waiting");
    a.y += 0.f;
    section_label(b, "HOTKEYS");
    char kb[48];
    kv_row(b, "menu", key_name(C::menuKey, kb, sizeof(kb)));
    kv_row(b, "save", "ctrl + s (config page)");
    float bb = card_end(b);

    y = std::max(ba, bb) + gap;

    Card t = card_begin(ImVec2(o.x, y), full, "Tools", "quick actions", nullptr);
    {
        const float sp = 8.f;
        const float by = t.py();
        if (t.cw() >= 530.f) {
            const float bw = 170.f;
            const float total = bw * 3.f + sp * 2.f;
            const float sx = t.x() + (t.cw() - total) * .5f;
            if (button_at(t, "Open config folder", ImVec2(sx, by), bw)) {
                char cmd[MAX_PATH + 32], d[MAX_PATH];
                cfg::dir(d, MAX_PATH);
                snprintf(cmd, sizeof(cmd), "explorer.exe \\"%s\\"", d);
                WinExec(cmd, SW_SHOWNORMAL);
            }
            if (button_at(t, "Copy diagnostics", ImVec2(sx + bw + sp, by), bw)) {
                char dg[256];
                snprintf(dg, sizeof(dg), "toker build %s | eng 0x%X pm 0x%X si 0x%X | %d players | %.0f fps",
                         st.build ? st.build : "?", st.eng_addr, st.pm_addr, st.si_addr, st.players, st.fps);
                copy_clip(dg);
            }
            if (button_at(t, "Reload config", ImVec2(sx + (bw + sp) * 2.f, by), bw)) cfg::load();
            t.y += 38.f;
        } else {
            const float bw = std::min(220.f, t.cw());
            const float sx = t.x() + (t.cw() - bw) * .5f;
            if (button_at(t, "Open config folder", ImVec2(sx, t.y), bw)) {
                char cmd[MAX_PATH + 32], d[MAX_PATH];
                cfg::dir(d, MAX_PATH);
                snprintf(cmd, sizeof(cmd), "explorer.exe \\"%s\\"", d);
                WinExec(cmd, SW_SHOWNORMAL);
            }
            t.y += 36.f;
            if (button_at(t, "Copy diagnostics", ImVec2(sx, t.y), bw)) {
                char dg[256];
                snprintf(dg, sizeof(dg), "toker build %s | eng 0x%X pm 0x%X si 0x%X | %d players | %.0f fps",
                         st.build ? st.build : "?", st.eng_addr, st.pm_addr, st.si_addr, st.players, st.fps);
                copy_clip(dg);
            }
            t.y += 36.f;
            if (button_at(t, "Reload config", ImVec2(sx, t.y), bw)) cfg::load();
            t.y += 36.f;
        }
    }
    float bt = card_end(t);

    y = bt + gap;
    ImGui::SetCursorScreenPos(ImVec2(o.x, y));
}

// ---------------------------------------------------------------------------
// menu
// ---------------------------------------------------------------------------
inline const char* k_pages[] = { "Visuals", "Aim", "World", "Interface", "Profiles" };
inline const char* k_subs[] = {
    "render settings and appearance",
    "input controls and sensitivity",
    "atmosphere and environment",
    "overlay, motion and telemetry",
    "save, load and preferences",
};

inline int& page_index() { static int p = 0; return p; }

inline void draw(State& st, bool* open) {
    int& page = page_index();
    static bool save_click = false, load_click = false, reset_click = false;

    fps_sample(st.fps);

    ImGuiIO& io = ImGui::GetIO();
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * .5f, vp->WorkPos.y + vp->WorkSize.y * .5f),
                            ImGuiCond_FirstUseEver, ImVec2(.5f, .5f));
    // stay usable on 800x600 / stretched low-res GoldSrc viewports
    ImVec2 ds = io.DisplaySize;
    float dw = (ds.x > 0.f) ? std::min(1000.f, std::max(560.f, ds.x - 48.f)) : 1000.f;
    float dh = (ds.y > 0.f) ? std::min(680.f, std::max(440.f, ds.y - 48.f)) : 680.f;
    ImGui::SetNextWindowSize(ImVec2(dw, dh), ImGuiCond_FirstUseEver);
    float mw = (ds.x > 0.f) ? std::min(900.f, std::max(520.f, ds.x - 24.f)) : 900.f;
    float mh = (ds.y > 0.f) ? std::min(620.f, std::max(400.f, ds.y - 24.f)) : 620.f;
    ImGui::SetNextWindowSizeConstraints(ImVec2(mw, mh), ImVec2(1600.f, 1000.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##toker_ui", open,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoSavedSettings);

    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Give the content area more room on narrow GoldSrc resolutions.
    const float RAIL = ws.x < 820.f ? 188.f : 224.f;
    const float HEAD = 84.f;

    // shell: real blurred backdrop + glass coats
    glass_shell(dl, wp, wp + ws, 16.f);
    dl->AddRectFilled(ImVec2(wp.x + 1.f, wp.y + 1.f), ImVec2(wp.x + RAIL, wp.y + ws.y - 1.f),
                      IM_COL32(7, 8, 11, 74), 15.f, ImDrawFlags_RoundCornersLeft);
    dl->AddRectFilledMultiColor(ImVec2(wp.x + 1.f, wp.y + 1.f), ImVec2(wp.x + RAIL, wp.y + 150.f),
                                IM_COL32(255, 255, 255, 5), IM_COL32(255, 255, 255, 5),
                                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    dl->AddLine(ImVec2(wp.x + RAIL, wp.y + 18.f), ImVec2(wp.x + RAIL, wp.y + ws.y - 18.f),
                IM_COL32(255, 255, 255, 14), 1.f);

    // logo mark
    dl->AddRectFilled(ImVec2(wp.x + 22.f, wp.y + 22.f), ImVec2(wp.x + 56.f, wp.y + 56.f),
                      IM_COL32(240, 242, 246, 255), 10.f);
    dl->AddCircleFilled(ImVec2(wp.x + 39.f, wp.y + 39.f), 11.f, IM_COL32(12, 13, 16, 255));
    dl->AddCircleFilled(ImVec2(wp.x + 39.f, wp.y + 39.f), 4.2f, IM_COL32(240, 242, 246, 255));
    dl->AddCircle(ImVec2(wp.x + 39.f, wp.y + 39.f), 7.4f, IM_COL32(240, 242, 246, 130), 24, 1.f);
    dl->AddText(f_bold, 16.f, ImVec2(wp.x + 66.f, wp.y + 23.f), TEXT, "TOKER");
    dl->AddText(f_small, 9.5f, ImVec2(wp.x + 67.f, wp.y + 42.f), DIM2, "INTERNAL  /  CS 1.6");

    // nav
    for (int i = 0; i < 5; i++) {
        ImVec2 ip(wp.x + 14.f, wp.y + 96.f + i * 46.f);
        ImVec2 isz(RAIL - 28.f, 40.f);
        ImGui::SetCursorScreenPos(ip);
        ImGui::PushID(i);
        ImGui::InvisibleButton("##nav", isz);
        if (ImGui::IsItemClicked()) page = i;
        bool hov = ImGui::IsItemHovered();
        float a = anim_to(ImGui::GetID("##sel"), page == i ? 1.f : 0.f, 12.f);
        float hv = anim_to(ImGui::GetID("##hov"), hov ? 1.f : 0.f, 16.f);
        if (a > .01f)
            dl->AddRectFilled(ip, ip + isz, mix(IM_COL32(255, 255, 255, 3), IM_COL32(255, 255, 255, 14), a), 10.f);
        if (hv > .01f && a < .5f)
            dl->AddRectFilled(ip, ip + isz, IM_COL32(255, 255, 255, (int)(6.f * hv)), 10.f);
        if (a > .01f)
            dl->AddRectFilled(ImVec2(ip.x - 14.f, ip.y + 11.f), ImVec2(ip.x - 10.f, ip.y + 29.f),
                              with_a(WHITE, .88f * a), 2.f);
        icon(dl, i, ImVec2(ip.x + 21.f, ip.y + 20.f), 8.5f,
             mix(DIM2, WHITE, a), 1.5f);
        dl->AddText(f_body, 13.5f, ImVec2(ip.x + 42.f, ip.y + 11.f), mix(DIM, TEXT, a), k_pages[i]);
        ImGui::PopID();
    }

    // rail telemetry (data center)
    {
        if (ws.y >= 560.f) {
        float blk = 138.f;
        float by = wp.y + ws.y - blk - 54.f;
        dl->AddRectFilled(ImVec2(wp.x + 14.f, by), ImVec2(wp.x + RAIL - 14.f, by + blk - 8.f),
                          IM_COL32(255, 255, 255, 5), 10.f);
        dl->AddRect(ImVec2(wp.x + 14.f, by), ImVec2(wp.x + RAIL - 14.f, by + blk - 8.f),
                    IM_COL32(255, 255, 255, 13), 10.f, 0, 1.f);
        dl->AddCircleFilled(ImVec2(wp.x + 26.f, by + 17.f), 2.5f,
                            st.eng ? IM_COL32(238, 241, 246, 255) : IM_COL32(255, 120, 120, 255));
        dl->AddText(f_small, 8.5f, ImVec2(wp.x + 35.f, by + 12.f), DIM2, "DATA CENTER");
        dl->AddText(f_small, 8.5f, ImVec2(wp.x + RAIL - 58.f, by + 12.f),
                    st.eng ? DIM : DIM2, st.eng ? "LIVE" : "IDLE");

        char sv[32];
        auto stat = [&](float x, const char* lab, const char* val) {
            dl->AddText(f_small, 8.5f, ImVec2(x, by + 32.f), DIM2, lab);
            dl->AddText(f_bold, 15.f, ImVec2(x, by + 44.f), TEXT, val);
        };
        const float statCol = (RAIL - 52.f) / 3.f;
        snprintf(sv, sizeof(sv), "%.0f", st.fps);      stat(wp.x + 26.f, "FPS", sv);
        snprintf(sv, sizeof(sv), "%d", st.players);    stat(wp.x + 26.f + statCol, "PLAYERS", sv);
        snprintf(sv, sizeof(sv), "%.0f", st.speed);    stat(wp.x + 26.f + statCol * 2.f, "SPEED", sv);

        fps_graph(dl, ImVec2(wp.x + 26.f, by + 76.f), RAIL - 52.f, 36.f, TEXT);
        }

        float fy = wp.y + ws.y - 46.f;
        dl->AddLine(ImVec2(wp.x + 20.f, fy), ImVec2(wp.x + RAIL - 20.f, fy), IM_COL32(255, 255, 255, 12), 1.f);
        dl->AddCircleFilled(ImVec2(wp.x + 27.f, fy + 20.f), 3.f,
                            st.eng ? IM_COL32(238, 241, 246, 255) : IM_COL32(255, 92, 92, 255));
        dl->AddText(f_small, 10.5f, ImVec2(wp.x + 38.f, fy + 13.f), DIM,
                    st.eng ? "engine attached" : "waiting for engine");
        dl->AddText(f_small, 9.f, ImVec2(wp.x + 27.f, fy + 31.f), DIM2,
                    "TOKER  /  GLASS INTERFACE");
    }

    // header
    float cx = wp.x + RAIL + 26.f;
    dl->AddText(f_big, 21.f, ImVec2(cx, wp.y + 22.f), TEXT, k_pages[page]);
    dl->AddText(f_small, 11.f, ImVec2(cx, wp.y + 51.f), DIM, k_subs[page]);
    dl->AddRectFilled(ImVec2(cx, wp.y + HEAD - 1.f), ImVec2(cx + 30.f, wp.y + HEAD + 1.f), with_a(ACCENT_A, .85f), 1.f);

    // fps chip
    {
        char fb[32];
        snprintf(fb, sizeof(fb), "%.0f fps", st.fps);
        float fw = f_bold->CalcTextSizeA(11.f, FLT_MAX, 0.f, fb).x + 22.f;
        ImVec2 cp(wp.x + ws.x - 46.f - fw, wp.y + 26.f);
        dl->AddRectFilled(cp, cp + ImVec2(fw, 24.f), IM_COL32(255, 255, 255, 7), 12.f);
        dl->AddText(f_bold, 11.f, ImVec2(cp.x + 11.f, cp.y + 6.f), DIM, fb);
    }

    // close
    {
        ImVec2 xc(wp.x + ws.x - 40.f, wp.y + 26.f);
        ImGui::SetCursorScreenPos(xc);
        ImGui::InvisibleButton("##close", ImVec2(24.f, 24.f));
        bool hov = ImGui::IsItemHovered();
        float hv = anim_to(ImGui::GetID("##closehov"), hov ? 1.f : 0.f, 18.f);
        dl->AddCircleFilled(xc + ImVec2(12.f, 12.f), 11.f, IM_COL32(255, 255, 255, (int)(6.f + 12.f * hv)));
        ImU32 lc = mix(DIM, TEXT, hv);
        dl->AddLine(xc + ImVec2(8.f, 8.f), xc + ImVec2(16.f, 16.f), lc, 1.6f);
        dl->AddLine(xc + ImVec2(16.f, 8.f), xc + ImVec2(8.f, 16.f), lc, 1.6f);
        if (ImGui::IsItemClicked()) *open = false;
    }

    // drag regions
    {
        ImGui::SetCursorScreenPos(wp);
        ImGui::InvisibleButton("##draglogo", ImVec2(RAIL, 70.f));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
            ImGui::SetWindowPos(wp + io.MouseDelta);
        ImGui::SetCursorScreenPos(ImVec2(cx - 6.f, wp.y));
        ImGui::InvisibleButton("##draghead", ImVec2(ws.x - RAIL - 330.f, HEAD));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
            ImGui::SetWindowPos(wp + io.MouseDelta);
    }

    dl->AddLine(ImVec2(cx, wp.y + HEAD), ImVec2(wp.x + ws.x - 26.f, wp.y + HEAD),
                IM_COL32(255, 255, 255, 10), 1.f);

    // content
    ImGui::SetCursorScreenPos(ImVec2(cx, wp.y + HEAD + 14.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.f);
    ImGui::BeginChild("##scroll", ImVec2(ws.x - RAIL - 52.f, ws.y - HEAD - 34.f),
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);

    ImVec2 o = ImGui::GetCursorScreenPos();
    float full = ImGui::GetContentRegionAvail().x;

    // A quiet veil reveals each page with a short, non-blocking fade.
    static int previous_page = -1;
    static float page_reveal = 1.f;
    if (previous_page != page) {
        previous_page = page;
        page_reveal = 0.f;
    }
    const float page_dt = std::min(.1f, io.DeltaTime);
    page_reveal += (1.f - page_reveal) * std::min(1.f, page_dt * (C::reduceMotion ? 24.f : 10.f));
    o.x += (1.f - page_reveal) * (C::reduceMotion ? 2.f : 7.f);

    switch (page) {
    case 0: page_visuals(o, full); break;
    case 1: page_aim(o, full, st); break;
    case 2: page_world(o, full); break;
    case 3: page_misc(o, full, st); break;
    default: page_config(o, full, st, &save_click, &load_click, &reset_click); break;
    }

    if (page_reveal < .995f) {
        ImDrawList* page_dl = ImGui::GetWindowDrawList();
        const ImVec2 child_min = ImGui::GetWindowPos();
        const ImVec2 child_max = child_min + ImGui::GetWindowSize();
        page_dl->AddRectFilled(child_min, child_max,
            IM_COL32(10, 11, 14, (int)(170.f * (1.f - page_reveal))), 10.f);
    }

    ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos());
    ImGui::Dummy(ImVec2(full, 1.f));

    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    if (save_click)  cfg::save();
    if (load_click)  cfg::load();
    if (reset_click) cfg::reset();
    save_click = load_click = reset_click = false;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) cfg::save();

    // resize grip hint
    for (int i = 0; i < 3; i++)
        dl->AddLine(ImVec2(wp.x + ws.x - 8.f - i * 4.f, wp.y + ws.y - 8.f),
                    ImVec2(wp.x + ws.x - 8.f, wp.y + ws.y - 8.f - i * 4.f),
                    IM_COL32(255, 255, 255, 26), 1.f);

    ImGui::End();
    ImGui::PopStyleVar(2);

    // remember the shell rect: next frame's backdrop capture uses it
    g_last_rect[0] = wp.x; g_last_rect[1] = wp.y;
    g_last_rect[2] = ws.x; g_last_rect[3] = ws.y;
}

} // namespace ui
