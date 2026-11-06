// C++17, x86, MinGW i686, GoldSrc CS 1.6 CSDuragi — FULL BUILD
#include <windows.h>
#include <psapi.h>
#include <GL/gl.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "sdk.h"
#include "config.h"
#include "ui.h"
#include "../minhook/include/MinHook.h"
#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_opengl2.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);

// ============================================================================
// CFG
// ============================================================================
// settings live in config.h so the UI layer and persistence share one source of truth.

// ============================================================================
// GLOBALS
// ============================================================================
namespace G {
    HMODULE hM=0;HWND hW=0;WNDPROC oW=0;bool imOk=false;
    void**eng=0;playermove_t*pm=0;r_studio_interface_t*si=0;
    float mv[16]={},pr[16]={};int vp[4]={};bool mOk=false;
}

#define TRAIL_N 24
struct Trail{float p[TRAIL_N][3];int h=0,c=0;DWORD t=0;};
static Trail g_tr[33];

#define BT_N 12
struct BTrace{float s[3],e[3];DWORD t;};
static BTrace g_bt[BT_N]; static int g_btH=0;

static int g_trigT=-1;static DWORD g_trigTm=0;

// ============================================================================
// ENGINE
// ============================================================================
static cl_entity_t*ELP(){return G::eng?((cl_entity_t*(__cdecl*)())G::eng[51])():0;}
static cl_entity_t*EEI(int i){return G::eng?((cl_entity_t*(__cdecl*)(int))G::eng[53])(i):0;}
static int EMC(){return G::eng?((int(__cdecl*)())G::eng[36])():32;}
static int EPI(int i,hud_player_info_t*p){return G::eng?((int(__cdecl*)(int,hud_player_info_t*))G::eng[21])(i,p):0;}
static void EPR(const char*f,...){if(!G::eng)return;va_list a;va_start(a,f);char b[512];vsnprintf(b,512,f,a);va_end(a);((void(__cdecl*)(const char*,...))G::eng[40])("%s",b);}

// ============================================================================
// MATH
// ============================================================================
static float NA(float a){while(a>180)a-=360;while(a<-180)a+=360;return a;}
static bool W2S(const float*w,float&sx,float&sy){
    if(!G::mOk)return false;
    float e[4],c[4];
    for(int i=0;i<4;i++)e[i]=G::mv[i]*w[0]+G::mv[4+i]*w[1]+G::mv[8+i]*w[2]+G::mv[12+i];
    for(int i=0;i<4;i++)c[i]=G::pr[i]*e[0]+G::pr[4+i]*e[1]+G::pr[8+i]*e[2]+G::pr[12+i]*e[3];
    if(c[3]<0.1f)return false;
    sx=(G::vp[2]*.5f)*(1+c[0]/c[3])+G::vp[0];
    sy=(G::vp[3]*.5f)*(1-c[1]/c[3])+G::vp[1];
    return true;
}
static float VD(const float*a,const float*b){float d0=a[0]-b[0],d1=a[1]-b[1],d2=a[2]-b[2];return sqrtf(d0*d0+d1*d1+d2*d2);}
static void CA(const float*s,const float*d,float*o){
    float dx=d[0]-s[0],dy=d[1]-s[1],dz=d[2]-s[2],h=sqrtf(dx*dx+dy*dy);
    o[0]=-atan2f(dz,h)*RAD2DEG;o[1]=atan2f(dy,dx)*RAD2DEG;o[2]=0;
}
static float GF(const float*v,const float*a){float dp=NA(a[0]-v[0]),dy=NA(a[1]-v[1]);return sqrtf(dp*dp+dy*dy);}
static void AV(const float*a,float*f){
    float sp=sinf(a[0]*DEG2RAD),cp=cosf(a[0]*DEG2RAD),sy=sinf(a[1]*DEG2RAD),cy=cosf(a[1]*DEG2RAD);
    f[0]=cp*cy;f[1]=cp*sy;f[2]=-sp;
}

// ============================================================================
// POINTER RESOLUTION
// ============================================================================
static bool IM(DWORD a,HMODULE m){MODULEINFO i;GetModuleInformation(GetCurrentProcess(),m,&i,sizeof(i));return a>(DWORD)m&&a<(DWORD)m+i.SizeOfImage;}
static void**FE(HMODULE c){auto p=(unsigned char*)GetProcAddress(c,"Initialize");if(!p)return 0;
    for(int i=0;i<200;i++){if(p[i]==0xBF){DWORD a=*(DWORD*)(p+i+1);if(IM(a,c))return(void**)a;}
    if(p[i]==0x68){DWORD a=*(DWORD*)(p+i+1);if(IM(a,c)){HMODULE h=GetModuleHandleA("hw.dll");if(h&&*(DWORD*)a>0&&IM(*(DWORD*)a,h))return(void**)a;}}}return 0;}
static r_studio_interface_t*FSI(HMODULE c){auto p=(unsigned char*)GetProcAddress(c,"HUD_GetStudioModelInterface");if(!p)return 0;
    for(int i=0;i<128;i++){if(p[i]==0xC7){unsigned char m=p[i+1];if((m&0xC0)==0&&m!=4&&m!=5){DWORD a=*(DWORD*)(p+i+2);if(IM(a,c))return(r_studio_interface_t*)a;}}}return 0;}

// ============================================================================
// HELPERS
// ============================================================================
static int GT(cl_entity_t*e,hud_player_info_t*i){
    int t=e->curstate.team;if(t==1||t==2)return t;
    if(i->model){if(strstr(i->model,"urban")||strstr(i->model,"gsg9")||strstr(i->model,"sas")||strstr(i->model,"gign"))return 2;
    if(strstr(i->model,"terror")||strstr(i->model,"leet")||strstr(i->model,"arctic")||strstr(i->model,"guerilla"))return 1;}return 0;}
static void Eye(float*o){if(G::pm){o[0]=G::pm->origin[0]+G::pm->view_ofs[0];o[1]=G::pm->origin[1]+G::pm->view_ofs[1];o[2]=G::pm->origin[2]+G::pm->view_ofs[2];}
    else{auto l=ELP();if(l){memcpy(o,l->curstate.origin,12);o[2]+=28;}else memset(o,0,12);}}
static void Head(cl_entity_t*e,float*o){memcpy(o,e->curstate.origin,12);o[2]+=(e->curstate.usehull==1)?12.f:28.f;}
static void Body(cl_entity_t*e,float*o){memcpy(o,e->curstate.origin,12);o[2]+=(e->curstate.usehull==1)?8.f:24.f;}
static bool IV(const float*f,const float*t){if(!G::eng||!G::eng[59])return true;
    auto tr=((pmtrace_t*(__cdecl*)(float*,float*,int,int,int))G::eng[59])((float*)f,(float*)t,0,0,-1);
    return tr&&tr->fraction>0.97f;}
// reject null / non-executable engine slots before calling through the vtable
static bool VF(void*p){
    if(!p)return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if(!VirtualQuery(p,&mbi,sizeof(mbi))||mbi.State!=MEM_COMMIT)return false;
    DWORD pr=mbi.Protect&0xFF;
    return pr==PAGE_EXECUTE||pr==PAGE_EXECUTE_READ||pr==PAGE_EXECUTE_READWRITE||pr==PAGE_EXECUTE_WRITECOPY;
}

// ============================================================================
// CL_CreateMove
// ============================================================================
typedef void(__cdecl*CLCM_t)(float,usercmd_t*,int);static CLCM_t oCLCM=0;
static void __cdecl hkCLCM(float ft,usercmd_t*cmd,int act){
    oCLCM(ft,cmd,act);if(!cmd||!act)return;
    auto lp=ELP();if(!lp)return;int lt=lp->curstate.team;

    // bhop
    if(C::bhop&&(cmd->buttons&IN_JUMP)&&G::pm&&!(G::pm->flags&FL_ONGROUND))
        cmd->buttons&=~IN_JUMP;
    // auto-strafe
    if(C::autoStrafe&&G::pm&&!(G::pm->flags&FL_ONGROUND)&&(cmd->buttons&IN_JUMP)){
        if(cmd->sidemove==0){
            if(cmd->viewangles[1]>G::pm->angles[1])cmd->sidemove=400;
            else if(cmd->viewangles[1]<G::pm->angles[1])cmd->sidemove=-400;
        }
    }

    // RCS (named engine field, no hardcoded offset)
    if(C::rcs&&G::pm){
        cmd->viewangles[0]-=G::pm->punchangle[0]*C::rcsF;
        cmd->viewangles[1]-=G::pm->punchangle[1]*C::rcsF;
        cmd->viewangles[2]-=G::pm->punchangle[2]*C::rcsF;
    }

    // no flash / damage flash - clear the engine's active screenfade every tick
    if((C::noFlash||C::dmgFlash)&&G::eng&&VF(G::eng[75])&&VF(G::eng[76])){
        screenfade_t sf{};((void(__cdecl*)(screenfade_t*))G::eng[75])(&sf);
        bool damage=(sf.fadeFlags&0x0001)&&sf.fader>120&&sf.fadeg<110&&sf.fadeb<110;
        if(sf.fadealpha>0&&((C::noFlash&&(sf.fadeFlags&0x0001))||(C::dmgFlash&&damage))){
            sf.fadeSpeed=0;sf.fadeEnd=0;sf.fadeTotalEnd=0;sf.fadeReset=0;sf.fadealpha=0;
            ((void(__cdecl*)(screenfade_t*))G::eng[76])(&sf);
        }
    }

    // aim + trigger scan
    float eye[3];Eye(eye);
    int mc=EMC();if(mc>32)mc=32;
    int bIdx=-1;float bFov=999,bAng[3]={};
    bool trigHit=false;

    for(int i=1;i<=mc;i++){
        if(i==lp->index)continue;
        auto e=EEI(i);if(!e||!e->player||e->curstate.movetype==MOVETYPE_NONE)continue;
        // relaxed messagenum check for longer range ESP/aim
        if(e->curstate.messagenum<lp->curstate.messagenum-2)continue;
        hud_player_info_t pi={};EPI(i,&pi);if(!pi.name||!pi.name[0])continue;
        int team=GT(e,&pi);if(team==lt&&team!=0)continue;

        float tgt[3];
        if(C::aimBone==0)Head(e,tgt);else Body(e,tgt);
        float ang[3];CA(eye,tgt,ang);
        float fov=GF(cmd->viewangles,ang);

        // aimbot
        if(C::aim&&fov<C::aimFov&&fov<bFov){
            if(!C::aimVis||IV(eye,tgt)){bFov=fov;bIdx=i;memcpy(bAng,ang,12);}
        }
        // triggerbot — "Any" picks whichever of head/body is closer to the crosshair
        float trigTgt[3], trigAng[3];
        if(C::trigPart==0){
            float hd[3],bd[3],ha[3],ba[3];
            Head(e,hd);Body(e,bd);CA(eye,hd,ha);CA(eye,bd,ba);
            if(GF(cmd->viewangles,ba)<GF(cmd->viewangles,ha)){memcpy(trigTgt,bd,12);memcpy(trigAng,ba,12);}
            else{memcpy(trigTgt,hd,12);memcpy(trigAng,ha,12);}
        } else {
            if(C::trigPart==2)Body(e,trigTgt); else Head(e,trigTgt);
            CA(eye,trigTgt,trigAng);
        }

        float trigFov=GF(cmd->viewangles,trigAng);
        if(C::trig&&trigFov<C::trigFov){
            if(!C::aimVis||IV(eye,trigTgt)){
                trigHit=true;
                if(i!=g_trigT){g_trigT=i;g_trigTm=GetTickCount();}
                if((int)(GetTickCount()-g_trigTm)>=C::trigDelay)cmd->buttons|=IN_ATTACK;
            }
        }
    }
    // reset the trigger timer only when nothing is actually under the crosshair
    if(!C::trig||!trigHit)g_trigT=-1;

    // apply aim
    if(C::aim&&bIdx>0){
        bool key=(C::aimKey==0)||(GetAsyncKeyState(C::aimKey)&0x8000);
        if(key){
            if(C::silent){
                // silent aim: only rewrite angles on attack ticks, then restore the visual view
                if(cmd->buttons&IN_ATTACK){
                    float orig[3];memcpy(orig,cmd->viewangles,12);
                    cmd->viewangles[0]=bAng[0];cmd->viewangles[1]=bAng[1];cmd->viewangles[2]=0;
                    if(G::eng&&G::eng[35])((void(__cdecl*)(float*))G::eng[35])(orig);
                }
            } else {
                if(C::aimSmooth<=1.01f){cmd->viewangles[0]=bAng[0];cmd->viewangles[1]=bAng[1];}
                else{cmd->viewangles[0]+=NA(bAng[0]-cmd->viewangles[0])/C::aimSmooth;
                     cmd->viewangles[1]+=NA(bAng[1]-cmd->viewangles[1])/C::aimSmooth;}
            }
        }
    }

    // auto pistol: pulse the attack button so semi-autos keep firing
    if(C::autoPistol){
        static bool ap=false;
        if(cmd->buttons&IN_ATTACK){ap=!ap;if(ap)cmd->buttons&=~IN_ATTACK;}
        else ap=false;
    }

    // bullet trace capture
    if(C::btrace&&(cmd->buttons&IN_ATTACK)&&G::eng&&G::eng[59]){
        float fwd[3];AV(cmd->viewangles,fwd);
        float end[3]={eye[0]+fwd[0]*8192,eye[1]+fwd[1]*8192,eye[2]+fwd[2]*8192};
        auto tr=((pmtrace_t*(__cdecl*)(float*,float*,int,int,int))G::eng[59])(eye,end,0,0,-1);
        if(tr){auto&b=g_bt[g_btH];memcpy(b.s,eye,12);memcpy(b.e,tr->endpos,12);b.t=GetTickCount();g_btH=(g_btH+1)%BT_N;}
    }
}

// ============================================================================
// HUD_PlayerMove
// ============================================================================
typedef void(__cdecl*HPM_t)(playermove_t*,int);static HPM_t oHPM=0;
static void __cdecl hkHPM(playermove_t*p,int s){G::pm=p;oHPM(p,s);}

// ============================================================================
// StudioDrawModel (viewmodel)
// ============================================================================
typedef int(__cdecl*SDM_t)(int);static SDM_t oSDM=0;
static int __cdecl hkSDM(int f){
    // Avoid a GL state query on the common path where no local appearance
    // effect is enabled.
    const bool hasOffset = C::vmOfs[0]!=0.f || C::vmOfs[1]!=0.f || C::vmOfs[2]!=0.f;
    if(!C::hand && !hasOffset) return oSDM(f);

    // GoldSrc renders the local weapon/hands in a separate depth range.
    // Keep every material pass confined to that local viewmodel path.
    GLfloat dr[2]={0.f,1.f};
    glGetFloatv(GL_DEPTH_RANGE,dr);
    const bool isViewModel = dr[1] < 0.5f;
    bool pushed=false;

    if(isViewModel && hasOffset){
        glPushMatrix();
        glTranslatef(C::vmOfs[0],C::vmOfs[1],C::vmOfs[2]);
        pushed=true;
    }

    if(!isViewModel || !C::hand){
        const int r=oSDM(f);
        if(pushed)glPopMatrix();
        return r;
    }

    // Fixed-function lighting can override glColor. Disable it for the tint;
    // GL_ALL_ATTRIB_BITS restores the game's state after these local passes.
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);

    const float alpha=std::max(0.f,std::min(1.f,C::handAlpha));
    const float pulse=(C::handM==3)
        ? (.72f + .28f*(.5f + .5f*sinf((float)GetTickCount()*.0032f)))
        : 1.f;
    int result=0;

    if(C::handM==0){
        // Clean matte / flat material.
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        glColor4f(C::handC[0],C::handC[1],C::handC[2],alpha);
        result=oSDM(f);
    }else if(C::handM==1){
        // Crisp wireframe.
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
        glLineWidth(C::handLineWidth);
        glColor4f(C::handC[0],C::handC[1],C::handC[2],alpha);
        result=oSDM(f);
    }else{
        // Glass and Pulse: translucent body + broad additive edges +
        // a narrow, brighter wire pass. Pulse animates only the local accent.
        // Let the translucent base establish the model depth, then draw
        // the luminous edge at equal depth without writing it again.
        glDepthMask(GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(C::handC[0],C::handC[1],C::handC[2],alpha*(C::handM==2?.20f:.15f));
        result=oSDM(f);

        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE);
        glLineWidth(std::min(5.f,C::handLineWidth*2.4f));
        glColor4f(C::handC[0],C::handC[1],C::handC[2],alpha*.28f*pulse);
        result=oSDM(f);

        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(C::handLineWidth);
        glColor4f(C::handC[0],C::handC[1],C::handC[2],alpha*.92f*pulse);
        result=oSDM(f);
    }

    glPopAttrib();
    if(pushed)glPopMatrix();
    return result;
}

// ============================================================================
// StudioDrawPlayer (chams)
// ============================================================================
typedef int(__cdecl*SDP_t)(int,entity_state_t*);static SDP_t oSDP=0;
static int __cdecl hkSDP(int f,entity_state_t*pp){
    glGetFloatv(GL_MODELVIEW_MATRIX,G::mv);
    glGetFloatv(GL_PROJECTION_MATRIX,G::pr);
    glGetIntegerv(GL_VIEWPORT,G::vp);G::mOk=true;
    if(C::chMode==0||!pp)return oSDP(f,pp);
    bool isT=(pp->team==TEAM_TERRORIST);
    const float*wc=isT?C::chTW:C::chCW,*vc=isT?C::chTV:C::chCV;

    // rainbow override
    float rb[3];
    if(C::chMode==4){
        float h=fmodf(GetTickCount()/1500.f+(isT?0:.5f),1.f);
        ImVec4 c=ImColor::HSV(h,1,1);rb[0]=c.x;rb[1]=c.y;rb[2]=c.z;
        wc=vc=rb;
    }

    // wall pass
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glDisable(GL_DEPTH_TEST);glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    if(C::chMode==3||C::chMode==4){glBlendFunc(GL_SRC_ALPHA,GL_ONE);glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);glLineWidth(3);}
    else{glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);}
    if(C::chMode==2){glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);glLineWidth(1.5f);}
    glColor4f(wc[0],wc[1],wc[2],(C::chMode>=3?.45f:.55f)*C::chamsAlpha);
    oSDP(f,pp);
    glPopAttrib();

    // visible pass
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glEnable(GL_DEPTH_TEST);glDisable(GL_TEXTURE_2D);
    if(C::chMode==2){glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);glLineWidth(1.5f);}
    if(C::chMode>=3){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);}
    else if(C::chamsAlpha<.999f){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);}
    glColor4f(vc[0],vc[1],vc[2],(C::chMode>=3?.85f:1.f)*C::chamsAlpha);
    int r=oSDP(f,pp);
    glPopAttrib();
    return r;
}

// ============================================================================
// ESP + VISUALS
// ============================================================================
static void DrawESP(){
    if(!G::eng||!G::mOk)return;
    auto lp=ELP();if(!lp)return;
    int mc=EMC();if(mc>32)mc=32;
    int lt=lp->curstate.team;
    auto*dl=ImGui::GetBackgroundDrawList();
    float cx=G::vp[2]*.5f,cy=G::vp[3]*.5f;

    // darken the world first so ESP / crosshair stay readable on top
    if(C::nightMode)
        dl->AddRectFilled(ImVec2(0,0),ImVec2((float)G::vp[2],(float)G::vp[3]),IM_COL32(0,0,15,(int)(C::nightAlpha*255)));

    if(C::esp) for(int i=1;i<=mc;i++){
        if(i==lp->index)continue;
        auto e=EEI(i);if(!e||!e->player||e->curstate.movetype==MOVETYPE_NONE)continue;
        if(e->curstate.messagenum<lp->curstate.messagenum-2)continue;
        hud_player_info_t pi={};EPI(i,&pi);if(!pi.name||!pi.name[0])continue;
        int team=GT(e,&pi);
        if(C::espIgnoreTeam&&team==lt&&team!=0)continue;
        float dm=VD(lp->curstate.origin,e->curstate.origin)/40.f;
        if(C::espMaxDist>0.f&&dm>C::espMaxDist)continue;
        float*cc=(team==1)?C::ecT:(team==2?C::ecCT:nullptr);
        ImU32 col=cc?IM_COL32((int)(cc[0]*255),(int)(cc[1]*255),(int)(cc[2]*255),255)
                    :IM_COL32(205,210,220,255);
        col=with_a(col,C::espAlpha);
        float*org=e->curstate.origin;
        float mn=e->curstate.mins[2],mx=e->curstate.maxs[2];
        if(mn==0&&mx==0){bool d=(e->curstate.usehull==1);mn=d?-18.f:-36.f;mx=d?18.f:36.f;}
        float ft[3]={org[0],org[1],org[2]+mn},hd[3]={org[0],org[1],org[2]+mx};
        float fS[2],hS[2];
        if(!W2S(ft,fS[0],fS[1]))continue;if(!W2S(hd,hS[0],hS[1]))continue;
        float h=fS[1]-hS[1];if(h<4)continue;float w=h*.38f;
        float x1=hS[0]-w*.5f,y1=hS[1],x2=hS[0]+w*.5f,y2=fS[1];
        ImU32 bk=IM_COL32(0,0,0,180);

        if(C::espBox){
            if(C::espGlow){
                for(int g=3;g>=1;g--)
                    dl->AddRect(ImVec2(x1-g*2.f,y1-g*2.f),ImVec2(x2+g*2.f,y2+g*2.f),
                                scale_a(col,.045f*g),0.f,0,1.f);
            }
            if(C::espStyle==0){ // full box
                dl->AddRect(ImVec2(x1-1,y1-1),ImVec2(x2+1,y2+1),scale_a(bk,C::espAlpha),0,0,2.f);
                dl->AddRect(ImVec2(x1,y1),ImVec2(x2,y2),col,0,0,1.4f);
            } else { // corner box
                float cw=w*.25f,ch=h*.2f;
                auto corner=[&](float ax,float ay,float bx,float by){
                    dl->AddLine(ImVec2(ax,ay),ImVec2(ax+(bx-ax>0?cw:-cw),ay),col,2);
                    dl->AddLine(ImVec2(ax,ay),ImVec2(ax,ay+(by-ay>0?ch:-ch)),col,2);
                };
                corner(x1,y1,x2,y2);corner(x2,y1,x1,y2);corner(x1,y2,x2,y1);corner(x2,y2,x1,y1);
            }
        }
        if(C::espName){
            auto ts=ImGui::CalcTextSize(pi.name);
            ImVec2 tp(hS[0]-ts.x*.5f,y1-ts.y-3);
            dl->AddText(ImVec2(tp.x+1,tp.y+1),IM_COL32(0,0,0,190),pi.name);
            dl->AddText(tp,col,pi.name);
        }
        if(C::espDist){
            char db[32];snprintf(db,32,"%.0f m",dm);
            auto ts=ImGui::CalcTextSize(db);
            ImVec2 dp(hS[0]-ts.x*.5f-5,y2+4);
            dl->AddRectFilled(dp,ImVec2(dp.x+ts.x+10,dp.y+ts.y+2),IM_COL32(8,10,14,170),3);
            dl->AddText(ImVec2(dp.x+5,dp.y+1),IM_COL32(215,220,230,(int)(230*C::espAlpha)),db);
        }
        if(C::espSnap)dl->AddLine(ImVec2(cx,(float)G::vp[3]),ImVec2(fS[0],fS[1]),ui::with_a(col,.30f),1.f);

        // particle trail
        if(C::espTrail){
            auto&t=g_tr[i];DWORD now=GetTickCount();
            if(now-t.t>35){t.h=(t.h+1)%TRAIL_N;memcpy(t.p[t.h],org,12);if(t.c<TRAIL_N)t.c++;t.t=now;}
            for(int j=0;j<t.c;j++){
                int ci=(t.h-j+TRAIL_N)%TRAIL_N;
                float sx,sy;if(!W2S(t.p[ci],sx,sy))continue;
                float al=1.f-(float)j/t.c;
                float ox=sinf(j*1.7f+ci*.3f)*(1-al)*18,oy=cosf(j*2.3f+ci*.5f)*(1-al)*18;
                float sz=2.5f*al;
                ImU32 tc=IM_COL32((int)(C::trailCol[0]*al*255),(int)(C::trailCol[1]*al*255),(int)(C::trailCol[2]*al*255),(int)(al*160));
                dl->AddCircleFilled(ImVec2(sx+ox,sy+oy),sz,tc);
            }
        }
    }

    // 2d radar - forward-up, monochrome chrome with team dots
    if(C::radar&&G::pm){
        float r=C::radarSize; if(r<40.f)r=40.f;
        ImVec2 c((float)G::vp[2]-r-30.f,(float)G::vp[3]-r-30.f);
        dl->AddCircleFilled(c,r+10.f,IM_COL32(6,7,10,150),72);
        dl->AddCircle(c,r+10.f,IM_COL32(255,255,255,26),72,1.f);
        dl->AddCircle(c,r*.66f,IM_COL32(255,255,255,10),72,1.f);
        dl->AddCircle(c,r*.33f,IM_COL32(255,255,255,10),72,1.f);
        dl->AddLine(ImVec2(c.x-r,c.y),ImVec2(c.x+r,c.y),IM_COL32(255,255,255,9),1.f);
        dl->AddLine(ImVec2(c.x,c.y-r),ImVec2(c.x,c.y+r),IM_COL32(255,255,255,9),1.f);

        float yaw=G::pm->angles[1]*DEG2RAD;
        float cyw=cosf(yaw),syw=sinf(yaw);
        float range=C::radarRange>1.f?C::radarRange:1.f;
        for(int i=1;i<=mc;i++){
            if(i==lp->index)continue;
            auto e=EEI(i);if(!e||!e->player||e->curstate.movetype==MOVETYPE_NONE)continue;
            if(e->curstate.messagenum<lp->curstate.messagenum-2)continue;
            hud_player_info_t pi={};EPI(i,&pi);if(!pi.name||!pi.name[0])continue;
            int team=GT(e,&pi);
            if(C::radarEnemies&&team==lt&&team!=0)continue;
            float dx=e->curstate.origin[0]-lp->curstate.origin[0];
            float dy=e->curstate.origin[1]-lp->curstate.origin[1];
            float px=dx*syw-dy*cyw;
            float py=-(dx*cyw+dy*syw);
            px=px/range*r; py=py/range*r;
            float dd=sqrtf(px*px+py*py);
            bool out=dd>r-5.f;
            if(out&&dd>0.f){px*=(r-5.f)/dd;py*=(r-5.f)/dd;}
            float*cc=(team==1)?C::ecT:(team==2?C::ecCT:nullptr);
            ImU32 dc=cc?IM_COL32((int)(cc[0]*255),(int)(cc[1]*255),(int)(cc[2]*255),out?110:255)
                       :IM_COL32(210,214,222,out?110:255);
            if(!out)dl->AddCircle(c+ImVec2(px,py),5.5f,with_a(dc,.18f),12,1.f);
            dl->AddCircleFilled(c+ImVec2(px,py),out?2.2f:3.4f,dc);
        }
        // local player arrow (always up)
        dl->AddTriangleFilled(c+ImVec2(0,-7),c+ImVec2(-5,5),c+ImVec2(5,5),IM_COL32(240,243,248,250));
        dl->AddCircleFilled(c+ImVec2(0,1),1.8f,IM_COL32(10,11,14,255));
    }

    // bullet traces
    if(C::btrace){DWORD now=GetTickCount();
        for(int i=0;i<BT_N;i++){auto&b=g_bt[i];if(!b.t)continue;
            float el=(now-b.t)/1000.f;if(el>1.5f){b.t=0;continue;}
            float al=1-el/1.5f;float s1[2],s2[2];
            if(!W2S(b.s,s1[0],s1[1]))continue;if(!W2S(b.e,s2[0],s2[1]))continue;
            dl->AddLine(ImVec2(s1[0],s1[1]),ImVec2(s2[0],s2[1]),
                IM_COL32((int)(C::btCol[0]*255),(int)(C::btCol[1]*255),(int)(C::btCol[2]*255),(int)(al*180)),1.5f);
        }
    }

    // custom crosshair — optional dark outline keeps the reticle readable
    // without changing aim or target selection.
    if(C::xhair){
        const float alpha=std::max(0.f,std::min(1.f,C::xhCol[3]));
        ImU32 xc=IM_COL32((int)(C::xhCol[0]*255),(int)(C::xhCol[1]*255),
                          (int)(C::xhCol[2]*255),(int)(alpha*255));
        ImU32 oc=IM_COL32(5,6,8,(int)(alpha*210));
        const float g=C::xhGap, len=C::xhSize, thick=C::xhThick;
        auto stroke=[&](ImVec2 a,ImVec2 b){
            if(C::xhOutline) dl->AddLine(a,b,oc,thick+2.2f);
            dl->AddLine(a,b,xc,thick);
        };
        stroke(ImVec2(cx-g-len,cy),ImVec2(cx-g,cy));
        stroke(ImVec2(cx+g,cy),ImVec2(cx+g+len,cy));
        stroke(ImVec2(cx,cy-g-len),ImVec2(cx,cy-g));
        stroke(ImVec2(cx,cy+g),ImVec2(cx,cy+g+len));
        if(C::xhDot){
            if(C::xhOutline) dl->AddCircleFilled(ImVec2(cx,cy),3.2f,oc,16);
            dl->AddCircleFilled(ImVec2(cx,cy),std::max(1.2f,thick*.72f),xc,16);
        }
    }

    // scope overlay: clean + with fading arms
    if(C::scopeOvr&&lp->curstate.fov>0&&lp->curstate.fov<90){
        ImU32 sc=IM_COL32(255,255,255,220);
        dl->AddCircleFilled(ImVec2(cx,cy),2,sc); // center dot
        for(float r=0;r<300;r+=1){
            float al=1.f-r/300.f;
            ImU32 c=IM_COL32(255,255,255,(int)(al*al*180));
            dl->AddLine(ImVec2(cx-r,cy),ImVec2(cx-r-1,cy),c);
            dl->AddLine(ImVec2(cx+r,cy),ImVec2(cx+r+1,cy),c);
            dl->AddLine(ImVec2(cx,cy-r),ImVec2(cx,cy-r-1),c);
            dl->AddLine(ImVec2(cx,cy+r),ImVec2(cx,cy+r+1),c);
        }
    }

    // FOV circle
    if(C::aim&&C::aimFov>0&&G::pr[0]!=0){
        float hf=2.f*atanf(1.f/G::pr[0])*RAD2DEG;
        float px=C::aimFov*((float)G::vp[2]/hf);
        dl->AddCircle(ImVec2(cx,cy),px,IM_COL32(255,255,255,30),64);
    }
}

// ============================================================================
// WATERMARK
// ============================================================================
static void DrawWatermark(){
    if(!C::watermark)return;
    int mc=EMC();
    char meta[96];
    snprintf(meta,sizeof(meta),"%d players   %.0f fps",mc,ImGui::GetIO().Framerate);
    ImDrawList* fg=ImGui::GetForegroundDrawList();
    ImFont* fb=ui::f_bold?ui::f_bold:ImGui::GetFont();
    ImFont* fs=ui::f_small?ui::f_small:ImGui::GetFont();
    float w1=fb->CalcTextSizeA(13.f,FLT_MAX,0.f,"TOKER").x;
    float w2=fs->CalcTextSizeA(9.5f,FLT_MAX,0.f,meta).x;
    float w=std::max(w1,w2)+26.f;
    ImVec2 p(18.f,18.f);
    fg->AddRectFilled(p,ImVec2(p.x+w,p.y+38.f),IM_COL32(8,10,14,185),7.f);
    fg->AddRectFilled(p,ImVec2(p.x+2.5f,p.y+38.f),ui::ACCENT_A,2.f);
    fg->AddText(fb,13.f,ImVec2(p.x+13.f,p.y+7.f),ui::TEXT,"TOKER");
    fg->AddText(fs,9.5f,ImVec2(p.x+13.f,p.y+23.f),ui::DIM2,meta);
}

// ============================================================================
// MENU -> src/ui.h  (ui::draw)
// ============================================================================

// ============================================================================
// WNDPROC + SWAPBUFFERS
// ============================================================================
static LRESULT CALLBACK HW(HWND h,UINT m,WPARAM w,LPARAM l){
    if(C::menu&&ImGui_ImplWin32_WndProcHandler(h,m,w,l))return 1;
    return CallWindowProcW(G::oW,h,m,w,l);}
typedef BOOL(APIENTRY*SB_t)(HDC);static SB_t oSB=0;
static void IG(HDC hdc){
    G::hW=WindowFromDC(hdc);if(!G::hW)return;
    IMGUI_CHECKVERSION();ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=0;
    ui::load_fonts();ui::apply_style();
    ImGui_ImplWin32_Init(G::hW);ImGui_ImplOpenGL2_Init();
    G::oW=(WNDPROC)SetWindowLongPtrW(G::hW,GWLP_WNDPROC,(LONG_PTR)HW);G::imOk=true;}
static BOOL APIENTRY hkSB(HDC hdc){
    if(!G::imOk)IG(hdc);if(!G::imOk)return oSB(hdc);

    // menu hotkey: polled every frame, works even when the engine eats WndProc keys
    {
        static bool prev=false;
        int k=C::menuKey;
        bool down=(k>0&&k<256)&&((GetAsyncKeyState(k)&0x8000)!=0);
        if(ui::bind_armed()){prev=down;}
        else{
            if(down&&!prev)C::menu=!C::menu;
            prev=down;
        }
    }

    // frost the scene behind the menu (back buffer is still intact here)
    if(C::menu&&C::glass)ui::capture_backdrop();

    // GoldSrc re-clips the cursor every frame; unclip while the menu owns the mouse
    static bool cursor_shown=false;
    if(C::menu&&!cursor_shown){ShowCursor(TRUE);cursor_shown=true;}
    else if(!C::menu&&cursor_shown){ShowCursor(FALSE);cursor_shown=false;}
    if(C::menu){ClipCursor(nullptr);SetCursor(LoadCursorA(nullptr,IDC_ARROW));}

    glPushAttrib(GL_FOG_BIT);glDisable(GL_FOG);
    ImGui_ImplOpenGL2_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
    DrawESP();DrawWatermark();

    ui::State st{};
    st.eng=G::eng!=0;st.pm=G::pm!=0;st.si=G::si!=0;
    st.fps=ImGui::GetIO().Framerate;st.players=EMC();
    st.eng_addr=(unsigned)(uintptr_t)G::eng;
    st.pm_addr=(unsigned)(uintptr_t)G::pm;
    st.si_addr=(unsigned)(uintptr_t)G::si;
    if(G::pm){
        st.flags=G::pm->flags;st.on_ground=(G::pm->flags&FL_ONGROUND)!=0;
        st.punch[0]=G::pm->punchangle[0];
        st.punch[1]=G::pm->punchangle[1];
        st.punch[2]=G::pm->punchangle[2];
        st.vel[0]=G::pm->velocity[0];st.vel[1]=G::pm->velocity[1];st.vel[2]=G::pm->velocity[2];
        st.speed=sqrtf(st.vel[0]*st.vel[0]+st.vel[1]*st.vel[1]);
    }
    st.build="2.0";
    ui::draw(st,&C::menu);

    ImGui::Render();
    glPushAttrib(GL_ALL_ATTRIB_BITS);glPushMatrix();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    glPopMatrix();glPopAttrib();glPopAttrib();
    BOOL r=oSB(hdc);
    if(C::fogOn){
        glEnable(GL_FOG);
        const GLenum fogMode = C::fogMode==1 ? GL_EXP : (C::fogMode==2 ? GL_EXP2 : GL_LINEAR);
        glFogi(GL_FOG_MODE,(GLint)fogMode);
        glFogf(GL_FOG_DENSITY,C::fogDensity);
        glFogf(GL_FOG_START,C::fogS);
        glFogf(GL_FOG_END,C::fogE);
        float fc[4]={C::fogC[0],C::fogC[1],C::fogC[2],1};
        glFogfv(GL_FOG_COLOR,fc);
    }
    else glDisable(GL_FOG);
    return r;}

// ============================================================================
// INIT / SHUTDOWN
// ============================================================================
static void SD(){
    if(G::hW&&G::oW){SetWindowLongPtrW(G::hW,GWLP_WNDPROC,(LONG_PTR)G::oW);G::oW=0;}
    if(G::si){DWORD o;
        if(oSDP){VirtualProtect(&G::si->StudioDrawPlayer,4,PAGE_READWRITE,&o);G::si->StudioDrawPlayer=oSDP;VirtualProtect(&G::si->StudioDrawPlayer,4,o,&o);}
        if(oSDM){VirtualProtect(&G::si->StudioDrawModel,4,PAGE_READWRITE,&o);G::si->StudioDrawModel=oSDM;VirtualProtect(&G::si->StudioDrawModel,4,o,&o);}}
    if(G::imOk){ImGui_ImplOpenGL2_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();G::imOk=false;}
    MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();glDisable(GL_FOG);}

static DWORD WINAPI IT(LPVOID lp){
    G::hM=(HMODULE)lp;HMODULE hw=0,cl=0;
    cfg::load();
    while(!hw||!cl){hw=GetModuleHandleA("hw.dll");cl=GetModuleHandleA("client.dll");Sleep(200);}
    Sleep(2500);
    G::eng=FE(cl);MH_Initialize();
    void*p;
    p=(void*)GetProcAddress(cl,"CL_CreateMove");
    if(p){MH_CreateHook(p,(LPVOID)hkCLCM,(LPVOID*)&oCLCM);MH_EnableHook(p);}
    p=(void*)GetProcAddress(cl,"HUD_PlayerMove");
    if(p){MH_CreateHook(p,(LPVOID)hkHPM,(LPVOID*)&oHPM);MH_EnableHook(p);}
    G::si=FSI(cl);
    if(G::si){DWORD o;
        if(G::si->StudioDrawPlayer){oSDP=G::si->StudioDrawPlayer;VirtualProtect(&G::si->StudioDrawPlayer,4,PAGE_READWRITE,&o);G::si->StudioDrawPlayer=hkSDP;VirtualProtect(&G::si->StudioDrawPlayer,4,o,&o);}
        if(G::si->StudioDrawModel){oSDM=G::si->StudioDrawModel;VirtualProtect(&G::si->StudioDrawModel,4,PAGE_READWRITE,&o);G::si->StudioDrawModel=hkSDM;VirtualProtect(&G::si->StudioDrawModel,4,o,&o);}}
    HMODULE gl=GetModuleHandleA("opengl32.dll");
    while(!gl){Sleep(100);gl=GetModuleHandleA("opengl32.dll");}
    p=(void*)GetProcAddress(gl,"wglSwapBuffers");
    if(p){MH_CreateHook(p,(LPVOID)hkSB,(LPVOID*)&oSB);MH_EnableHook(p);}
    if(G::eng){
        EPR("[Toker] loaded eng:%p si:%p\n",G::eng,G::si);
        EPR("[Toker] CLCM:%s HPM:%s\n",oCLCM?"OK":"MISS",oHPM?"OK":"MISS");
    }
    return 0;}

BOOL WINAPI DllMain(HINSTANCE h,DWORD r,LPVOID){
    if(r==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(h);CreateThread(0,0,IT,h,0,0);}
    return TRUE;}
