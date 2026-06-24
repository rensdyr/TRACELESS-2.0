#include "gui.hpp"
#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <imgui.h>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <deque>

namespace FrameWork {

// ========================= WINDOW SUBCLASS =========================

static HWND     g_hWindow        = nullptr;
static WNDPROC  g_OriginalWndProc = nullptr;
static bool     g_bMenuOpen      = true;

// When menu is closed, return HTTRANSPARENT so all clicks pass through to the game/desktop.
extern "C" LRESULT CALLBACK SubclassedWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCHITTEST && !g_bMenuOpen)
        return HTTRANSPARENT;
    return CallWindowProc(g_OriginalWndProc, hWnd, msg, wParam, lParam);
}

void SetupWindowSubclass(HWND hWnd) {
    if (!g_hWindow) {
        g_hWindow        = hWnd;
        g_OriginalWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)SubclassedWindowProc);
    }
}

namespace {

// ========================= STATE =========================

struct ToastMsg { std::string text; double created; float life; ImU32 col; float anim; };

std::map<std::string, bool>   section_open;
std::map<std::string, float>  section_anim;
std::map<std::string, float>  chk_anim;
std::map<std::string, float>  tog_anim;
std::map<std::string, float>  sld_hover_anim;
std::map<std::string, float>  hdr_hover_anim;
std::map<std::string, float>  kb_press_anim;
std::map<std::string, float>  cmb_hover_anim;
std::map<std::string, float>  btn_hover_anim;
std::map<std::string, float>  nav_hover_anim;
std::map<std::string, bool>   chk_prev_val;
std::map<std::string, double> chk_last_change_time;
std::map<std::string, int>    sld_last_val;
std::map<std::string, double> sld_last_change_time;
std::deque<ToastMsg>          toasts;

int   active_tab      = 0;
int   prev_active_tab = 0;
float tab_content_anim = 1.0f;

constexpr int FPS_HIST_SIZE = 90;
float fps_history[FPS_HIST_SIZE] = {0};
int   fps_history_idx = 0;
float fps_smoothed    = 0.0f;
float nav_pill_y      = 0.0f;
float nav_pill_h      = 0.0f;
bool  nav_pill_init   = false;
float nav_pill_alpha  = 0.0f;
float gui_fade_anim   = 0.0f;

bool  key_listening        = false;
int*  listening_key        = nullptr;
bool  listening_wait_release = false;

float menu_scale  = 1.0f;
bool  should_unload = false;
bool  pause_game  = false;
bool  dragging    = false;
ImVec2 drag_offset(0, 0);
bool  menu_visible = true;

// ========================= PALETTE =========================
// Static colours (never change)
const ImU32 C_BG_WIN       = IM_COL32(10,  11,  14,  245);
const ImU32 C_BG_SIDEBAR   = IM_COL32(14,  15,  19,  252);
const ImU32 C_BG_TOPBAR    = IM_COL32(13,  14,  18,  252);
const ImU32 C_BG_CONTENT   = IM_COL32(16,  17,  22,  240);
const ImU32 C_BG_CARD      = IM_COL32(22,  24,  30,  240);
const ImU32 C_BG_CARD_HOV  = IM_COL32(28,  30,  37,  248);
const ImU32 C_BG_CARD_BOT  = IM_COL32(18,  19,  24,  240);
const ImU32 C_BG_INPUT     = IM_COL32(15,  16,  21,  230);
const ImU32 C_BG_INPUT_HOV = IM_COL32(20,  22,  28,  240);
const ImU32 C_BG_PILL      = IM_COL32(32,  34,  41,  200);
const ImU32 C_BORDER       = IM_COL32(48,  51,  60,  100);
const ImU32 C_BORDER_SOFT  = IM_COL32(38,  40,  48,   60);
const ImU32 C_BORDER_BR    = IM_COL32(72,  76,  88,  130);
const ImU32 C_TEXT         = IM_COL32(210, 213, 220, 240);
const ImU32 C_TEXT_DIM     = IM_COL32(130, 134, 144, 220);
const ImU32 C_TEXT_HINT    = IM_COL32(92,  96,  107, 200);
const ImU32 C_TEXT_BRIGHT  = IM_COL32(238, 240, 245, 252);
const ImU32 C_WHITE        = IM_COL32(245, 246, 250, 255);
const ImU32 C_SUCCESS      = IM_COL32(85,  200, 140, 255);
const ImU32 C_WARNING      = IM_COL32(245, 185, 95,  255);

// Dynamic accent (updated each frame from g_Options.General.AccentColor)
ImU32 C_ACCENT       = IM_COL32(225, 70, 75, 255);
ImU32 C_ACCENT_DIM   = IM_COL32(180, 50, 55, 220);
ImU32 C_ACCENT_LIGHT = IM_COL32(240,105,110, 220);
ImU32 C_ACCENT_GLOW  = IM_COL32(225, 70, 75,  90);

// ========================= MATH =========================

float Clamp01(float t) { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }

float SmoothLerp(float cur, float tgt, float speed, float dt) {
    if (dt > 0.1f) dt = 0.1f;
    return cur + (tgt - cur) * Clamp01(1.f - expf(-speed * dt));
}
float EaseOutCubic(float t)  { t = Clamp01(t); float f = t-1.f; return f*f*f+1.f; }
float EaseOutQuart(float t)  { t = Clamp01(t); float f = t-1.f; return 1.f-f*f*f*f; }
float EaseInOutCubic(float t){
    t = Clamp01(t);
    if (t < 0.5f) return 4*t*t*t;
    float f = -2*t+2; return 1.f-(f*f*f)*0.5f;
}
float EaseOutBack(float t) {
    t = Clamp01(t); const float c1=1.70158f, c3=c1+1.f;
    float f=t-1.f; return 1.f+c3*f*f*f+c1*f*f;
}

ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    t = Clamp01(t);
    int ra=a&0xFF, ga=(a>>8)&0xFF, ba2=(a>>16)&0xFF, aa=(a>>24)&0xFF;
    int rb=b&0xFF, gb=(b>>8)&0xFF, bb=(b>>16)&0xFF, ab=(b>>24)&0xFF;
    return IM_COL32((int)(ra+(rb-ra)*t+.5f),(int)(ga+(gb-ga)*t+.5f),
                    (int)(ba2+(bb-ba2)*t+.5f),(int)(aa+(ab-aa)*t+.5f));
}
ImU32 ScaleAlpha(ImU32 c, float s) {
    int a=(int)(((c>>24)&0xFF)*Clamp01(s)+.5f);
    return (c&0x00FFFFFF)|((ImU32)a<<24);
}
float& AnimState(std::map<std::string,float>& m, const std::string& k, float def) {
    auto it=m.find(k);
    if (it==m.end()) it=m.emplace(k,def).first;
    return it->second;
}
void ShowToast(const std::string& msg, ImU32 col=IM_COL32(225,70,75,255), float life=2.4f) {
    if (!toasts.empty() && toasts.back().text==msg && ImGui::GetTime()-toasts.back().created<0.3) return;
    ToastMsg t; t.text=msg; t.created=ImGui::GetTime(); t.life=life; t.col=col; t.anim=0.f;
    toasts.push_back(t);
    if (toasts.size()>4) toasts.pop_front();
}
std::string KeyCodeToString(int key) {
    if (!key) return "none";
    switch(key) {
        case VK_LBUTTON: return "lmb"; case VK_RBUTTON: return "rmb"; case VK_MBUTTON: return "mmb";
        case VK_XBUTTON1: return "m4"; case VK_XBUTTON2: return "m5";
        case VK_SHIFT: return "shift"; case VK_CONTROL: return "ctrl"; case VK_MENU: return "alt";
        case VK_SPACE: return "space"; case VK_RETURN: return "enter";
        case VK_ESCAPE: return "esc"; case VK_TAB: return "tab";
        default:
            if (key>='A'&&key<='Z') return std::string(1,(char)(key+32));
            if (key>=VK_F1&&key<=VK_F12){ char b[4]; sprintf_s(b,sizeof(b),"f%d",key-VK_F1+1); return b; }
            if (key>='0'&&key<='9') return std::string(1,(char)key);
            return "key";
    }
}

// ========================= ICONS =========================

void DrawCrosshairIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c,7,col,22,1.4f);
    dl->AddCircleFilled(c,1.4f,col);
    dl->AddLine(ImVec2(c.x-10,c.y),ImVec2(c.x-3,c.y),col,1.4f);
    dl->AddLine(ImVec2(c.x+3,c.y),ImVec2(c.x+10,c.y),col,1.4f);
    dl->AddLine(ImVec2(c.x,c.y-10),ImVec2(c.x,c.y-3),col,1.4f);
    dl->AddLine(ImVec2(c.x,c.y+3),ImVec2(c.x,c.y+10),col,1.4f);
}
void DrawEyeIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddBezierCubic(ImVec2(c.x-10,c.y),ImVec2(c.x-5,c.y-6),ImVec2(c.x+5,c.y-6),ImVec2(c.x+10,c.y),col,1.4f);
    dl->AddBezierCubic(ImVec2(c.x-10,c.y),ImVec2(c.x-5,c.y+6),ImVec2(c.x+5,c.y+6),ImVec2(c.x+10,c.y),col,1.4f);
    dl->AddCircleFilled(c,2.5f,col);
}
void DrawOverlayIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddRect(ImVec2(c.x-8,c.y-8),ImVec2(c.x+8,c.y+8),col,2.f,0,1.3f);
    dl->AddRect(ImVec2(c.x-5,c.y-5),ImVec2(c.x+5,c.y+5),ScaleAlpha(col,0.6f),1.5f,0,1.2f);
    dl->AddRectFilled(ImVec2(c.x-1.5f,c.y-1.5f),ImVec2(c.x+1.5f,c.y+1.5f),col,.5f);
}
void DrawBoltIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    ImVec2 pts[6]={ {c.x-1,c.y-9},{c.x-7,c.y+2},{c.x-1,c.y+2},{c.x+1,c.y+9},{c.x+7,c.y-2},{c.x+1,c.y-2} };
    dl->AddConvexPolyFilled(pts,6,col);
}
void DrawListIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    for (int i=-1;i<=1;i++) {
        float y=c.y+i*5.f;
        dl->AddCircleFilled(ImVec2(c.x-7,y),1.5f,col);
        dl->AddLine(ImVec2(c.x-4,y),ImVec2(c.x+8,y),col,1.3f);
    }
}
void DrawSettingsIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c,4.5f,col,20,1.4f);
    for (int i=0;i<6;i++) {
        float a=i*3.14159f/3.f;
        float cx2=c.x+cosf(a)*8.f, cy2=c.y+sinf(a)*8.f;
        dl->AddCircleFilled(ImVec2(cx2,cy2),2.f,col);
    }
}
void DrawKeyboardIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddRect(ImVec2(c.x-9,c.y-5),ImVec2(c.x+9,c.y+5),col,2.f,0,1.3f);
    for (int r=0;r<2;r++) for (int k=0;k<3;k++) {
        float kx=c.x-4.f+k*4.f, ky=c.y-2.f+r*4.f;
        dl->AddRectFilled(ImVec2(kx-1,ky-1),ImVec2(kx+1,ky+1),col,.5f);
    }
}
void DrawCloseIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddLine(ImVec2(c.x-4,c.y-4),ImVec2(c.x+4,c.y+4),col,1.6f);
    dl->AddLine(ImVec2(c.x+4,c.y-4),ImVec2(c.x-4,c.y+4),col,1.6f);
}
void DrawMinimizeIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddLine(ImVec2(c.x-5,c.y+2),ImVec2(c.x+5,c.y+2),col,1.6f);
}

// ========================= DRAW HELPERS =========================

void DrawShadow(ImDrawList* dl, ImVec2 pos, float w, float h, float alpha) {
    for (int i=0;i<5;i++) {
        float t=i/4.f, off=(i+1)*2.5f;
        dl->AddRectFilled(ImVec2(pos.x-off,pos.y-off),ImVec2(pos.x+w+off,pos.y+h+off),
                          ScaleAlpha(IM_COL32(0,0,0,180),(1.f-t)*alpha),8.f+off*.5f);
    }
}
void DrawPill(ImDrawList* dl, ImVec2 pos, float w, float h, ImU32 bg, ImU32 border=0) {
    dl->AddRectFilled(pos,ImVec2(pos.x+w,pos.y+h),bg,h*.5f);
    if (border) dl->AddRect(pos,ImVec2(pos.x+w,pos.y+h),border,h*.5f,0,1.f);
}
void DrawStatusDot(ImDrawList* dl, ImVec2 c, ImU32 col, float t) {
    float p=(sinf(t*2.5f)*.5f+.5f);
    dl->AddCircleFilled(c,5.f,ScaleAlpha(col,.15f+p*.1f));
    dl->AddCircleFilled(c,3.5f,ScaleAlpha(col,.35f+p*.15f));
    dl->AddCircleFilled(c,2.f,col);
}
void DrawPulsingDot(ImDrawList* dl, ImVec2 c, float t, ImU32 col) {
    dl->AddCircleFilled(c,5.f+t*2.f,ScaleAlpha(col,.4f*(1.f-t)));
    dl->AddCircleFilled(c,3.5f,col);
}
void DrawAnimatedCheckmark(ImDrawList* dl, ImVec2 c, float sz, float progress, ImU32 col) {
    float p=Clamp01(progress*2.f);
    ImVec2 a(c.x-sz*.38f, c.y+sz*.05f);
    ImVec2 m(c.x-sz*.05f, c.y+sz*.38f);
    ImVec2 b(c.x+sz*.42f, c.y-sz*.30f);
    if (p<1.f) {
        ImVec2 mp(a.x+(m.x-a.x)*p, a.y+(m.y-a.y)*p);
        dl->AddLine(a,mp,col,1.6f);
    } else {
        float p2=Clamp01(progress*2.f-1.f);
        ImVec2 ep(m.x+(b.x-m.x)*p2, m.y+(b.y-m.y)*p2);
        dl->AddLine(a,m,col,1.6f);
        dl->AddLine(m,ep,col,1.6f);
    }
}

// ========================= CARD =========================

struct Card {
    ImDrawList* dl;
    ImVec2      pos;
    float       w, h, y;
    float       pad_x = 18.f;
    const char* id;
    float       dt;
    float       anim_progress;
    float       master_alpha;
    float       off_x;

    static constexpr float CHK_STEP = 26.f;
    static constexpr float TOG_STEP = 26.f;
    static constexpr float KB_STEP  = 52.f;
    static constexpr float CMB_STEP = 50.f;
    static constexpr float SLD_STEP = 42.f;
    static constexpr float BTN_H    = 30.f;
    static constexpr float SEC_STEP = 24.f;
    static constexpr float HDR_H    = 46.f;
    static constexpr float CP_STEP  = 28.f;

    Card(ImDrawList* _dl, ImVec2 _pos, float _w, float _h, const char* _id, float _dt, float _ox)
        : dl(_dl), w(_w), h(_h), id(_id), dt(_dt), off_x(_ox)
    {
        // Entry stagger animation
        static std::map<std::string,double> first_seen;
        double now = ImGui::GetTime();
        auto it = first_seen.find(_id);
        if (it == first_seen.end()) { first_seen[_id] = now; it = first_seen.find(_id); }
        double age = now - it->second;
        float raw = Clamp01((float)(age / 0.35));
        float entry_eased = EaseOutBack(raw);

        pos = ImVec2(_pos.x + off_x, _pos.y + (1.f-entry_eased)*18.f);
        y   = HDR_H;
        master_alpha = entry_eased;
        anim_progress = entry_eased;

        // Card background
        DrawShadow(dl, pos, w, h, .22f * master_alpha);
        dl->AddRectFilled(pos, ImVec2(pos.x+w, pos.y+h), ScaleAlpha(C_BG_CARD, master_alpha), 8.f);
        dl->AddLine(ImVec2(pos.x,pos.y+h-1), ImVec2(pos.x+w,pos.y+h-1),
                    ScaleAlpha(C_BG_CARD_BOT, master_alpha*.7f), 1.f);
        dl->AddRect(pos, ImVec2(pos.x+w,pos.y+h), ScaleAlpha(C_BORDER, master_alpha*.8f), 8.f, 0, 1.f);
    }

    void Header(const char* label, bool* enabled = nullptr, const char* subtitle = nullptr) {
        float ha = anim_progress;
        ImU32 tc  = ScaleAlpha(C_TEXT_BRIGHT, ha);
        ImU32 sc  = ScaleAlpha(C_TEXT_HINT,   ha*.8f);
        dl->AddText(ImVec2(pos.x+pad_x, pos.y+14), tc, label);
        if (subtitle) dl->AddText(ImVec2(pos.x+pad_x, pos.y+28), sc, subtitle);

        if (enabled) {
            std::string hk = std::string(id)+"_hdr";
            float& ha2 = AnimState(hdr_hover_anim, hk, 0.f);
            float bw=40.f, bh=18.f;
            ImVec2 bp(pos.x+w-pad_x-bw, pos.y+14.f);
            bool hov = ImGui::IsMouseHoveringRect(bp,ImVec2(bp.x+bw,bp.y+bh));
            ha2 = SmoothLerp(ha2, hov?1.f:0.f, 22.f, dt);

            float& ta = AnimState(tog_anim, hk, *enabled?1.f:0.f);
            ta = SmoothLerp(ta, *enabled?1.f:0.f, 18.f, dt);

            ImU32 trbg = LerpColor(C_BG_INPUT, C_ACCENT, ta);
            dl->AddRectFilled(bp,ImVec2(bp.x+bw,bp.y+bh),trbg,bh*.5f);
            dl->AddRect(bp,ImVec2(bp.x+bw,bp.y+bh),ScaleAlpha(C_BORDER_BR,.6f),bh*.5f,0,1.f);
            float kx = bp.x+bh*.5f + ta*(bw-bh);
            dl->AddCircleFilled(ImVec2(kx,bp.y+bh*.5f), bh*.5f-2.f, C_WHITE);

            if (*enabled) {
                const char* on_txt = "on";
                ImVec2 ots = ImGui::CalcTextSize(on_txt);
                dl->AddText(ImVec2(bp.x+4, bp.y+(bh-ots.y)*.5f), ScaleAlpha(C_WHITE,.8f), on_txt);
            }

            ImGui::SetCursorScreenPos(bp);
            ImGui::PushID(id); ImGui::PushID("hdr");
            ImGui::InvisibleButton("##hdr",ImVec2(bw,bh));
            if (ImGui::IsItemClicked()) { *enabled=!*enabled; }
            ImGui::PopID(); ImGui::PopID();
        }

        float lx0=pos.x+pad_x, lx1=pos.x+w-pad_x;
        dl->AddLine(ImVec2(lx0,pos.y+HDR_H-1),ImVec2(lx1,pos.y+HDR_H-1),
                    ScaleAlpha(C_BORDER_SOFT,ha*.6f),1.f);
    }

    void Chk(const char* label, bool* v) {
        if (anim_progress<.01f){ y+=CHK_STEP; return; }
        std::string ck=std::string(id)+"_chk_"+label;
        float& ca=AnimState(chk_anim,ck,0.f);

        auto prev_it=chk_prev_val.find(ck);
        bool prev=(prev_it!=chk_prev_val.end())?prev_it->second:*v;
        if (prev!=*v){ chk_prev_val[ck]=*v; chk_last_change_time[ck]=ImGui::GetTime(); }
        chk_prev_val[ck]=*v;

        float ts=Clamp01(1.f-(float)((ImGui::GetTime()-chk_last_change_time[ck])/.35));
        ca=SmoothLerp(ca,*v?1.f:0.f,18.f,dt);

        float bx=pos.x+w-pad_x-18.f, by=pos.y+y+1.f, bsz=16.f;
        ImU32 bk=LerpColor(C_BG_INPUT, ScaleAlpha(C_ACCENT,.7f), ca);
        dl->AddRectFilled(ImVec2(bx,by),ImVec2(bx+bsz,by+bsz),bk,4.f);
        dl->AddRect(ImVec2(bx,by),ImVec2(bx+bsz,by+bsz),
                    LerpColor(C_BORDER,C_ACCENT,ca*.8f),4.f,0,1.f);
        if (ca>.05f)
            DrawAnimatedCheckmark(dl,ImVec2(bx+bsz*.5f,by+bsz*.5f),bsz*.5f,ca,
                                  ScaleAlpha(C_WHITE,ca*master_alpha));

        ImU32 tc=LerpColor(C_TEXT_DIM,C_TEXT_BRIGHT,ca*.4f+ts*.15f);
        dl->AddText(ImVec2(pos.x+pad_x,pos.y+y+1.f),ScaleAlpha(tc,master_alpha),label);

        if (anim_progress>.6f) {
            ImGui::SetCursorScreenPos(ImVec2(pos.x+pad_x,pos.y+y));
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##chk",ImVec2(w-pad_x*2,CHK_STEP-2.f));
            if (ImGui::IsItemClicked()){ *v=!*v; }
            ImGui::PopID(); ImGui::PopID();
        }
        y+=CHK_STEP;
    }

    void ChkTog(const char* label, bool* v, bool* toggle_enabled) {
        if (anim_progress<.01f){ y+=TOG_STEP; return; }
        std::string tk=std::string(id)+"_tog_"+label;
        float& ta=AnimState(tog_anim,tk,*v?1.f:0.f);
        ta=SmoothLerp(ta,*v?1.f:0.f,18.f,dt);

        dl->AddText(ImVec2(pos.x+pad_x,pos.y+y+2.f),ScaleAlpha(C_TEXT,master_alpha),label);
        float bw=36.f,bh=16.f;
        ImVec2 bp(pos.x+w-pad_x-bw,pos.y+y+1.f);
        ImU32 tbg=LerpColor(C_BG_INPUT,C_ACCENT,ta);
        dl->AddRectFilled(bp,ImVec2(bp.x+bw,bp.y+bh),tbg,bh*.5f);
        dl->AddRect(bp,ImVec2(bp.x+bw,bp.y+bh),ScaleAlpha(C_BORDER_BR,.6f),bh*.5f,0,1.f);
        float kx=bp.x+bh*.5f+ta*(bw-bh);
        dl->AddCircleFilled(ImVec2(kx,bp.y+bh*.5f),bh*.5f-2.f,C_WHITE);

        if (anim_progress>.6f) {
            ImGui::SetCursorScreenPos(bp);
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##tog",ImVec2(bw,bh));
            if (ImGui::IsItemClicked()){ *v=!*v; if (toggle_enabled) *toggle_enabled=*v; }
            ImGui::PopID(); ImGui::PopID();
        }
        y+=TOG_STEP;
    }

    void KB(const char* label, int* key) {
        if (anim_progress<.01f){ y+=KB_STEP; return; }
        dl->AddText(ImVec2(pos.x+pad_x,pos.y+y),ScaleAlpha(C_TEXT,master_alpha),label);
        y+=18.f;

        std::string kk=std::string(id)+"_kb_"+label;
        bool is_listening=(key_listening && listening_key==key);
        float bw=w-pad_x*2.f, bh=26.f;
        ImVec2 bp(pos.x+pad_x,pos.y+y);

        float& kp=AnimState(kb_press_anim,kk,0.f);
        kp=SmoothLerp(kp,is_listening?1.f:0.f,20.f,dt);

        ImU32 kb_bg=LerpColor(C_BG_INPUT,ScaleAlpha(C_ACCENT,.3f),kp);
        ImU32 kb_br=LerpColor(C_BORDER,C_ACCENT,kp*.7f);
        dl->AddRectFilled(bp,ImVec2(bp.x+bw,bp.y+bh),kb_bg,4.f);
        dl->AddRect(bp,ImVec2(bp.x+bw,bp.y+bh),kb_br,4.f,0,1.f);

        std::string ks = is_listening ? "[ press key ]" : KeyCodeToString(*key);
        ImVec2 kts=ImGui::CalcTextSize(ks.c_str());
        dl->AddText(ImVec2(bp.x+(bw-kts.x)*.5f,bp.y+(bh-kts.y)*.5f),
                    is_listening?C_ACCENT:ScaleAlpha(C_TEXT_BRIGHT,master_alpha),ks.c_str());

        if (is_listening) DrawPulsingDot(dl,ImVec2(bp.x+bw-7,bp.y+bh*.5f),
                                         fmodf((float)ImGui::GetTime()*2.f,1.f),C_ACCENT);

        if (anim_progress>.6f) {
            ImGui::SetCursorScreenPos(bp);
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##kb",ImVec2(bw,bh));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)&&!is_listening){ *key=0; ShowToast(std::string(label)+" cleared",C_WARNING); }
            else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)){ key_listening=true; listening_key=key; listening_wait_release=true; ShowToast("press any key (esc=clear)",C_TEXT_DIM,1.5f); }
            ImGui::PopID(); ImGui::PopID();
        }
        y+=KB_STEP-18.f;
    }

    void Combo(const char* label, int* v, const std::vector<std::string>& items) {
        if (anim_progress<.01f){ y+=CMB_STEP; return; }
        dl->AddText(ImVec2(pos.x+pad_x,pos.y+y),ScaleAlpha(C_TEXT,master_alpha),label);
        y+=17.f;
        ImVec2 bp(pos.x+pad_x,pos.y+y);
        float bw=w-pad_x*2.f, bh=26.f;
        int count=(int)items.size();

        std::string ck=std::string(id)+"_cmb_"+label;
        float& hv=AnimState(cmb_hover_anim,ck,0.f);
        bool bhov=ImGui::IsMouseHoveringRect(bp,ImVec2(bp.x+bw,bp.y+bh));
        hv=SmoothLerp(hv,bhov?1.f:0.f,20.f,dt);

        dl->AddRectFilled(bp,ImVec2(bp.x+bw,bp.y+bh),LerpColor(C_BG_INPUT,C_BG_INPUT_HOV,hv),4.f);
        dl->AddRect(bp,ImVec2(bp.x+bw,bp.y+bh),LerpColor(C_BORDER,ScaleAlpha(C_ACCENT,.6f),hv*.7f),4.f,0,1.f);

        if (*v>=0&&*v<count)
            dl->AddText(ImVec2(bp.x+12,bp.y+6),LerpColor(C_TEXT,C_ACCENT_LIGHT,hv*.3f),items[*v].c_str());

        float ay=bp.y+bh*.5f;
        ImU32 ac=LerpColor(C_TEXT_DIM,C_ACCENT,hv*.5f);
        dl->AddTriangleFilled(ImVec2(bp.x+bw-22,ay-3),ImVec2(bp.x+bw-22,ay+3),ImVec2(bp.x+bw-26,ay),ac);
        dl->AddTriangleFilled(ImVec2(bp.x+bw-12,ay-3),ImVec2(bp.x+bw-12,ay+3),ImVec2(bp.x+bw-8,ay),ac);

        if (count>1) {
            float pw=(bw-24.f)/(float)count;
            for (int i=0;i<count;i++) {
                float px2=bp.x+12+i*pw+pw*.5f-1;
                dl->AddRectFilled(ImVec2(px2,bp.y+bh-4),ImVec2(px2+2,bp.y+bh-2),(i==*v)?C_ACCENT:ScaleAlpha(C_BORDER,.5f));
            }
        }
        if (anim_progress>.6f&&count>0) {
            ImGui::SetCursorScreenPos(bp);
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##cmb",ImVec2(bw,bh));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) *v=(*v+1)%count;
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) *v=(*v-1+count)%count;
            if (ImGui::IsItemHovered()){
                float wheel=ImGui::GetIO().MouseWheel;
                if (wheel>0) *v=(*v-1+count)%count;
                else if (wheel<0) *v=(*v+1)%count;
            }
            ImGui::PopID(); ImGui::PopID();
        }
        y+=bh+7.f;
    }

    void Sld(const char* label, int* v, int vmin, int vmax, const char* suffix="") {
        if (anim_progress<.01f){ y+=SLD_STEP; return; }
        std::string sk=std::string(id)+"_sld_"+label;
        float range=(float)(vmax-vmin);

        auto sv_it=sld_last_val.find(sk);
        if (sv_it==sld_last_val.end()) sld_last_val[sk]=*v;
        else if (sv_it->second!=*v){ sv_it->second=*v; sld_last_change_time[sk]=ImGui::GetTime(); }
        auto ct_it=sld_last_change_time.find(sk);
        double ts2=ct_it!=sld_last_change_time.end()?ImGui::GetTime()-ct_it->second:10.0;
        float hl=Clamp01(1.f-(float)(ts2/.5));

        dl->AddText(ImVec2(pos.x+pad_x,pos.y+y),ScaleAlpha(C_TEXT,master_alpha),label);
        char val_str[32]; sprintf_s(val_str,"%d%s",*v,suffix);
        ImVec2 vs=ImGui::CalcTextSize(val_str);
        float vpad=8.f,vh=vs.y+2.f;
        ImVec2 vp(pos.x+w-vs.x-vpad*2.f-pad_x,pos.y+y-1.f);
        DrawPill(dl,vp,vs.x+vpad*2.f,vh,LerpColor(C_BG_PILL,ScaleAlpha(C_ACCENT,.4f),hl*.6f));
        dl->AddText(ImVec2(vp.x+vpad,vp.y+1.f),LerpColor(C_TEXT_BRIGHT,C_WHITE,hl*.5f),val_str);

        y+=18.f;
        float th=4.f;
        ImVec2 tp(pos.x+pad_x,pos.y+y+3.f);
        float tw=w-pad_x*2.f;
        dl->AddRectFilled(tp,ImVec2(tp.x+tw,tp.y+th),C_BG_INPUT,2.f);
        float r=(range>0)?Clamp01((float)(*v-vmin)/range):0.f;
        if (tw*r>1.f)
            dl->AddRectFilledMultiColor(tp,ImVec2(tp.x+tw*r,tp.y+th),C_ACCENT_DIM,C_ACCENT,C_ACCENT,C_ACCENT_DIM);

        bool hovered=ImGui::IsMouseHoveringRect(ImVec2(tp.x,tp.y-8),ImVec2(tp.x+tw,tp.y+12));
        float& sa=AnimState(sld_hover_anim,sk,1.f);
        sa=SmoothLerp(sa,hovered?1.4f:1.f,22.f,dt);
        float knob=5.5f*sa;
        float kx=tp.x+tw*r, ky=tp.y+th*.5f;
        if (sa>1.05f){
            float g2=(sa-1.f)/.4f;
            dl->AddCircleFilled(ImVec2(kx,ky),knob+4,ScaleAlpha(C_ACCENT,g2*.3f));
            dl->AddCircleFilled(ImVec2(kx,ky),knob+2,ScaleAlpha(C_ACCENT,g2*.4f));
        }
        dl->AddCircleFilled(ImVec2(kx,ky),knob,C_WHITE);
        dl->AddCircleFilled(ImVec2(kx,ky),knob-1.5f,ScaleAlpha(C_ACCENT,hl*.5f));

        if (anim_progress>.6f){
            ImGui::SetCursorScreenPos(ImVec2(tp.x,tp.y-8));
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##sld",ImVec2(tw,16));
            if (ImGui::IsItemActive()){
                float mx=ImGui::GetIO().MousePos.x;
                *v=(int)(vmin+Clamp01((mx-tp.x)/tw)*range+.5f);
            }
            if (ImGui::IsItemHovered()&&range>0){
                float wheel=ImGui::GetIO().MouseWheel;
                if (wheel!=0.f){
                    int step=(vmax-vmin>100)?5:1;
                    *v+=(int)(wheel)*step;
                    if (*v<vmin)*v=vmin; if (*v>vmax)*v=vmax;
                }
            }
            ImGui::PopID(); ImGui::PopID();
        }
        y+=SLD_STEP-18.f;
    }

    void Btn(const char* label, bool* clicked_out, ImU32 btn_bg=0, ImU32 btn_brd=0) {
        if (anim_progress<.01f){ y+=BTN_H+4.f; return; }
        ImVec2 bp(pos.x+pad_x,pos.y+y);
        float bw=w-pad_x*2.f;

        std::string bk=std::string(id)+"_btn_"+label;
        float& hv=AnimState(btn_hover_anim,bk,0.f);
        bool bhov=ImGui::IsMouseHoveringRect(bp,ImVec2(bp.x+bw,bp.y+BTN_H));
        hv=SmoothLerp(hv,bhov?1.f:0.f,22.f,dt);

        ImU32 bg=btn_bg?btn_bg:IM_COL32(180,50,55,220);
        ImU32 border=btn_brd?btn_brd:C_ACCENT_LIGHT;
        dl->AddRectFilledMultiColor(bp,ImVec2(bp.x+bw,bp.y+BTN_H),
            LerpColor(bg,ScaleAlpha(C_ACCENT,.85f),hv*.4f),
            LerpColor(bg,ScaleAlpha(C_ACCENT,.85f),hv*.4f),
            LerpColor(ScaleAlpha(bg,.85f),bg,hv*.5f),
            LerpColor(ScaleAlpha(bg,.85f),bg,hv*.5f));
        dl->AddRect(bp,ImVec2(bp.x+bw,bp.y+BTN_H),LerpColor(border,C_ACCENT_LIGHT,hv*.4f),5.f,0,1.f);
        if (hv>.05f) dl->AddRect(ImVec2(bp.x-1,bp.y-1),ImVec2(bp.x+bw+1,bp.y+BTN_H+1),ScaleAlpha(border,hv*.4f),6.f,0,1.5f);

        ImVec2 ts=ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(bp.x+(bw-ts.x)*.5f,bp.y+(BTN_H-ts.y)*.5f),C_WHITE,label);

        *clicked_out=false;
        if (anim_progress>.6f){
            ImGui::SetCursorScreenPos(bp);
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##btn",ImVec2(bw,BTN_H));
            if (ImGui::IsItemClicked()) *clicked_out=true;
            ImGui::PopID(); ImGui::PopID();
        }
        y+=BTN_H+4.f;
    }

    void ColorPicker(const char* label, float color[4]) {
        if (anim_progress<.01f){ y+=CP_STEP; return; }
        dl->AddText(ImVec2(pos.x+pad_x,pos.y+y),ScaleAlpha(C_TEXT,master_alpha),label);

        float bw=52.f, bh=20.f;
        ImVec2 bp(pos.x+w-pad_x-bw, pos.y+y-1.f);

        // checkerboard background (shows alpha)
        int cs=6;
        for (int r=0;r<(int)(bh/cs)+1;r++) for (int c2=0;c2<(int)(bw/cs)+1;c2++) {
            float cx2=bp.x+c2*cs, cy2=bp.y+r*cs;
            float ex=cx2+cs>bp.x+bw?bp.x+bw:cx2+cs;
            float ey=cy2+cs>bp.y+bh?bp.y+bh:cy2+cs;
            bool odd=(r+c2)%2==0;
            dl->AddRectFilled(ImVec2(cx2,cy2),ImVec2(ex,ey),odd?IM_COL32(100,100,100,200):IM_COL32(155,155,155,200));
        }

        ImU32 col=IM_COL32((int)(color[0]*255+.5f),(int)(color[1]*255+.5f),(int)(color[2]*255+.5f),(int)(color[3]*255+.5f));
        dl->AddRectFilled(bp,ImVec2(bp.x+bw,bp.y+bh),col,4.f);
        dl->AddRect(bp,ImVec2(bp.x+bw,bp.y+bh),C_BORDER_BR,4.f,0,1.f);

        std::string popup_id=std::string("##cpop_")+id+"_"+label;

        if (anim_progress>.6f){
            ImGui::SetCursorScreenPos(bp);
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##cpbtn",ImVec2(bw,bh));
            if (ImGui::IsItemClicked()) ImGui::OpenPopup(popup_id.c_str());

            ImGui::PushStyleColor(ImGuiCol_PopupBg,     ImVec4(.09f,.10f,.13f,.98f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg,     ImVec4(.12f,.13f,.17f,.95f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(.15f,.16f,.21f,1.f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrab,  ImVec4(.88f,.27f,.29f,1.f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.f,.4f,.43f,1.f));
            if (ImGui::BeginPopup(popup_id.c_str())) {
                ImGui::ColorPicker4("##pick",color,
                    ImGuiColorEditFlags_NoLabel|ImGuiColorEditFlags_AlphaBar|
                    ImGuiColorEditFlags_NoSidePreview|ImGuiColorEditFlags_PickerHueWheel);
                ImGui::EndPopup();
            }
            ImGui::PopStyleColor(5);
            ImGui::PopID(); ImGui::PopID();
        }
        y+=CP_STEP;
    }

    void Section(const char* label) {
        if (anim_progress<.5f){ y+=SEC_STEP; return; }
        float a=(anim_progress-.5f)/.5f;
        dl->AddText(ImVec2(pos.x+pad_x,pos.y+y+4.f),ScaleAlpha(C_TEXT_HINT,a),label);
        ImVec2 ls=ImGui::CalcTextSize(label);
        float lx0=pos.x+pad_x+ls.x+10.f, lx1=pos.x+w-pad_x;
        float ly=pos.y+y+ls.y*.5f+4.f;
        dl->AddLine(ImVec2(lx0,ly),ImVec2(lx1,ly),ScaleAlpha(C_BORDER,a*.7f),1.f);
        y+=SEC_STEP;
    }

    void Space(float px) { y+=px; }
};

// ========================= NAV =========================

typedef void (*IconFn)(ImDrawList*, ImVec2, ImU32);
struct NavSlot { const char* label; IconFn icon; };
const NavSlot NAV[] = {
    {"aimbot",   DrawCrosshairIcon},
    {"visuals",  DrawEyeIcon},
    {"overlays", DrawOverlayIcon},
    {"exploits", DrawBoltIcon},
    {"configs",  DrawListIcon},
    {"settings", DrawSettingsIcon},
};
const int NAV_COUNT = sizeof(NAV)/sizeof(NAV[0]);

bool RenderNavItem(ImDrawList* dl, ImVec2 origin, int idx, bool active, float dt) {
    float w=200-24, h=38;
    ImVec2 pos(origin.x+12,origin.y);
    bool hov=ImGui::IsMouseHoveringRect(pos,ImVec2(pos.x+w,pos.y+h));
    std::string key=std::string("nav_")+NAV[idx].label;
    float& ha=AnimState(nav_hover_anim,key,0.f);
    ha=SmoothLerp(ha,(hov||active)?1.f:0.f,22.f,dt);

    if (ha>.01f){
        ImU32 bg=ScaleAlpha(C_WHITE,ha*.04f);
        if (active) bg=LerpColor(bg,ScaleAlpha(C_ACCENT,.12f),1.f);
        dl->AddRectFilled(pos,ImVec2(pos.x+w,pos.y+h),bg,6.f);
    }

    ImU32 ic_col=LerpColor(C_TEXT_DIM,C_ACCENT,active?1.f:ha*.4f);
    NAV[idx].icon(dl,ImVec2(pos.x+22,pos.y+h*.5f),ic_col);
    ImU32 txt_col=active?C_WHITE:LerpColor(C_TEXT_DIM,C_TEXT_BRIGHT,ha*.7f);
    ImVec2 ts=ImGui::CalcTextSize(NAV[idx].label);
    dl->AddText(ImVec2(pos.x+44,pos.y+(h-ts.y)*.5f),txt_col,NAV[idx].label);

    if (active){
        float dot_x=pos.x+w-12, dot_y=pos.y+h*.5f;
        dl->AddCircleFilled(ImVec2(dot_x,dot_y),2.5f,C_ACCENT);
    }

    ImGui::SetCursorScreenPos(pos);
    ImGui::PushID(idx);
    ImGui::InvisibleButton("##nav",ImVec2(w,h));
    bool clicked=ImGui::IsItemClicked();
    ImGui::PopID();
    return clicked;
}

void RenderToasts(ImDrawList* dl_fg, ImVec2 anchor, float dt) {
    double now=ImGui::GetTime();
    for (auto it=toasts.begin();it!=toasts.end();) {
        if (now-it->created>it->life){ it=toasts.erase(it); continue; }
        ++it;
    }
    float ty=anchor.y;
    for (auto it=toasts.rbegin();it!=toasts.rend();++it){
        double age=now-it->created;
        float a=Clamp01((float)(age/.25f))*Clamp01((float)((it->life-age)/.4f));
        it->anim=SmoothLerp(it->anim,1.f,20.f,dt);

        ImVec2 ts2=ImGui::CalcTextSize(it->text.c_str());
        float px2=14.f,py2=8.f,tw=ts2.x+px2*2.f+24.f,th=ts2.y+py2*2.f;
        ImVec2 tp(anchor.x-tw, ty-th);
        tp.x+=(1.f-EaseOutCubic(it->anim))*30.f;

        DrawShadow(dl_fg,tp,tw,th,.4f*a);
        dl_fg->AddRectFilled(tp,ImVec2(tp.x+tw,tp.y+th),ScaleAlpha(IM_COL32(25,27,33,250),a),8.f);
        dl_fg->AddRect(tp,ImVec2(tp.x+tw,tp.y+th),ScaleAlpha(C_BORDER_BR,a*.8f),8.f,0,1.f);
        dl_fg->AddRectFilled(tp,ImVec2(tp.x+3,tp.y+th),ScaleAlpha(it->col,a),1.5f);
        DrawStatusDot(dl_fg,ImVec2(tp.x+16,tp.y+th*.5f),it->col,(float)now);
        dl_fg->AddText(ImVec2(tp.x+30,tp.y+py2),ScaleAlpha(C_TEXT_BRIGHT,a),it->text.c_str());
        ty-=th+8.f;
    }
}

} // anonymous namespace

// ========================= APPLY THEME =========================

void ApplyDarkTheme() {
    ImGuiStyle& s=ImGui::GetStyle();
    s.WindowRounding=12; s.FrameRounding=5; s.GrabRounding=5;
    s.PopupRounding=5; s.ChildRounding=8;
    s.WindowPadding=ImVec2(0,0); s.FramePadding=ImVec2(4,3);
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0,0,0,0);
    s.Colors[ImGuiCol_ChildBg]  = ImVec4(0,0,0,0);
    s.Colors[ImGuiCol_Border]   = ImVec4(0,0,0,0);
}

// ========================= RENDER GUI =========================

void RenderGui() {
    ApplyDarkTheme();

    // Window subclass (once)
    static bool subclass_done=false;
    if (!subclass_done) {
        HWND hw=GetActiveWindow();
        if (hw){ SetupWindowSubclass(hw); subclass_done=true; }
    }

    // First-run defaults
    static bool defaults_set=false;
    if (!defaults_set) {
        g_Options.General.AccentColor[0]=225.f/255.f;
        g_Options.General.AccentColor[1]=70.f/255.f;
        g_Options.General.AccentColor[2]=75.f/255.f;
        g_Options.General.AccentColor[3]=1.f;
        g_Options.General.MenuOpacity=100;
        g_Options.LegitBot.AimBot.Smoothness=50;
        g_Options.LegitBot.AimBot.FovRadius=120;
        g_Options.Misc.Alerts.LowHealthThreshold=25;
        g_Options.Exploits.Spawner.MoneyDropAmount=10;
        g_Options.Visuals.ESP.Players.BoxColor[0]=1.f; g_Options.Visuals.ESP.Players.BoxColor[3]=1.f;
        g_Options.Visuals.ESP.Players.SkeletonColor[0]=.9f; g_Options.Visuals.ESP.Players.SkeletonColor[3]=1.f;
        g_Options.Visuals.ESP.Players.NameColor[0]=1.f; g_Options.Visuals.ESP.Players.NameColor[1]=1.f; g_Options.Visuals.ESP.Players.NameColor[2]=1.f; g_Options.Visuals.ESP.Players.NameColor[3]=1.f;
        g_Options.Visuals.ESP.Vehicles.Color[0]=1.f; g_Options.Visuals.ESP.Vehicles.Color[1]=.8f; g_Options.Visuals.ESP.Vehicles.Color[3]=1.f;
        defaults_set=true;
    }

    // Update dynamic accent colours
    {
        float* ac=g_Options.General.AccentColor;
        int r=(int)(ac[0]*255.f+.5f), g=(int)(ac[1]*255.f+.5f), b=(int)(ac[2]*255.f+.5f);
        auto clb=[](int v)->int{ return v<0?0:(v>255?255:v); };
        C_ACCENT       = IM_COL32(r, g, b, 255);
        C_ACCENT_DIM   = IM_COL32(clb((int)(r*.8f)),clb((int)(g*.71f)),clb((int)(b*.73f)),220);
        C_ACCENT_LIGHT = IM_COL32(clb(r+15),clb(g+35),clb(b+35),220);
        C_ACCENT_GLOW  = IM_COL32(r, g, b, 90);
    }

    // INSERT key toggle (raw Win32 so it works as an overlay)
    static bool insert_was_down=false;
    bool insert_down=(GetAsyncKeyState(VK_INSERT)&0x8000)!=0;
    if (insert_down&&!insert_was_down){
        menu_visible=!menu_visible;
        if (menu_visible) gui_fade_anim=0.f;
    }
    insert_was_down=insert_down;
    g_bMenuOpen=menu_visible;

    // Key listener
    if (key_listening && listening_key) {
        if (listening_wait_release) {
            bool any=false;
            for (int k=1;k<256;k++) if (GetAsyncKeyState(k)&0x8000){ any=true; break; }
            if (!any) listening_wait_release=false;
        } else {
            for (int k=1;k<256;k++) {
                if (!(GetAsyncKeyState(k)&0x8000)) continue;
                if (k==VK_ESCAPE) *listening_key=0;
                else              *listening_key=k;
                key_listening=false; listening_key=nullptr;
                break;
            }
        }
    }

    float dt=ImGui::GetIO().DeltaTime;
    if (dt>.1f) dt=.1f;
    gui_fade_anim=SmoothLerp(gui_fade_anim,menu_visible?1.f:0.f,8.f,dt);
    if (gui_fade_anim<.005f && !menu_visible) {
        ImGui::SetNextWindowSize(ImVec2(1,1));
        ImGui::SetNextWindowPos(ImVec2(-10,-10));
        ImGui::Begin("##hidden",nullptr,
            ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoInputs);
        ImGui::End();
        return;
    }

    // FPS history
    float cur_fps=ImGui::GetIO().Framerate;
    fps_smoothed=fps_smoothed==0.f?cur_fps:SmoothLerp(fps_smoothed,cur_fps,4.f,dt);
    static double last_fps_sample=0;
    if (ImGui::GetTime()-last_fps_sample>.066){
        fps_history[fps_history_idx]=fps_smoothed;
        fps_history_idx=(fps_history_idx+1)%FPS_HIST_SIZE;
        last_fps_sample=ImGui::GetTime();
    }

    ImVec2 scaled_size(1000.f*menu_scale, 700.f*menu_scale);
    ImGui::SetNextWindowSize(scaled_size,ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(80,50),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.f);

    float opacity=g_Options.General.MenuOpacity/100.f;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, opacity * EaseOutCubic(gui_fade_anim));

    int wf=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
           ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse|
           ImGuiWindowFlags_NoBackground;
    ImGui::Begin("##kenzo",nullptr,wf);

    // Disable ImGui mouse when menu hidden
    if (!menu_visible){
        ImGuiIO& io=ImGui::GetIO();
        for (int i=0;i<5;i++) io.MouseDown[i]=false;
        io.MouseWheel=0.f;
    }

    ImVec2 wp=ImGui::GetWindowPos();
    ImVec2 ws=ImGui::GetWindowSize();
    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImDrawList* fg=ImGui::GetForegroundDrawList();

    float fade=EaseOutCubic(gui_fade_anim);
    float oy=(1.f-fade)*14.f;
    wp.y+=oy;

    const float TOPBAR_H=60.f, SIDEBAR_W=200.f;

    // Dragging via topbar
    ImVec2 hdr_min(wp.x,wp.y), hdr_max(wp.x+ws.x,wp.y+TOPBAR_H);
    if (ImGui::IsMouseHoveringRect(hdr_min,hdr_max)&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
        dragging=true;
        drag_offset=ImVec2(ImGui::GetIO().MousePos.x-wp.x,ImGui::GetIO().MousePos.y-wp.y);
    }
    if (dragging){
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)){
            ImVec2 np(ImGui::GetIO().MousePos.x-drag_offset.x,ImGui::GetIO().MousePos.y-drag_offset.y-oy);
            ImGui::SetWindowPos(np);
            wp=ImVec2(np.x,np.y+oy);
        } else dragging=false;
    }

    // Drop shadow
    for (int i=0;i<6;i++){
        float t=i/5.f, off=(i+1)*3.f;
        dl->AddRect(ImVec2(wp.x-off,wp.y-off),ImVec2(wp.x+ws.x+off,wp.y+ws.y+off),
                    ScaleAlpha(IM_COL32(0,0,0,160),(1.f-t)*.4f*fade),14.f+off*.6f,0,1.5f);
    }

    // Backdrop
    dl->AddRectFilled(wp,ImVec2(wp.x+ws.x,wp.y+ws.y),C_BG_WIN,12.f);

    // Corner glows
    for (int i=0;i<10;i++) dl->AddCircleFilled(ImVec2(wp.x+60,wp.y+60),200+i*35,ScaleAlpha(C_ACCENT,.025f*(1.f-i/9.f)));
    for (int i=0;i<8;i++)  dl->AddCircleFilled(ImVec2(wp.x+ws.x-80,wp.y+ws.y-80),180+i*30,ScaleAlpha(C_ACCENT,.018f*(1.f-i/7.f)));

    // ── TOP BAR ──────────────────────────────────────────────
    dl->AddRectFilled(wp,ImVec2(wp.x+ws.x,wp.y+TOPBAR_H),C_BG_TOPBAR,12.f);
    dl->AddRectFilled(ImVec2(wp.x,wp.y+TOPBAR_H-8),ImVec2(wp.x+ws.x,wp.y+TOPBAR_H),C_BG_TOPBAR,0.f);
    dl->AddLine(ImVec2(wp.x,wp.y+TOPBAR_H),ImVec2(wp.x+ws.x,wp.y+TOPBAR_H),ScaleAlpha(C_BORDER_SOFT,.8f),1.f);

    // Logo mark
    dl->AddRectFilled(ImVec2(wp.x+22,wp.y+22),ImVec2(wp.x+38,wp.y+38),C_ACCENT,4.f);
    dl->AddRectFilled(ImVec2(wp.x+26,wp.y+26),ImVec2(wp.x+34,wp.y+34),C_BG_TOPBAR,2.f);
    dl->AddCircleFilled(ImVec2(wp.x+30,wp.y+30),2,C_ACCENT);

    dl->AddText(ImVec2(wp.x+50,wp.y+17),C_TEXT_BRIGHT,"KENZO EXTERNAL");
    ImVec2 brand_sz=ImGui::CalcTextSize("KENZO EXTERNAL");
    dl->AddText(ImVec2(wp.x+50+brand_sz.x+2,wp.y+17),C_ACCENT,"v6");
    dl->AddText(ImVec2(wp.x+50,wp.y+31),C_TEXT_HINT,"premium external cheat");

    // Status pill (center)
    {
        char status_buf[64]; sprintf_s(status_buf,"ACTIVE  %.0f FPS",ImGui::GetIO().Framerate);
        ImVec2 sts=ImGui::CalcTextSize(status_buf);
        float pill_w=sts.x+38,pill_h=26;
        ImVec2 pp(wp.x+ws.x*.5f-pill_w*.5f,wp.y+(TOPBAR_H-pill_h)*.5f);
        DrawPill(dl,pp,pill_w,pill_h,C_BG_PILL,C_BORDER);
        DrawStatusDot(dl,ImVec2(pp.x+14,pp.y+pill_h*.5f),C_SUCCESS,(float)ImGui::GetTime());
        dl->AddText(ImVec2(pp.x+26,pp.y+(pill_h-sts.y)*.5f),C_TEXT_BRIGHT,status_buf);
    }

    // Top-right: PREMIUM badge + build + controls
    {
        const char* prem_text="PREMIUM";
        ImVec2 pts=ImGui::CalcTextSize(prem_text);
        float pp_w=pts.x+22,pp_h=22;
        ImVec2 pp_pos(wp.x+ws.x-80-pp_w-14,wp.y+19);
        float prem_pulse=(sinf((float)ImGui::GetTime()*1.4f)*.5f+.5f);
        DrawPill(dl,pp_pos,pp_w,pp_h,
                 LerpColor(IM_COL32(60,22,25,200),IM_COL32(90,28,32,220),prem_pulse),
                 ScaleAlpha(C_ACCENT,.6f+prem_pulse*.3f));
        dl->AddCircleFilled(ImVec2(pp_pos.x+10,pp_pos.y+pp_h*.5f),2.5f,C_ACCENT);
        dl->AddText(ImVec2(pp_pos.x+18,pp_pos.y+(pp_h-pts.y)*.5f),C_ACCENT_LIGHT,prem_text);

        const char* build_str="build 1.3.0  2026";
        ImVec2 bs=ImGui::CalcTextSize(build_str);
        dl->AddText(ImVec2(wp.x+ws.x-bs.x-80,wp.y+23),C_TEXT_HINT,build_str);

        float btn_sz=24.f;
        // Minimize (cosmetic)
        ImVec2 mn_pos(wp.x+ws.x-60,wp.y+18);
        bool mn_hov=ImGui::IsMouseHoveringRect(mn_pos,ImVec2(mn_pos.x+btn_sz,mn_pos.y+btn_sz));
        float& mh=AnimState(btn_hover_anim,"tb_mn",0.f);
        mh=SmoothLerp(mh,mn_hov?1.f:0.f,20.f,dt);
        dl->AddRectFilled(mn_pos,ImVec2(mn_pos.x+btn_sz,mn_pos.y+btn_sz),ScaleAlpha(C_WHITE,mh*.06f),4.f);
        DrawMinimizeIcon(dl,ImVec2(mn_pos.x+btn_sz*.5f,mn_pos.y+btn_sz*.5f),LerpColor(C_TEXT_DIM,C_TEXT_BRIGHT,mh));

        // Close
        ImVec2 cl_pos(wp.x+ws.x-32,wp.y+18);
        bool cl_hov=ImGui::IsMouseHoveringRect(cl_pos,ImVec2(cl_pos.x+btn_sz,cl_pos.y+btn_sz));
        float& ch=AnimState(btn_hover_anim,"tb_cl",0.f);
        ch=SmoothLerp(ch,cl_hov?1.f:0.f,20.f,dt);
        dl->AddRectFilled(cl_pos,ImVec2(cl_pos.x+btn_sz,cl_pos.y+btn_sz),ScaleAlpha(C_ACCENT,ch*.3f),4.f);
        DrawCloseIcon(dl,ImVec2(cl_pos.x+btn_sz*.5f,cl_pos.y+btn_sz*.5f),LerpColor(C_TEXT_DIM,C_WHITE,ch));
        ImGui::SetCursorScreenPos(cl_pos);
        ImGui::InvisibleButton("##cl",ImVec2(btn_sz,btn_sz));
        if (ImGui::IsItemClicked()){ should_unload=true; ShowToast("unloading...",C_WARNING,2.f); }
    }

    // ── SIDEBAR ──────────────────────────────────────────────
    ImVec2 sb_pos(wp.x,wp.y+TOPBAR_H);
    dl->AddRectFilled(sb_pos,ImVec2(sb_pos.x+SIDEBAR_W,wp.y+ws.y),C_BG_SIDEBAR,0);
    dl->AddLine(ImVec2(sb_pos.x+SIDEBAR_W,sb_pos.y),ImVec2(sb_pos.x+SIDEBAR_W,wp.y+ws.y),ScaleAlpha(C_BORDER_SOFT,.6f),1.f);
    dl->AddText(ImVec2(sb_pos.x+18,sb_pos.y+14),C_TEXT_HINT,"NAVIGATION");

    float nav_y=sb_pos.y+34, nav_item_h=38, nav_gap=4;
    float active_y_pill=0, active_h_pill=0; bool any_active=false;
    for (int i=0;i<NAV_COUNT;i++){
        if (RenderNavItem(dl,ImVec2(sb_pos.x,nav_y),i,active_tab==i,dt)) active_tab=i;
        if (active_tab==i){ active_y_pill=nav_y; active_h_pill=nav_item_h; any_active=true; }
        nav_y+=nav_item_h+nav_gap;
    }
    if (any_active){
        if (!nav_pill_init){ nav_pill_y=active_y_pill; nav_pill_h=active_h_pill; nav_pill_alpha=1.f; nav_pill_init=true; }
        else { nav_pill_y=SmoothLerp(nav_pill_y,active_y_pill,18.f,dt); nav_pill_h=SmoothLerp(nav_pill_h,active_h_pill,18.f,dt); nav_pill_alpha=SmoothLerp(nav_pill_alpha,1.f,14.f,dt); }
        float py=nav_pill_y+(nav_pill_h-20)*.5f;
        dl->AddRectFilled(ImVec2(sb_pos.x+4,py),ImVec2(sb_pos.x+7,py+20),ScaleAlpha(C_ACCENT,nav_pill_alpha),1.5f);
        dl->AddRectFilled(ImVec2(sb_pos.x+2,py),ImVec2(sb_pos.x+7,py+20),ScaleAlpha(C_ACCENT_GLOW,nav_pill_alpha*.5f),1.5f);
    }

    // Sidebar footer
    float footer_y=wp.y+ws.y-64;
    dl->AddLine(ImVec2(sb_pos.x+14,footer_y),ImVec2(sb_pos.x+SIDEBAR_W-14,footer_y),ScaleAlpha(C_BORDER_SOFT,.5f),1.f);
    dl->AddText(ImVec2(sb_pos.x+18,footer_y+12),C_TEXT_HINT,"SESSION");
    DrawStatusDot(dl,ImVec2(sb_pos.x+22,footer_y+36),C_SUCCESS,(float)ImGui::GetTime());
    dl->AddText(ImVec2(sb_pos.x+36,footer_y+32),C_TEXT,"premium");

    // ── CONTENT ──────────────────────────────────────────────
    if (active_tab!=prev_active_tab){ tab_content_anim=0.f; prev_active_tab=active_tab; }
    tab_content_anim=SmoothLerp(tab_content_anim,1.f,14.f,dt);
    float off_x=(1.f-EaseOutCubic(tab_content_anim))*30.f;

    float content_x=wp.x+SIDEBAR_W;
    float content_y=wp.y+TOPBAR_H;
    float content_w=ws.x-SIDEBAR_W;
    float pad=18.f, col_gap=14.f, v_gap=12.f;
    float cw=(content_w-pad*2.f-col_gap)*.5f;

    // Tab title bar
    const char* tab_subs[]={
        "precision aiming and target acquisition",
        "world rendering and player visualization",
        "on-screen overlays and notifications",
        "movement and weapon exploits",
        "configuration management",
        "menu preferences and session controls",
    };
    dl->AddText(ImVec2(content_x+pad,content_y+14),C_TEXT_BRIGHT,NAV[active_tab].label);
    dl->AddText(ImVec2(content_x+pad,content_y+30),C_TEXT_HINT,tab_subs[active_tab]);
    float tab_tl_y=content_y+48.f;
    dl->AddLine(ImVec2(content_x+pad,tab_tl_y),ImVec2(content_x+content_w-pad,tab_tl_y),ScaleAlpha(C_BORDER_SOFT,.5f),1.f);
    content_y=tab_tl_y+12.f;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,1.f);

    // ═══════════════════════════════════════════════
    // TAB 0 — AIMBOT
    // ═══════════════════════════════════════════════
    if (active_tab==0){
        { Card c(dl,ImVec2(content_x+pad,content_y),cw,460,"aimbot",dt,off_x);
          c.Header("aimbot",&g_Options.LegitBot.AimBot.Enabled,"precision targeting");
          c.KB("keybind",&g_Options.LegitBot.AimBot.KeyBind);
          c.Chk("target npcs",&g_Options.LegitBot.AimBot.TargetNPC);
          c.Chk("visible check",&g_Options.LegitBot.AimBot.VisibleCheck);
          c.Chk("prediction",&g_Options.LegitBot.AimBot.Prediction);
          c.Section("precision");
          c.Sld("smoothness",&g_Options.LegitBot.AimBot.Smoothness,0,100,"%");
          c.Sld("fov radius",&g_Options.LegitBot.AimBot.FovRadius,0,500,"px");
          c.Sld("max distance",&g_Options.LegitBot.AimBot.MaxDistance,0,1000,"m");
          c.Section("targeting");
          static std::vector<std::string> hitboxes={"head","neck","chest","pelvis"};
          c.Combo("hitbox",&g_Options.LegitBot.AimBot.HitBox,hitboxes);
        }
        { Card c(dl,ImVec2(content_x+pad+cw+col_gap,content_y),cw,268,"silent_aim",dt,off_x);
          c.Header("silent aim",&g_Options.LegitBot.SilentAim.Enabled,"bullet redirection");
          c.KB("keybind",&g_Options.LegitBot.SilentAim.KeyBind);
          c.Section("targeting");
          c.Chk("target npcs",&g_Options.LegitBot.SilentAim.ShotNPC);
          c.Chk("visible check",&g_Options.LegitBot.SilentAim.VisibleCheck);
          c.Chk("magic bullet",&g_Options.LegitBot.SilentAim.MagicBullet);
          static std::vector<std::string> hb2={"head","neck","chest","pelvis"};
          static int sil_hb=0;
          c.Combo("hitbox",&sil_hb,hb2);
          c.Sld("max distance",&g_Options.LegitBot.SilentAim.MaxDistance,0,1000,"m");
        }
        { Card c(dl,ImVec2(content_x+pad+cw+col_gap,content_y+268+v_gap),cw,180,"trigger",dt,off_x);
          c.Header("triggerbot",&g_Options.LegitBot.Trigger.Enabled,"auto-fire on target");
          c.KB("keybind",&g_Options.LegitBot.Trigger.KeyBind);
          c.Chk("target npcs",&g_Options.LegitBot.Trigger.ShotNPC);
          c.Chk("visible check",&g_Options.LegitBot.Trigger.VisibleCheck);
          c.Sld("max distance",&g_Options.LegitBot.Trigger.MaxDistance,0,1000,"m");
        }
    }

    // ═══════════════════════════════════════════════
    // TAB 1 — VISUALS
    // ═══════════════════════════════════════════════
    else if (active_tab==1){
        { Card c(dl,ImVec2(content_x+pad,content_y),cw,540,"players",dt,off_x);
          c.Header("players",&g_Options.Visuals.ESP.Players.Enabled,"esp & overlays");
          c.Section("box & geometry");
          c.Chk("box",&g_Options.Visuals.ESP.Players.Box);
          c.Chk("corner box",&g_Options.Visuals.ESP.Players.CornerBox);
          c.Chk("skeleton",&g_Options.Visuals.ESP.Players.Skeleton);
          c.Chk("head",&g_Options.Visuals.ESP.Players.Head);
          c.Section("info text");
          c.Chk("name",&g_Options.Visuals.ESP.Players.Name);
          c.Chk("health bar",&g_Options.Visuals.ESP.Players.HealthBar);
          c.Chk("armor bar",&g_Options.Visuals.ESP.Players.ArmorBar);
          c.Chk("weapon name",&g_Options.Visuals.ESP.Players.WeaponName);
          c.Chk("distance",&g_Options.Visuals.ESP.Players.Distance);
          c.Chk("snap lines",&g_Options.Visuals.ESP.Players.SnapLines);
          c.Section("appearance");
          static std::vector<std::string> box_styles={"outline","filled","corners"};
          c.Combo("box style",&g_Options.Visuals.ESP.Players.BoxStyle,box_styles);
          c.Chk("distance color",&g_Options.Visuals.ESP.Players.DistanceColor);
          c.Section("colors");
          c.ColorPicker("box color",g_Options.Visuals.ESP.Players.BoxColor);
          c.ColorPicker("skeleton",g_Options.Visuals.ESP.Players.SkeletonColor);
          c.ColorPicker("name color",g_Options.Visuals.ESP.Players.NameColor);
        }
        { Card c(dl,ImVec2(content_x+pad+cw+col_gap,content_y),cw,232,"vehicles",dt,off_x);
          c.Header("vehicles",&g_Options.Visuals.ESP.Vehicles.Enabled);
          c.Chk("ignore occupied",&g_Options.Visuals.ESP.Vehicles.IgnoreOccupiedVehicles);
          c.Chk("name",&g_Options.Visuals.ESP.Vehicles.Name);
          c.Chk("distance",&g_Options.Visuals.ESP.Vehicles.Distance);
          c.Chk("marker",&g_Options.Visuals.ESP.Vehicles.Marker);
          c.Section("colors");
          c.ColorPicker("color",g_Options.Visuals.ESP.Vehicles.Color);
        }
        { Card c(dl,ImVec2(content_x+pad+cw+col_gap,content_y+232+v_gap),cw,180,"set_esp",dt,off_x);
          c.Header("filters");
          c.Chk("show local player",&g_Options.Visuals.ESP.Players.ShowLocalPlayer);
          c.Chk("show npcs",&g_Options.Visuals.ESP.Players.ShowNPCs);
          c.Chk("visible only",&g_Options.Visuals.ESP.Players.VisibleOnly);
          c.Sld("render distance",&g_Options.Visuals.ESP.Players.RenderDistance,0,1000,"m");
        }
    }

    // ═══════════════════════════════════════════════
    // TAB 2 — OVERLAYS
    // ═══════════════════════════════════════════════
    else if (active_tab==2){
        { Card c(dl,ImVec2(content_x+pad,content_y),cw,252,"screen",dt,off_x);
          c.Header("screen overlays",nullptr,"hud elements");
          c.Chk("watermark",&g_Options.Misc.Screen.EnableWatermark);
          c.Chk("keybind list",&g_Options.Misc.Screen.EnableKeybindList);
          c.Section("fov indicators");
          c.Chk("aimbot fov",&g_Options.Misc.Screen.ShowAimbotFov);
          c.Chk("silent fov",&g_Options.Misc.Screen.ShowSilentAimFov);
          c.Chk("trigger fov",&g_Options.Misc.Screen.ShowTriggerFov);
        }
        { Card c(dl,ImVec2(content_x+pad,content_y+252+v_gap),cw,196,"alerts",dt,off_x);
          c.Header("alerts",nullptr,"notifications");
          c.Chk("admin join/leave",&g_Options.Misc.Alerts.AdminNotify);
          c.Chk("low health warning",&g_Options.Misc.Alerts.LowHealthWarning);
          c.Sld("health threshold",&g_Options.Misc.Alerts.LowHealthThreshold,0,100,"%");
          c.Chk("wanted level alert",&g_Options.Misc.Alerts.WantedLevelAlert);
        }
        { Card c(dl,ImVec2(content_x+pad+cw+col_gap,content_y),cw,296,"info_ov",dt,off_x);
          c.Header("about",nullptr,"info");
          c.Space(6);
          dl->AddText(ImVec2(c.pos.x+c.pad_x,c.pos.y+c.y),C_TEXT_DIM,"build");
          dl->AddText(ImVec2(c.pos.x+c.w-c.pad_x-96,c.pos.y+c.y),C_TEXT_BRIGHT,"1.3.0 (2026)");
          c.y+=22;
          dl->AddText(ImVec2(c.pos.x+c.pad_x,c.pos.y+c.y),C_TEXT_DIM,"subscription");
          dl->AddText(ImVec2(c.pos.x+c.w-c.pad_x-60,c.pos.y+c.y),C_SUCCESS,"premium");
          c.y+=22;
          dl->AddText(ImVec2(c.pos.x+c.pad_x,c.pos.y+c.y),C_TEXT_DIM,"renews in");
          dl->AddText(ImVec2(c.pos.x+c.w-c.pad_x-50,c.pos.y+c.y),C_TEXT_BRIGHT,"28 days");
          c.y+=30;
          // Session uptime
          c.Section("session");
          static DWORD session_start=GetTickCount();
          DWORD elapsed=(GetTickCount()-session_start)/1000;
          char uptime_buf[32]; sprintf_s(uptime_buf,"%02d:%02d:%02d",elapsed/3600,(elapsed%3600)/60,elapsed%60);
          dl->AddText(ImVec2(c.pos.x+c.pad_x,c.pos.y+c.y),C_TEXT_DIM,"uptime");
          dl->AddText(ImVec2(c.pos.x+c.w-c.pad_x-ImGui::CalcTextSize(uptime_buf).x,c.pos.y+c.y),C_TEXT_BRIGHT,uptime_buf);
          c.y+=22;
          dl->AddText(ImVec2(c.pos.x+c.pad_x,c.pos.y+c.y),C_TEXT_HINT,"thanks for using kenzo");
        }
    }

    // ═══════════════════════════════════════════════
    // TAB 3 — EXPLOITS
    // ═══════════════════════════════════════════════
    else if (active_tab==3){
        { Card c(dl,ImVec2(content_x+pad,content_y),cw,296,"local",dt,off_x);
          c.Header("local player",nullptr,"self modifications");
          c.Chk("god mode",&g_Options.Exploits.LocalPlayer.God);
          c.Chk("noclip",&g_Options.Exploits.LocalPlayer.Noclip);
          c.Chk("invisible",&g_Options.Exploits.LocalPlayer.Invisible);
          c.Chk("shrink",&g_Options.Exploits.LocalPlayer.Shrink);
          c.Chk("speed hack",&g_Options.Exploits.LocalPlayer.speed);
          static int spd=(int)g_Options.Exploits.LocalPlayer.Player_speed;
          if (spd<1) spd=5;
          c.Sld("player speed",&spd,1,10);
          g_Options.Exploits.LocalPlayer.Player_speed=(float)spd;
          c.Chk("teleport to marker",&g_Options.Exploits.LocalPlayer.TeleportToMarker);
        }
        { Card c(dl,ImVec2(content_x+pad+cw+col_gap,content_y),cw,240,"weapon",dt,off_x);
          c.Header("weapon",nullptr,"firearm tweaks");
          c.Chk("no reload",&g_Options.Exploits.Weapon.NoReload);
          c.Chk("no recoil",&g_Options.Exploits.Weapon.NoRecoil);
          c.Chk("no spread",&g_Options.Exploits.Weapon.NoSpread);
          c.Chk("rapid fire",&g_Options.Exploits.Weapon.RapidFire);
          c.Chk("one shot kill",&g_Options.Exploits.Weapon.OneShotKill);
          c.Chk("infinite ammo",&g_Options.Exploits.Weapon.InfiniteAmmo);
        }
        { Card c(dl,ImVec2(content_x+pad,content_y+296+v_gap),cw*2+col_gap,244,"spawner",dt,off_x);
          c.Header("spawner",nullptr,"spawn entities & drop cash");
          static std::vector<std::string> vehicles={"adder","zentorno","t20","oppressor mk2","buzzard","insurgent","rhino","lazer"};
          static std::vector<std::string> weapons={"pistol","smg","assault rifle","sniper","rpg","minigun","shotgun","knife"};
          c.Section("vehicles");
          c.Combo("model",&g_Options.Exploits.Spawner.VehicleModel,vehicles);
          static bool spawn_v=false;
          c.Btn("SPAWN VEHICLE",&spawn_v);
          c.Section("weapons");
          c.Combo("weapon",&g_Options.Exploits.Spawner.WeaponModel,weapons);
          static bool spawn_w=false;
          c.Btn("GIVE WEAPON",&spawn_w);
          c.Section("money");
          c.Sld("amount",&g_Options.Exploits.Spawner.MoneyDropAmount,1,100,"k");
          c.Chk("drop money loop",&g_Options.Exploits.Spawner.DropMoney);
        }
    }

    // ═══════════════════════════════════════════════
    // TAB 4 — CONFIGS
    // ═══════════════════════════════════════════════
    else if (active_tab==4){
        Card c(dl,ImVec2(content_x+pad,content_y),content_w-pad*2,240,"cfg",dt,off_x);
        c.Header("configurations",nullptr,"save & load presets");
        c.Space(8);
        dl->AddText(ImVec2(c.pos.x+c.pad_x,c.pos.y+c.y),C_TEXT_DIM,"preset management coming in v1.4");
        c.y+=24;
        dl->AddText(ImVec2(c.pos.x+c.pad_x,c.pos.y+c.y),C_TEXT_HINT,"save, load, and share configs across sessions");
    }

    // ═══════════════════════════════════════════════
    // TAB 5 — SETTINGS
    // ═══════════════════════════════════════════════
    else if (active_tab==5){
        { Card c(dl,ImVec2(content_x+pad,content_y),cw,196,"general",dt,off_x);
          c.Header("general",nullptr,"core behavior");
          c.Chk("capture bypass",&g_Options.General.CaptureBypass);
          c.Chk("legit mode",&g_Options.Misc.Other.legit_mode);
          c.Chk("anti screenshot",&g_Options.Misc.Other.anti_screenshot);
          c.Sld("thread delay",&g_Options.General.ThreadDelay,0,32,"ms");
        }
        { Card c(dl,ImVec2(content_x+pad+cw+col_gap,content_y),cw,264,"menu_settings",dt,off_x);
          c.Header("menu",nullptr,"appearance");
          static int menu_scale_pct=100;
          menu_scale_pct=(int)(menu_scale*100.f+.5f);
          int prev_pct=menu_scale_pct;
          c.Sld("scale",&menu_scale_pct,60,150,"%");
          if (menu_scale_pct!=prev_pct) menu_scale=menu_scale_pct/100.f;
          c.Sld("opacity",&g_Options.General.MenuOpacity,50,100,"%");
          c.Section("accent color");
          c.ColorPicker("color",g_Options.General.AccentColor);
          c.Chk("pause game while open",&pause_game);
          c.Space(4);
        }
        { Card c(dl,ImVec2(content_x+pad,content_y+196+v_gap),cw*2+col_gap,124,"danger",dt,off_x);
          c.Header("danger zone",nullptr,"irreversible actions");
          c.Space(2);
          bool unload_clicked=false;
          c.Btn("UNLOAD CHEAT",&unload_clicked);
          if (unload_clicked){ should_unload=true; ShowToast("unloading cheat...",C_WARNING,2.f); }
        }
    }

    ImGui::PopStyleVar();

    // Toasts
    RenderToasts(fg,ImVec2(wp.x+ws.x-18,wp.y+ws.y-18),dt);

    ImGui::End();
    ImGui::PopStyleVar(); // opacity
}

} // namespace FrameWork
