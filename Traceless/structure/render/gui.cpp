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

static HWND g_hWindow = nullptr;
static WNDPROC g_OriginalWndProc = nullptr;
static bool g_bMenuOpen = false;

// When menu is closed, return HTTRANSPARENT to make clicks pass through to game/desktop.
// When menu is open, let the original window proc handle it normally.
extern "C" LRESULT CALLBACK SubclassedWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCHITTEST && !g_bMenuOpen) {
        return HTTRANSPARENT;
    }
    return CallWindowProc(g_OriginalWndProc, hWnd, msg, wParam, lParam);
}

void SetupWindowSubclass(HWND hWnd) {
    if (g_hWindow == nullptr) {
        g_hWindow = hWnd;
        g_OriginalWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)SubclassedWindowProc);
    }
}

namespace {

// ========================= STATE =========================

struct ToastMsg {
    std::string text;
    double      created;
    float       life;
    ImU32       col;
    float       anim;
};

std::map<std::string, bool>    section_open;
std::map<std::string, float>   section_anim;
std::map<std::string, float>   chk_anim;
std::map<std::string, float>   tog_anim;
std::map<std::string, float>   sld_hover_anim;
std::map<std::string, float>   hdr_hover_anim;
std::map<std::string, float>   kb_press_anim;
std::map<std::string, float>   cmb_hover_anim;
std::map<std::string, float>   btn_hover_anim;
std::map<std::string, float>   nav_hover_anim;
std::map<std::string, bool>    chk_prev_val;
std::map<std::string, double>  chk_last_change_time;
std::map<std::string, int>     sld_last_val;
std::map<std::string, double>  sld_last_change_time;
std::deque<ToastMsg>           toasts;

int   active_tab = 0;
int   prev_active_tab = 0;
float tab_content_anim = 1.0f;
int   card_render_index = 0;

constexpr int FPS_HIST_SIZE = 90;
float fps_history[FPS_HIST_SIZE] = {0};
int   fps_history_idx = 0;
float fps_smoothed = 0.0f;
float nav_pill_y = 0.0f;
float nav_pill_h = 0.0f;
bool  nav_pill_init = false;
float nav_pill_alpha = 0.0f;
float gui_fade_anim = 0.0f;

bool  key_listening = false;
int*  listening_key = nullptr;
bool  listening_wait_release = false;

float menu_scale = 1.0f;
bool  should_unload = false;
bool  pause_game = false;

bool  dragging = false;
ImVec2 drag_offset(0, 0);
bool  menu_visible = true;

// ========================= PALETTE =========================

const ImU32 C_BG_WIN       = IM_COL32(10, 11, 14, 245);
const ImU32 C_BG_SIDEBAR   = IM_COL32(14, 15, 19, 252);
const ImU32 C_BG_TOPBAR    = IM_COL32(13, 14, 18, 252);
const ImU32 C_BG_CONTENT   = IM_COL32(16, 17, 22, 240);
const ImU32 C_BG_CARD      = IM_COL32(22, 24, 30, 240);
const ImU32 C_BG_CARD_HOV  = IM_COL32(28, 30, 37, 248);
const ImU32 C_BG_CARD_BOT  = IM_COL32(18, 19, 24, 240);
const ImU32 C_BG_INPUT     = IM_COL32(15, 16, 21, 230);
const ImU32 C_BG_INPUT_HOV = IM_COL32(20, 22, 28, 240);
const ImU32 C_BG_PILL      = IM_COL32(32, 34, 41, 200);
const ImU32 C_BORDER       = IM_COL32(48, 51, 60, 100);
const ImU32 C_BORDER_SOFT  = IM_COL32(38, 40, 48, 60);
const ImU32 C_BORDER_BR    = IM_COL32(72, 76, 88, 130);
const ImU32 C_TEXT         = IM_COL32(210, 213, 220, 240);
const ImU32 C_TEXT_DIM     = IM_COL32(130, 134, 144, 220);
const ImU32 C_TEXT_HINT    = IM_COL32(92, 96, 107, 200);
const ImU32 C_TEXT_BRIGHT  = IM_COL32(238, 240, 245, 252);
const ImU32 C_WHITE        = IM_COL32(245, 246, 250, 255);
const ImU32 C_ACCENT       = IM_COL32(225, 70, 75, 255);
const ImU32 C_ACCENT_DIM   = IM_COL32(180, 50, 55, 220);
const ImU32 C_ACCENT_LIGHT = IM_COL32(240, 105, 110, 220);
const ImU32 C_ACCENT_GLOW  = IM_COL32(225, 70, 75, 90);
const ImU32 C_SUCCESS      = IM_COL32(85, 200, 140, 255);
const ImU32 C_WARNING      = IM_COL32(245, 185, 95, 255);

// ========================= MATH =========================

float Clamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

float SmoothLerp(float cur, float tgt, float speed, float dt) {
    if (dt > 0.1f) dt = 0.1f;
    float t = 1.0f - expf(-speed * dt);
    return cur + (tgt - cur) * Clamp01(t);
}

float EaseOutCubic(float t) { t = Clamp01(t); float f = t - 1.0f; return f * f * f + 1.0f; }
float EaseOutQuart(float t) { t = Clamp01(t); float f = t - 1.0f; return 1.0f - f * f * f * f; }
float EaseInOutCubic(float t) {
    t = Clamp01(t);
    if (t < 0.5f) return 4 * t * t * t;
    float f = -2 * t + 2;
    return 1.0f - (f * f * f) * 0.5f;
}
float EaseOutBack(float t) {
    t = Clamp01(t);
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    float f = t - 1.0f;
    return 1.0f + c3 * f * f * f + c1 * f * f;
}

ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    t = Clamp01(t);
    int ra = a & 0xFF, ga = (a >> 8) & 0xFF, ba2 = (a >> 16) & 0xFF, aa = (a >> 24) & 0xFF;
    int rb = b & 0xFF, gb = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF, ab = (b >> 24) & 0xFF;
    return IM_COL32(
        (int)(ra + (rb - ra) * t + 0.5f),
        (int)(ga + (gb - ga) * t + 0.5f),
        (int)(ba2 + (bb - ba2) * t + 0.5f),
        (int)(aa + (ab - aa) * t + 0.5f));
}

ImU32 ScaleAlpha(ImU32 c, float s) {
    int a = (int)(((c >> 24) & 0xFF) * Clamp01(s) + 0.5f);
    return (c & 0x00FFFFFF) | ((ImU32)a << 24);
}

float& AnimState(std::map<std::string, float>& m, const std::string& k, float def) {
    auto it = m.find(k);
    if (it == m.end()) it = m.emplace(k, def).first;
    return it->second;
}

void ShowToast(const std::string& msg, ImU32 col = C_ACCENT, float life = 2.4f) {
    if (!toasts.empty() && toasts.back().text == msg &&
        ImGui::GetTime() - toasts.back().created < 0.3) return;
    ToastMsg t;
    t.text = msg;
    t.created = ImGui::GetTime();
    t.life = life;
    t.col = col;
    t.anim = 0.0f;
    toasts.push_back(t);
    if (toasts.size() > 4) toasts.pop_front();
}

std::string KeyCodeToString(int key) {
    if (key == 0) return "none";
    switch (key) {
        case VK_LBUTTON: return "lmb"; case VK_RBUTTON: return "rmb"; case VK_MBUTTON: return "mmb";
        case VK_XBUTTON1: return "m4"; case VK_XBUTTON2: return "m5";
        case VK_SHIFT: return "shift"; case VK_CONTROL: return "ctrl"; case VK_MENU: return "alt";
        case VK_SPACE: return "space"; case VK_RETURN: return "enter"; case VK_ESCAPE: return "esc"; case VK_TAB: return "tab";
        default:
            if (key >= 'A' && key <= 'Z') return std::string(1, (char)(key + 32));
            if (key >= VK_F1 && key <= VK_F12) { char b[4]; sprintf_s(b, sizeof(b), "f%d", key - VK_F1 + 1); return std::string(b); }
            if (key >= '0' && key <= '9') return std::string(1, (char)key);
            return "key";
    }
}

// ========================= ICONS =========================

void DrawCrosshairIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c, 7, col, 22, 1.4f);
    dl->AddCircleFilled(c, 1.4f, col);
    dl->AddLine(ImVec2(c.x - 10, c.y), ImVec2(c.x - 3, c.y), col, 1.4f);
    dl->AddLine(ImVec2(c.x + 3, c.y), ImVec2(c.x + 10, c.y), col, 1.4f);
    dl->AddLine(ImVec2(c.x, c.y - 10), ImVec2(c.x, c.y - 3), col, 1.4f);
    dl->AddLine(ImVec2(c.x, c.y + 3), ImVec2(c.x, c.y + 10), col, 1.4f);
}

void DrawEyeIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddBezierCubic(ImVec2(c.x - 10, c.y), ImVec2(c.x - 5, c.y - 6), ImVec2(c.x + 5, c.y - 6), ImVec2(c.x + 10, c.y), col, 1.4f);
    dl->AddBezierCubic(ImVec2(c.x - 10, c.y), ImVec2(c.x - 5, c.y + 6), ImVec2(c.x + 5, c.y + 6), ImVec2(c.x + 10, c.y), col, 1.4f);
    dl->AddCircleFilled(c, 2.5f, col);
}

void DrawOverlayIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddRect(ImVec2(c.x - 8, c.y - 8), ImVec2(c.x + 8, c.y + 8), col, 2.0f, 0, 1.3f);
    dl->AddRect(ImVec2(c.x - 5, c.y - 5), ImVec2(c.x + 5, c.y + 5), ScaleAlpha(col, 0.6f), 1.5f, 0, 1.2f);
    dl->AddRectFilled(ImVec2(c.x - 1.5f, c.y - 1.5f), ImVec2(c.x + 1.5f, c.y + 1.5f), col, 0.5f);
}

void DrawBoltIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    ImVec2 pts[6] = {
        ImVec2(c.x - 1, c.y - 9),
        ImVec2(c.x - 7, c.y + 2),
        ImVec2(c.x - 1, c.y + 2),
        ImVec2(c.x + 1, c.y + 9),
        ImVec2(c.x + 7, c.y - 2),
        ImVec2(c.x + 1, c.y - 2),
    };
    dl->AddConvexPolyFilled(pts, 6, col);
}

void DrawListIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    for (int i = 0; i < 3; i++) {
        float y = c.y - 6 + i * 6;
        dl->AddCircleFilled(ImVec2(c.x - 7, y), 1.4f, col);
        dl->AddLine(ImVec2(c.x - 3, y), ImVec2(c.x + 8, y), col, 1.4f);
    }
}

void DrawSettingsIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c, 7.5f, col, 24, 1.4f);
    dl->AddCircle(c, 2.8f, col, 14, 1.3f);
    for (int i = 0; i < 8; i++) {
        float a = i * 6.2831f / 8 + 0.39f;
        dl->AddLine(ImVec2(c.x + cosf(a) * 7.5f, c.y + sinf(a) * 7.5f),
                    ImVec2(c.x + cosf(a) * 10.0f, c.y + sinf(a) * 10.0f), col, 1.4f);
    }
}

void DrawKeyboardIcon(ImDrawList* dl, ImVec2 p, ImU32 col) {
    dl->AddRect(p, ImVec2(p.x + 15, p.y + 10), col, 2.0f, 0, 1.0f);
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 4; c++) {
            float x = p.x + 2 + c * 3.1f, y = p.y + 2 + r * 3.0f;
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 1.6f, y + 1.6f), col);
        }
}

void DrawCloseIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddLine(ImVec2(c.x - 4, c.y - 4), ImVec2(c.x + 4, c.y + 4), col, 1.4f);
    dl->AddLine(ImVec2(c.x + 4, c.y - 4), ImVec2(c.x - 4, c.y + 4), col, 1.4f);
}

void DrawMinimizeIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddLine(ImVec2(c.x - 5, c.y + 3), ImVec2(c.x + 5, c.y + 3), col, 1.4f);
}

// ========================= EFFECTS =========================

void DrawShadow(ImDrawList* dl, ImVec2 pos, float w, float h, float intensity) {
    if (intensity < 0.01f) return;
    for (int i = 0; i < 5; i++) {
        float t = (float)i / 5.0f;
        ImU32 c = ScaleAlpha(IM_COL32(0, 0, 0, 80), intensity * (1.0f - t) * 0.5f);
        float off = (i + 1) * 1.6f;
        dl->AddRect(ImVec2(pos.x - off, pos.y - off + 1),
                    ImVec2(pos.x + w + off, pos.y + h + off + 1),
                    c, 12.0f + off * 0.5f, 0, 1.0f);
    }
}

void DrawAnimatedCheckmark(ImDrawList* dl, ImVec2 cp, float sz, float ca) {
    if (ca < 0.02f) return;
    float pad = sz * 0.167f;
    dl->AddRectFilled(ImVec2(cp.x + pad, cp.y + pad), ImVec2(cp.x + sz - pad, cp.y + sz - pad),
                      ScaleAlpha(C_ACCENT, ca), 2.0f);
    ImU32 mk = ScaleAlpha(C_WHITE, ca);
    float prog = EaseOutCubic(ca);
    ImVec2 a0(cp.x + sz * 0.21f, cp.y + sz * 0.50f);
    ImVec2 a1(cp.x + sz * 0.42f, cp.y + sz * 0.71f);
    ImVec2 a2(cp.x + sz * 0.79f, cp.y + sz * 0.29f);
    float s1 = prog < 0.4f ? prog / 0.4f : 1.0f;
    float s2 = prog < 0.4f ? 0.0f : (prog - 0.4f) / 0.6f;
    ImVec2 e1(a0.x + (a1.x - a0.x) * s1, a0.y + (a1.y - a0.y) * s1);
    dl->AddLine(a0, e1, mk, 1.6f);
    if (s2 > 0) {
        ImVec2 e2(a1.x + (a2.x - a1.x) * s2, a1.y + (a2.y - a1.y) * s2);
        dl->AddLine(a1, e2, mk, 1.6f);
    }
}

void DrawStatusDot(ImDrawList* dl, ImVec2 pos, ImU32 col, float pulse) {
    float phase = fmodf(pulse, 1.0f);
    float p = (sinf(phase * 6.2831f) * 0.5f + 0.5f);
    dl->AddCircleFilled(pos, 6.0f + p * 1.2f, ScaleAlpha(col, 0.15f + p * 0.15f));
    dl->AddCircleFilled(pos, 3.5f, col);
    dl->AddCircleFilled(pos, 1.5f, C_WHITE);
}

void DrawPulsingDot(ImDrawList* dl, ImVec2 pos, float pulse, ImU32 col) {
    float phase = fmodf(pulse, 1.0f);
    float s = sinf(phase * 6.2831f);
    dl->AddCircleFilled(pos, 3.0f + s * 1.4f, ScaleAlpha(col, 0.7f + s * 0.3f));
}

void DrawPill(ImDrawList* dl, ImVec2 pos, float w, float h, ImU32 bg, ImU32 border = 0) {
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), bg, h * 0.5f);
    if (border) dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), border, h * 0.5f, 0, 1.0f);
}

// ========================= CARD =========================

struct Card {
    ImDrawList* dl;
    ImVec2 pos;
    float w, h, h_target, y, dt;
    const char* id;
    bool open;
    float pad_x, anim_progress, hover_amount;
    bool clip_pushed;
    bool has_master;
    bool master_on;
    float master_alpha;

    static constexpr float HEADER_H = 46.0f;
    static constexpr float CHK_STEP = 28.0f;
    static constexpr float KB_STEP  = 30.0f;
    static constexpr float CMB_STEP = 50.0f;
    static constexpr float SLD_STEP = 40.0f;
    static constexpr float BTN_H    = 30.0f;
    static constexpr float SEC_STEP = 22.0f;
    float entry_eased;

    Card(ImDrawList* d, ImVec2 p, float width, float full_h, const char* sid, float dtime, float off_x = 0.0f)
        : dl(d), pos(ImVec2(p.x + off_x, p.y)), w(width), h_target(full_h), y(0), id(sid),
          pad_x(18.0f), dt(dtime), clip_pushed(false), hover_amount(0),
          has_master(false), master_on(false), master_alpha(1.0f), entry_eased(1.0f) {

        // Per-card stagger entry (each card delayed slightly behind the previous)
        int my_idx = card_render_index++;
        float delay = my_idx * 0.07f;
        if (delay > 0.5f) delay = 0.5f;
        float entry = Clamp01((tab_content_anim - delay) / (1.0f - delay));
        entry_eased = EaseOutCubic(entry);
        pos.y += (1.0f - entry_eased) * 16.0f;
        pos.x += (1.0f - entry_eased) * (off_x > 0 ? 14.0f : -2.0f);

        auto it = section_open.find(sid);
        if (it == section_open.end()) { section_open[sid] = true; open = true; }
        else { open = it->second; }

        float& a = AnimState(section_anim, sid, open ? 1.0f : 0.0f);
        a = SmoothLerp(a, open ? 1.0f : 0.0f, 16.0f, dt);
        anim_progress = a;

        float min_h = HEADER_H + 4;
        h = min_h + (h_target - min_h) * EaseOutCubic(anim_progress);

        bool hovered = ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + w, pos.y + h));
        float& hv = AnimState(hdr_hover_anim, std::string("c_") + sid, 0.0f);
        hv = SmoothLerp(hv, hovered ? 1.0f : 0.0f, 18.0f, dt);
        hover_amount = hv;

        float lift = hv * 1.8f;
        pos.y -= lift;

        float content_fade = entry_eased;
        DrawShadow(dl, pos, w, h, (0.25f + hv * 0.4f) * content_fade);

        ImU32 bg_top = LerpColor(C_BG_CARD, C_BG_CARD_HOV, hv);
        ImU32 bg_bot = LerpColor(C_BG_CARD_BOT, C_BG_CARD, hv * 0.6f);
        dl->AddRectFilledMultiColor(pos, ImVec2(pos.x + w, pos.y + h), bg_top, bg_top, bg_bot, bg_bot);

        ImU32 br = LerpColor(C_BORDER, C_BORDER_BR, hv * 0.7f);
        dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), br, 10.0f, 0, 1.0f);

        ImU32 sheen = ScaleAlpha(C_ACCENT, hv * 0.06f);
        dl->AddRectFilledMultiColor(pos, ImVec2(pos.x + w, pos.y + 24),
                                    sheen, sheen, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));

        dl->AddLine(ImVec2(pos.x + 1, pos.y + 1), ImVec2(pos.x + w - 1, pos.y + 1),
                    ScaleAlpha(C_WHITE, 0.04f), 1.0f);

        dl->PushClipRect(pos, ImVec2(pos.x + w, pos.y + h), true);
        clip_pushed = true;
    }

    ~Card() { if (clip_pushed) dl->PopClipRect(); }

    void Header(const char* label, bool* master = nullptr, const char* desc = nullptr) {
        ImU32 txt_col = LerpColor(C_TEXT_BRIGHT, C_WHITE, hover_amount * 0.3f);
        dl->AddText(ImVec2(pos.x + pad_x, pos.y + 13), txt_col, label);

        ImVec2 lbl_size = ImGui::CalcTextSize(label);
        if (desc) {
            dl->AddText(ImVec2(pos.x + pad_x + lbl_size.x + 10, pos.y + 14), C_TEXT_HINT, desc);
        }

        float right_x = pos.x + w - pad_x;
        float tw = 30, th = 15;
        ImVec2 tp(right_x - tw - 22, pos.y + 14);

        if (master) {
            has_master = true;
            master_on = *master;

            std::string tk = std::string(id) + "_mt";
            float& ta = AnimState(tog_anim, tk, *master ? 1.0f : 0.0f);
            ta = SmoothLerp(ta, *master ? 1.0f : 0.0f, 22.0f, dt);

            ImU32 fill = LerpColor(C_BG_INPUT, C_ACCENT, ta);
            dl->AddRectFilled(tp, ImVec2(tp.x + tw, tp.y + th), fill, th * 0.5f);
            dl->AddRect(tp, ImVec2(tp.x + tw, tp.y + th),
                        LerpColor(C_BORDER, C_ACCENT, ta * 0.5f), th * 0.5f, 0, 1.0f);

            if (ta > 0.3f) {
                ImU32 glow = ScaleAlpha(C_ACCENT_GLOW, (ta - 0.3f) * 0.8f);
                dl->AddRect(ImVec2(tp.x - 2, tp.y - 2), ImVec2(tp.x + tw + 2, tp.y + th + 2),
                            glow, th * 0.5f + 2, 0, 1.5f);
            }

            float kr = (th - 4) * 0.5f;
            float kx = tp.x + 2 + kr + (tw - 4 - kr * 2) * EaseOutBack(ta);
            dl->AddCircleFilled(ImVec2(kx, tp.y + th * 0.5f), kr, C_WHITE);
            if (ta > 0.4f) {
                dl->AddCircleFilled(ImVec2(kx, tp.y + th * 0.5f), kr - 1.5f, ScaleAlpha(C_ACCENT, (ta - 0.4f) * 0.3f));
            }

            master_alpha = 0.45f + ta * 0.55f;
        }

        ImVec2 cv(right_x - 2, pos.y + 20);
        float rot = (anim_progress - 1.0f) * 1.5708f;
        float cr = cosf(rot), sr = sinf(rot);
        auto rp = [&](float dx, float dy) {
            return ImVec2(cv.x + dx * cr - dy * sr, cv.y + dx * sr + dy * cr);
        };
        ImU32 chev_col = LerpColor(C_TEXT_DIM, C_ACCENT, anim_progress * 0.4f + hover_amount * 0.3f);
        dl->AddTriangleFilled(rp(-4, -2), rp(4, -2), rp(0, 3), chev_col);

        if (anim_progress > 0.3f) {
            float la = (anim_progress - 0.3f) / 0.7f;
            float lx0 = pos.x + pad_x, lx1 = pos.x + w - pad_x;
            float ly = pos.y + HEADER_H - 4;
            int seg = 24;
            for (int i = 0; i < seg; i++) {
                float t0 = i / (float)seg, t1 = (i + 1) / (float)seg;
                float fade = sinf((t0 + t1) * 0.5f * 3.1415f);
                ImU32 c = ScaleAlpha(C_ACCENT, la * 0.22f * fade);
                dl->AddLine(ImVec2(lx0 + (lx1 - lx0) * t0, ly), ImVec2(lx0 + (lx1 - lx0) * t1, ly), c, 1.0f);
            }
        }

        // Header click first (full width), master toggle click last so it wins in overlap
        ImGui::SetCursorScreenPos(pos);
        ImGui::PushID(id);
        ImGui::InvisibleButton("##hdr", ImVec2(w, HEADER_H));
        if (ImGui::IsItemClicked()) {
            section_open[id] = !section_open[id];
            open = section_open[id];
        }
        ImGui::PopID();

        if (master) {
            ImGui::SetCursorScreenPos(tp);
            ImGui::PushID(id);
            ImGui::InvisibleButton("##mt", ImVec2(tw, th));
            if (ImGui::IsItemClicked()) {
                *master = !*master;
                ShowToast(std::string(label) + (*master ? " enabled" : " disabled"),
                          *master ? C_SUCCESS : C_ACCENT_DIM);
            }
            ImGui::PopID();
        }
        y = HEADER_H + 4;
    }

    bool Chk(const char* label, bool* v) {
        if (anim_progress < 0.01f) { y += CHK_STEP; return false; }

        std::string key = std::string(id) + "_chk_" + label;
        float& ca = AnimState(chk_anim, key, *v ? 1.0f : 0.0f);
        ca = SmoothLerp(ca, *v ? 1.0f : 0.0f, 24.0f, dt);

        auto prev_it = chk_prev_val.find(key);
        if (prev_it == chk_prev_val.end()) chk_prev_val[key] = *v;
        else if (prev_it->second != *v) { chk_last_change_time[key] = ImGui::GetTime(); prev_it->second = *v; }

        auto ct_it = chk_last_change_time.find(key);
        double ts = ct_it != chk_last_change_time.end() ? ImGui::GetTime() - ct_it->second : 10.0;
        float hl = Clamp01(1.0f - (float)(ts / 0.4));

        std::string hk = std::string(id) + "_chk_h_" + label;
        ImVec2 row_min(pos.x + pad_x - 6, pos.y + y - 1);
        ImVec2 row_max(pos.x + w - pad_x + 6, pos.y + y + CHK_STEP - 3);
        bool row_hov = ImGui::IsMouseHoveringRect(row_min, row_max);
        float& rh = AnimState(item_hover_anim_get(hk), hk, 0.0f);
        rh = SmoothLerp(rh, row_hov ? 1.0f : 0.0f, 20.0f, dt);
        if (rh > 0.01f) {
            dl->AddRectFilled(row_min, row_max, ScaleAlpha(C_WHITE, rh * 0.025f), 4.0f);
        }

        float sz = 12.0f;
        ImVec2 cp(pos.x + pad_x, pos.y + y + 3);
        ImU32 brd = LerpColor(C_BORDER, C_ACCENT, ca);
        dl->AddRectFilled(cp, ImVec2(cp.x + sz, cp.y + sz), ScaleAlpha(C_BG_INPUT, 1.0f - ca), 3.0f);
        dl->AddRect(cp, ImVec2(cp.x + sz, cp.y + sz), brd, 3.0f, 0, 1.0f);

        if (hl > 0.1f) {
            dl->AddRect(ImVec2(cp.x - 2, cp.y - 2), ImVec2(cp.x + sz + 2, cp.y + sz + 2),
                        ScaleAlpha(C_ACCENT, hl * 0.4f), 3.0f, 0, 1.0f);
        }

        DrawAnimatedCheckmark(dl, cp, sz, ca);
        ImU32 lbl_col = LerpColor(C_TEXT, C_ACCENT_LIGHT, hl * 0.3f);
        lbl_col = ScaleAlpha(lbl_col, master_alpha);
        dl->AddText(ImVec2(cp.x + sz + 8, cp.y - 2), lbl_col, label);

        if (anim_progress > 0.6f) {
            ImGui::SetCursorScreenPos(row_min);
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##chk", ImVec2(row_max.x - row_min.x, row_max.y - row_min.y));
            if (ImGui::IsItemClicked()) *v = !*v;
            ImGui::PopID(); ImGui::PopID();
        }
        y += CHK_STEP;
        return true;
    }

    // Tiny helper so we can use a per-row hover anim map
    std::map<std::string, float>& item_hover_anim_get(const std::string&) {
        static std::map<std::string, float> m;
        return m;
    }

    bool ChkTog(const char* label, bool* v, bool* tog, ImU32 tog_color = 0) {
        if (anim_progress < 0.01f) { y += CHK_STEP; return false; }
        std::string ck = std::string(id) + "_ct_" + label;
        float& ca = AnimState(chk_anim, ck, *v ? 1.0f : 0.0f);
        ca = SmoothLerp(ca, *v ? 1.0f : 0.0f, 24.0f, dt);

        float sz = 12.0f;
        ImVec2 cp(pos.x + pad_x, pos.y + y + 3);
        dl->AddRectFilled(cp, ImVec2(cp.x + sz, cp.y + sz), ScaleAlpha(C_BG_INPUT, 1.0f - ca), 3.0f);
        dl->AddRect(cp, ImVec2(cp.x + sz, cp.y + sz), LerpColor(C_BORDER, C_ACCENT, ca), 3.0f, 0, 1.0f);
        DrawAnimatedCheckmark(dl, cp, sz, ca);
        dl->AddText(ImVec2(cp.x + sz + 8, cp.y - 2), ScaleAlpha(C_TEXT, master_alpha), label);

        std::string tk = std::string(id) + "_tg_" + label;
        float& ta = AnimState(tog_anim, tk, *tog ? 1.0f : 0.0f);
        ta = SmoothLerp(ta, *tog ? 1.0f : 0.0f, 22.0f, dt);

        float tw = 26, th = 13;
        ImVec2 tp(pos.x + w - pad_x - tw, pos.y + y + 3);
        ImU32 tcol = tog_color ? tog_color : C_ACCENT;
        ImU32 fill = LerpColor(C_BG_INPUT, tcol, ta);
        dl->AddRectFilled(tp, ImVec2(tp.x + tw, tp.y + th), fill, th * 0.5f);
        dl->AddRect(tp, ImVec2(tp.x + tw, tp.y + th), LerpColor(C_BORDER, tcol, ta * 0.5f), th * 0.5f, 0, 1.0f);

        float kr = (th - 4) * 0.5f;
        float kx = tp.x + 2 + kr + (tw - 4 - kr * 2) * EaseOutBack(ta);
        dl->AddCircleFilled(ImVec2(kx, tp.y + th * 0.5f), kr, C_WHITE);

        if (anim_progress > 0.6f) {
            ImGui::SetCursorScreenPos(ImVec2(pos.x + pad_x - 4, pos.y + y));
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##ct_c", ImVec2(w - pad_x * 2 - tw - 6, CHK_STEP - 2));
            if (ImGui::IsItemClicked()) *v = !*v;
            ImGui::SetCursorScreenPos(tp);
            ImGui::InvisibleButton("##ct_t", ImVec2(tw, th));
            if (ImGui::IsItemClicked()) *tog = !*tog;
            ImGui::PopID(); ImGui::PopID();
        }
        y += CHK_STEP;
        return true;
    }

    void KB(const char* label, int* key) {
        bool is_listening = (key_listening && listening_key == key);
        if (is_listening) {
            if (listening_wait_release) {
                bool any = false;
                for (int i = 1; i < 256; i++) if (GetAsyncKeyState(i) & 0x8000) { any = true; break; }
                if (!any) listening_wait_release = false;
            } else {
                for (int i = 1; i < 256; i++) {
                    if (GetAsyncKeyState(i) & 0x8000) {
                        *key = (i == VK_ESCAPE) ? 0 : i;
                        ShowToast("keybind set: " + KeyCodeToString(*key));
                        key_listening = false;
                        listening_key = nullptr;
                        is_listening = false;
                        break;
                    }
                }
            }
        }

        if (anim_progress < 0.01f) { y += KB_STEP; return; }
        dl->AddText(ImVec2(pos.x + pad_x, pos.y + y + 4), ScaleAlpha(C_TEXT, master_alpha), label);

        float bw = 76, bh = 20;
        ImVec2 bp(pos.x + w - bw - pad_x, pos.y + y + 1);

        std::string kk = std::string(id) + "_kb_" + label;
        float& kp = AnimState(kb_press_anim, kk, 0.0f);
        kp = SmoothLerp(kp, is_listening ? 1.0f : 0.0f, 20.0f, dt);

        ImU32 bg = LerpColor(C_BG_INPUT, ScaleAlpha(C_ACCENT, 0.35f), kp);
        ImU32 brd = LerpColor(C_BORDER, C_ACCENT, kp);
        dl->AddRectFilled(bp, ImVec2(bp.x + bw, bp.y + bh), bg, 4.0f);
        dl->AddRect(bp, ImVec2(bp.x + bw, bp.y + bh), brd, 4.0f, 0, 1.0f);

        DrawKeyboardIcon(dl, ImVec2(bp.x + 6, bp.y + 5), is_listening ? C_ACCENT : C_TEXT_DIM);
        std::string s = is_listening ? "..." : KeyCodeToString(*key);
        ImVec2 ts = ImGui::CalcTextSize(s.c_str());
        dl->AddText(ImVec2(bp.x + 27, bp.y + (bh - ts.y) * 0.5f), is_listening ? C_ACCENT : C_TEXT, s.c_str());

        if (is_listening) {
            DrawPulsingDot(dl, ImVec2(bp.x + bw - 7, bp.y + bh * 0.5f),
                           fmodf((float)ImGui::GetTime() * 2.0f, 1.0f), C_ACCENT);
        }

        if (anim_progress > 0.6f) {
            ImGui::SetCursorScreenPos(bp);
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##kb", ImVec2(bw, bh));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !is_listening) {
                *key = 0;
                ShowToast(std::string(label) + " cleared", C_WARNING);
            } else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                key_listening = true;
                listening_key = key;
                listening_wait_release = true;
                ShowToast("press any key (esc=clear)", C_TEXT_DIM, 1.5f);
            }
            ImGui::PopID(); ImGui::PopID();
        }
        y += KB_STEP;
    }

    void Combo(const char* label, int* v, const std::vector<std::string>& items) {
        if (anim_progress < 0.01f) { y += CMB_STEP; return; }
        dl->AddText(ImVec2(pos.x + pad_x, pos.y + y), ScaleAlpha(C_TEXT, master_alpha), label);
        y += 17;
        ImVec2 bp(pos.x + pad_x, pos.y + y);
        float bw = w - pad_x * 2, bh = 26;
        int count = (int)items.size();

        std::string ck = std::string(id) + "_cmb_" + label;
        float& hv = AnimState(cmb_hover_anim, ck, 0.0f);
        bool bhov = ImGui::IsMouseHoveringRect(bp, ImVec2(bp.x + bw, bp.y + bh));
        hv = SmoothLerp(hv, bhov ? 1.0f : 0.0f, 20.0f, dt);

        ImU32 bg = LerpColor(C_BG_INPUT, C_BG_INPUT_HOV, hv);
        dl->AddRectFilled(bp, ImVec2(bp.x + bw, bp.y + bh), bg, 4.0f);
        ImU32 br = LerpColor(C_BORDER, ScaleAlpha(C_ACCENT, 0.6f), hv * 0.7f);
        dl->AddRect(bp, ImVec2(bp.x + bw, bp.y + bh), br, 4.0f, 0, 1.0f);

        if (*v >= 0 && *v < count) {
            ImU32 txt_col = LerpColor(C_TEXT, C_ACCENT_LIGHT, hv * 0.3f);
            dl->AddText(ImVec2(bp.x + 12, bp.y + 6), txt_col, items[*v].c_str());
        }

        ImU32 arr_col = LerpColor(C_TEXT_DIM, C_ACCENT, hv * 0.5f);
        float ay = bp.y + bh * 0.5f;
        dl->AddTriangleFilled(ImVec2(bp.x + bw - 22, ay - 3), ImVec2(bp.x + bw - 22, ay + 3), ImVec2(bp.x + bw - 26, ay), arr_col);
        dl->AddTriangleFilled(ImVec2(bp.x + bw - 12, ay - 3), ImVec2(bp.x + bw - 12, ay + 3), ImVec2(bp.x + bw - 8, ay), arr_col);

        if (count > 1) {
            float pip_w = (bw - 24) / (float)count;
            for (int i = 0; i < count; i++) {
                float pip_x = bp.x + 12 + i * pip_w + pip_w * 0.5f - 1;
                ImU32 pc = (i == *v) ? C_ACCENT : ScaleAlpha(C_BORDER, 0.5f);
                dl->AddRectFilled(ImVec2(pip_x, bp.y + bh - 4), ImVec2(pip_x + 2, bp.y + bh - 2), pc);
            }
        }

        if (anim_progress > 0.6f && count > 0) {
            ImGui::SetCursorScreenPos(bp);
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##cmb", ImVec2(bw, bh));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) *v = (*v + 1) % count;
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) *v = (*v - 1 + count) % count;
            if (ImGui::IsItemHovered()) {
                float wheel = ImGui::GetIO().MouseWheel;
                if (wheel > 0) *v = (*v - 1 + count) % count;
                else if (wheel < 0) *v = (*v + 1) % count;
            }
            ImGui::PopID(); ImGui::PopID();
        }
        y += bh + 7;
    }

    void Sld(const char* label, int* v, int vmin, int vmax, const char* suffix = "") {
        if (anim_progress < 0.01f) { y += SLD_STEP; return; }
        std::string sk = std::string(id) + "_sld_" + label;
        float range = (float)(vmax - vmin);

        auto sv_it = sld_last_val.find(sk);
        if (sv_it == sld_last_val.end()) sld_last_val[sk] = *v;
        else if (sv_it->second != *v) { sv_it->second = *v; sld_last_change_time[sk] = ImGui::GetTime(); }

        auto ct_it = sld_last_change_time.find(sk);
        double ts = ct_it != sld_last_change_time.end() ? ImGui::GetTime() - ct_it->second : 10.0;
        float hl = Clamp01(1.0f - (float)(ts / 0.5));

        dl->AddText(ImVec2(pos.x + pad_x, pos.y + y), ScaleAlpha(C_TEXT, master_alpha), label);
        char val_str[32];
        sprintf_s(val_str, "%d%s", *v, suffix);
        ImVec2 vs = ImGui::CalcTextSize(val_str);

        float vpad = 8, vh = vs.y + 2;
        ImVec2 vp(pos.x + w - vs.x - vpad * 2 - pad_x, pos.y + y - 1);
        ImU32 vbg = LerpColor(C_BG_PILL, ScaleAlpha(C_ACCENT, 0.4f), hl * 0.6f);
        DrawPill(dl, vp, vs.x + vpad * 2, vh, vbg);
        ImU32 val_col = LerpColor(C_TEXT_BRIGHT, C_WHITE, hl * 0.5f);
        dl->AddText(ImVec2(vp.x + vpad, vp.y + 1), val_col, val_str);

        y += 18;
        float th = 4;
        ImVec2 tp(pos.x + pad_x, pos.y + y + 3);
        float tw = w - pad_x * 2;
        dl->AddRectFilled(tp, ImVec2(tp.x + tw, tp.y + th), C_BG_INPUT, 2.0f);

        float r = (range > 0) ? Clamp01((float)(*v - vmin) / range) : 0.0f;
        if (tw * r > 1.0f)
            dl->AddRectFilledMultiColor(tp, ImVec2(tp.x + tw * r, tp.y + th),
                                        C_ACCENT_DIM, C_ACCENT, C_ACCENT, C_ACCENT_DIM);

        bool hovered = ImGui::IsMouseHoveringRect(ImVec2(tp.x, tp.y - 8), ImVec2(tp.x + tw, tp.y + 12));
        float& sa = AnimState(sld_hover_anim, sk, 1.0f);
        sa = SmoothLerp(sa, hovered ? 1.4f : 1.0f, 22.0f, dt);
        float knob = 5.5f * sa;
        float kx = tp.x + tw * r;
        float ky = tp.y + th * 0.5f;

        if (sa > 1.05f) {
            float g = (sa - 1.0f) / 0.4f;
            dl->AddCircleFilled(ImVec2(kx, ky), knob + 4, ScaleAlpha(C_ACCENT, g * 0.3f));
            dl->AddCircleFilled(ImVec2(kx, ky), knob + 2, ScaleAlpha(C_ACCENT, g * 0.4f));
        }
        dl->AddCircleFilled(ImVec2(kx, ky), knob, C_WHITE);
        dl->AddCircleFilled(ImVec2(kx, ky), knob - 1.5f, ScaleAlpha(C_ACCENT, hl * 0.5f));

        if (anim_progress > 0.6f) {
            ImGui::SetCursorScreenPos(ImVec2(tp.x, tp.y - 8));
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##sld", ImVec2(tw, 16));
            if (ImGui::IsItemActive()) {
                float mx = ImGui::GetIO().MousePos.x;
                *v = (int)(vmin + Clamp01((mx - tp.x) / tw) * range + 0.5f);
            }
            if (ImGui::IsItemHovered() && range > 0) {
                float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f) {
                    int step = (vmax - vmin > 100) ? 5 : 1;
                    *v += (int)(wheel) * step;
                    if (*v < vmin) *v = vmin;
                    if (*v > vmax) *v = vmax;
                }
            }
            ImGui::PopID(); ImGui::PopID();
        }
        y += SLD_STEP - 18;
    }

    void Btn(const char* label, bool* clicked_out, ImU32 btn_bg = 0, ImU32 btn_brd = 0) {
        if (anim_progress < 0.01f) { y += BTN_H + 4; return; }
        ImVec2 bp(pos.x + pad_x, pos.y + y);
        float bw = w - pad_x * 2;

        std::string bk = std::string(id) + "_btn_" + label;
        float& hv = AnimState(btn_hover_anim, bk, 0.0f);
        bool bhov = ImGui::IsMouseHoveringRect(bp, ImVec2(bp.x + bw, bp.y + BTN_H));
        hv = SmoothLerp(hv, bhov ? 1.0f : 0.0f, 22.0f, dt);

        ImU32 bg = btn_bg ? btn_bg : IM_COL32(180, 50, 55, 220);
        ImU32 border = btn_brd ? btn_brd : C_ACCENT_LIGHT;
        ImU32 bg_top = LerpColor(bg, ScaleAlpha(C_ACCENT, 0.85f), hv * 0.4f);
        ImU32 bg_bot = LerpColor(ScaleAlpha(bg, 0.85f), bg, hv * 0.5f);
        ImU32 brd = LerpColor(border, C_ACCENT_LIGHT, hv * 0.4f);

        dl->AddRectFilledMultiColor(bp, ImVec2(bp.x + bw, bp.y + BTN_H), bg_top, bg_top, bg_bot, bg_bot);
        dl->AddRect(bp, ImVec2(bp.x + bw, bp.y + BTN_H), brd, 5.0f, 0, 1.0f);

        if (hv > 0.05f) {
            dl->AddRect(ImVec2(bp.x - 1, bp.y - 1), ImVec2(bp.x + bw + 1, bp.y + BTN_H + 1),
                        ScaleAlpha(border, hv * 0.4f), 6.0f, 0, 1.5f);
        }

        ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(bp.x + (bw - ts.x) * 0.5f, bp.y + (BTN_H - ts.y) * 0.5f), C_WHITE, label);

        *clicked_out = false;
        if (anim_progress > 0.6f) {
            ImGui::SetCursorScreenPos(bp);
            ImGui::PushID(id); ImGui::PushID(label);
            ImGui::InvisibleButton("##btn", ImVec2(bw, BTN_H));
            if (ImGui::IsItemClicked()) *clicked_out = true;
            ImGui::PopID(); ImGui::PopID();
        }
        y += BTN_H + 4;
    }

    void Section(const char* label) {
        if (anim_progress < 0.5f) { y += SEC_STEP; return; }
        float a = (anim_progress - 0.5f) / 0.5f;
        ImU32 col = ScaleAlpha(C_TEXT_HINT, a);
        dl->AddText(ImVec2(pos.x + pad_x, pos.y + y + 4), col, label);
        ImVec2 ls = ImGui::CalcTextSize(label);
        float lx0 = pos.x + pad_x + ls.x + 10;
        float lx1 = pos.x + w - pad_x;
        float ly = pos.y + y + ls.y * 0.5f + 4;
        dl->AddLine(ImVec2(lx0, ly), ImVec2(lx1, ly), ScaleAlpha(C_BORDER, a * 0.7f), 1.0f);
        y += SEC_STEP;
    }

    void Space(float px) { y += px; }
};

// ========================= NAV ITEM =========================

typedef void (*IconFn)(ImDrawList*, ImVec2, ImU32);

struct NavSlot {
    const char* label;
    IconFn icon;
};

const NavSlot NAV[] = {
    {"aimbot",   DrawCrosshairIcon},
    {"visuals",  DrawEyeIcon},
    {"overlays", DrawOverlayIcon},
    {"exploits", DrawBoltIcon},
    {"configs",  DrawListIcon},
    {"settings", DrawSettingsIcon},
};
const int NAV_COUNT = sizeof(NAV) / sizeof(NAV[0]);

bool RenderNavItem(ImDrawList* dl, ImVec2 origin, int idx, bool active, float dt) {
    float w = 200 - 24, h = 38;
    ImVec2 pos(origin.x + 12, origin.y);

    bool hov = ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + w, pos.y + h));
    std::string key = std::string("nav_") + NAV[idx].label;
    float& ha = AnimState(nav_hover_anim, key, 0.0f);
    ha = SmoothLerp(ha, (hov || active) ? 1.0f : 0.0f, 22.0f, dt);

    if (ha > 0.01f) {
        ImU32 bg = ScaleAlpha(C_WHITE, ha * 0.04f);
        if (active) bg = LerpColor(bg, ScaleAlpha(C_ACCENT, 0.12f), 1.0f);
        dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), bg, 6.0f);
    }

    ImU32 ic_col = LerpColor(C_TEXT_DIM, C_ACCENT, active ? 1.0f : ha * 0.4f);
    NAV[idx].icon(dl, ImVec2(pos.x + 22, pos.y + h * 0.5f), ic_col);

    ImU32 txt_col = LerpColor(C_TEXT_DIM, C_TEXT_BRIGHT, active ? 1.0f : ha * 0.7f);
    if (active) txt_col = C_WHITE;
    ImVec2 ts = ImGui::CalcTextSize(NAV[idx].label);
    dl->AddText(ImVec2(pos.x + 44, pos.y + (h - ts.y) * 0.5f), txt_col, NAV[idx].label);

    if (active) {
        float dot_x = pos.x + w - 12;
        float dot_y = pos.y + h * 0.5f;
        dl->AddCircleFilled(ImVec2(dot_x, dot_y), 2.5f, C_ACCENT);
    }

    ImGui::SetCursorScreenPos(pos);
    ImGui::PushID(idx);
    ImGui::InvisibleButton("##nav", ImVec2(w, h));
    bool clicked = ImGui::IsItemClicked();
    ImGui::PopID();
    return clicked;
}

void RenderToasts(ImDrawList* dl_fg, ImVec2 anchor, float dt) {
    double now = ImGui::GetTime();
    for (auto it = toasts.begin(); it != toasts.end(); ) {
        double age = now - it->created;
        if (age > it->life) { it = toasts.erase(it); continue; }
        ++it;
    }

    float y = anchor.y;
    for (auto it = toasts.rbegin(); it != toasts.rend(); ++it) {
        double age = now - it->created;
        float fade_in = Clamp01((float)(age / 0.25));
        float fade_out = Clamp01((float)((it->life - age) / 0.4));
        float a = fade_in * fade_out;
        it->anim = SmoothLerp(it->anim, 1.0f, 20.0f, dt);

        ImVec2 ts = ImGui::CalcTextSize(it->text.c_str());
        float pad_x = 14, pad_y = 8;
        float w = ts.x + pad_x * 2 + 24;
        float h = ts.y + pad_y * 2;
        ImVec2 pos(anchor.x - w, y - h);
        pos.x += (1.0f - EaseOutCubic(it->anim)) * 30.0f;

        DrawShadow(dl_fg, pos, w, h, 0.4f * a);
        dl_fg->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), ScaleAlpha(IM_COL32(25, 27, 33, 250), a), 8.0f);
        dl_fg->AddRect(pos, ImVec2(pos.x + w, pos.y + h), ScaleAlpha(C_BORDER_BR, a * 0.8f), 8.0f, 0, 1.0f);
        dl_fg->AddRectFilled(pos, ImVec2(pos.x + 3, pos.y + h), ScaleAlpha(it->col, a), 1.5f);

        DrawStatusDot(dl_fg, ImVec2(pos.x + 16, pos.y + h * 0.5f), it->col, (float)now);
        dl_fg->AddText(ImVec2(pos.x + 30, pos.y + pad_y), ScaleAlpha(C_TEXT_BRIGHT, a), it->text.c_str());

        y -= h + 8;
    }
}

} // anonymous namespace

void ApplyDarkTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 12; s.FrameRounding = 5; s.GrabRounding = 5; s.PopupRounding = 5; s.ChildRounding = 8;
    s.WindowPadding = ImVec2(0, 0); s.FramePadding = ImVec2(4, 3);
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    s.Colors[ImGuiCol_ChildBg]  = ImVec4(0, 0, 0, 0);
    s.Colors[ImGuiCol_Border]   = ImVec4(0, 0, 0, 0);
}

void RenderGui() {
    ApplyDarkTheme();

    // Set up window subclass once
    static bool subclass_setup = false;
    if (!subclass_setup && GetActiveWindow()) {
        SetupWindowSubclass(GetActiveWindow());
        subclass_setup = true;
    }

    // Handle INSERT key to toggle menu
    static bool insert_pressed_last = false;
    bool insert_pressed = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (insert_pressed && !insert_pressed_last) {
        menu_visible = !menu_visible;
        if (menu_visible) {
            gui_fade_anim = 0.0f;
        }
    }
    insert_pressed_last = insert_pressed;

    // Sync the global for window subclass
    g_bMenuOpen = menu_visible;

    float dt = ImGui::GetIO().DeltaTime;
    if (dt > 0.1f) dt = 0.1f;
    gui_fade_anim = SmoothLerp(gui_fade_anim, menu_visible ? 1.0f : 0.0f, 8.0f, dt);

    // FPS history (one sample per frame, smoothed)
    float cur_fps = ImGui::GetIO().Framerate;
    fps_smoothed = fps_smoothed == 0.0f ? cur_fps : SmoothLerp(fps_smoothed, cur_fps, 4.0f, dt);
    static double last_fps_sample = 0;
    if (ImGui::GetTime() - last_fps_sample > 0.066) {
        fps_history[fps_history_idx] = fps_smoothed;
        fps_history_idx = (fps_history_idx + 1) % FPS_HIST_SIZE;
        last_fps_sample = ImGui::GetTime();
    }

    ImVec2 base_size(1000, 700);
    ImVec2 scaled_size(base_size.x * menu_scale, base_size.y * menu_scale);

    ImGui::SetNextWindowSize(scaled_size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(80, 50), ImGuiCond_FirstUseEver);

    int wf = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
             ImGuiWindowFlags_NoBackground;
    ImGui::Begin("##traceless", nullptr, wf);

    // When menu is closed, disable all ImGui input
    if (!menu_visible) {
        ImGuiIO& io = ImGui::GetIO();
        for (int i = 0; i < 5; i++) io.MouseDown[i] = false;
        io.MouseWheel = 0.0f;
        io.MouseWheelH = 0.0f;
        ImGui::GetCurrentContext()->ActiveId = 0;
    }

    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImDrawList* fg = ImGui::GetForegroundDrawList();

    float fade = EaseOutCubic(gui_fade_anim);
    float oy = (1.0f - fade) * 14.0f;
    wp.y += oy;

    const float TOPBAR_H = 60.0f;
    const float SIDEBAR_W = 200.0f;

    // --- Window dragging via topbar ---
    ImVec2 hdr_min(wp.x, wp.y), hdr_max(wp.x + ws.x, wp.y + TOPBAR_H);
    if (ImGui::IsMouseHoveringRect(hdr_min, hdr_max) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        dragging = true;
        drag_offset = ImVec2(ImGui::GetIO().MousePos.x - wp.x, ImGui::GetIO().MousePos.y - wp.y);
    }
    if (dragging) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            ImVec2 np(ImGui::GetIO().MousePos.x - drag_offset.x, ImGui::GetIO().MousePos.y - drag_offset.y - oy);
            ImGui::SetWindowPos(np);
            wp = ImVec2(np.x, np.y + oy);
        } else dragging = false;
    }

    // --- Outer drop shadow (window-wide) ---
    for (int i = 0; i < 6; i++) {
        float t = i / 5.0f;
        ImU32 sc = ScaleAlpha(IM_COL32(0, 0, 0, 160), (1.0f - t) * 0.4f * fade);
        float off = (i + 1) * 3.0f;
        dl->AddRect(ImVec2(wp.x - off, wp.y - off),
                    ImVec2(wp.x + ws.x + off, wp.y + ws.y + off),
                    sc, 14.0f + off * 0.6f, 0, 1.5f);
    }

    // --- Window backdrop ---
    dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), C_BG_WIN, 12.0f);

    // Subtle accent glow in top-left corner
    for (int i = 0; i < 10; i++) {
        float t = i / 9.0f;
        ImU32 g = ScaleAlpha(C_ACCENT, 0.025f * (1.0f - t));
        dl->AddCircleFilled(ImVec2(wp.x + 60, wp.y + 60), 200 + i * 35, g);
    }
    // Subtle accent glow in bottom-right corner (mirror)
    for (int i = 0; i < 8; i++) {
        float t = i / 7.0f;
        ImU32 g = ScaleAlpha(C_ACCENT, 0.018f * (1.0f - t));
        dl->AddCircleFilled(ImVec2(wp.x + ws.x - 80, wp.y + ws.y - 80), 180 + i * 30, g);
    }

    // --- TOP BAR ---
    dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + TOPBAR_H), C_BG_TOPBAR, 12.0f);
    dl->AddRectFilled(ImVec2(wp.x, wp.y + TOPBAR_H - 8), ImVec2(wp.x + ws.x, wp.y + TOPBAR_H), C_BG_TOPBAR, 0.0f);
    dl->AddLine(ImVec2(wp.x, wp.y + TOPBAR_H), ImVec2(wp.x + ws.x, wp.y + TOPBAR_H),
                ScaleAlpha(C_BORDER_SOFT, 0.8f), 1.0f);

    // Logo
    dl->AddRectFilled(ImVec2(wp.x + 22, wp.y + 22), ImVec2(wp.x + 38, wp.y + 38), C_ACCENT, 4.0f);
    dl->AddRectFilled(ImVec2(wp.x + 26, wp.y + 26), ImVec2(wp.x + 34, wp.y + 34), C_BG_TOPBAR, 2.0f);
    dl->AddCircleFilled(ImVec2(wp.x + 30, wp.y + 30), 2, C_ACCENT);

    dl->AddText(ImVec2(wp.x + 50, wp.y + 17), C_TEXT_BRIGHT, "KENZO EXTERNAL");
    ImVec2 brand_sz = ImGui::CalcTextSize("KENZO EXTERNAL");
    dl->AddText(ImVec2(wp.x + 50 + brand_sz.x + 2, wp.y + 17), C_ACCENT, "v6");
    dl->AddText(ImVec2(wp.x + 50, wp.y + 31), C_TEXT_HINT, "premium external cheat");

    // Status pill (center)
    {
        float fps = ImGui::GetIO().Framerate;
        char status_buf[64];
        sprintf_s(status_buf, "ACTIVE  %.0f FPS", fps);
        ImVec2 sts = ImGui::CalcTextSize(status_buf);
        float pill_w = sts.x + 38;
        float pill_h = 26;
        ImVec2 pp(wp.x + ws.x * 0.5f - pill_w * 0.5f, wp.y + (TOPBAR_H - pill_h) * 0.5f);
        DrawPill(dl, pp, pill_w, pill_h, C_BG_PILL, C_BORDER);
        DrawStatusDot(dl, ImVec2(pp.x + 14, pp.y + pill_h * 0.5f), C_SUCCESS, (float)ImGui::GetTime());
        dl->AddText(ImVec2(pp.x + 26, pp.y + (pill_h - sts.y) * 0.5f), C_TEXT_BRIGHT, status_buf);
    }

    // Build info + window controls (right)
    {
        // PREMIUM badge
        const char* prem_text = "PREMIUM";
        ImVec2 pts = ImGui::CalcTextSize(prem_text);
        float pp_w = pts.x + 22, pp_h = 22;
        ImVec2 pp_pos(wp.x + ws.x - 80 - pp_w - 14, wp.y + 19);
        float prem_pulse = (sinf((float)ImGui::GetTime() * 1.4f) * 0.5f + 0.5f);
        ImU32 pp_bg = LerpColor(IM_COL32(60, 22, 25, 200), IM_COL32(90, 28, 32, 220), prem_pulse);
        DrawPill(dl, pp_pos, pp_w, pp_h, pp_bg, ScaleAlpha(C_ACCENT, 0.6f + prem_pulse * 0.3f));
        dl->AddCircleFilled(ImVec2(pp_pos.x + 10, pp_pos.y + pp_h * 0.5f), 2.5f, C_ACCENT);
        dl->AddText(ImVec2(pp_pos.x + 18, pp_pos.y + (pp_h - pts.y) * 0.5f), C_ACCENT_LIGHT, prem_text);

        const char* build_str = "build 1.2.0  nov 29 2025";
        ImVec2 bs = ImGui::CalcTextSize(build_str);
        dl->AddText(ImVec2(wp.x + ws.x - bs.x - 80, wp.y + 23), C_TEXT_HINT, build_str);

        // Minimize (cosmetic)
        ImVec2 mn_pos(wp.x + ws.x - 60, wp.y + 18);
        float mn_sz = 24;
        bool mn_hov = ImGui::IsMouseHoveringRect(mn_pos, ImVec2(mn_pos.x + mn_sz, mn_pos.y + mn_sz));
        float& mh = AnimState(btn_hover_anim, "tb_mn", 0.0f);
        mh = SmoothLerp(mh, mn_hov ? 1.0f : 0.0f, 20.0f, dt);
        dl->AddRectFilled(mn_pos, ImVec2(mn_pos.x + mn_sz, mn_pos.y + mn_sz),
                          ScaleAlpha(C_WHITE, mh * 0.06f), 4.0f);
        DrawMinimizeIcon(dl, ImVec2(mn_pos.x + mn_sz * 0.5f, mn_pos.y + mn_sz * 0.5f),
                         LerpColor(C_TEXT_DIM, C_TEXT_BRIGHT, mh));

        // Close → unload
        ImVec2 cl_pos(wp.x + ws.x - 32, wp.y + 18);
        bool cl_hov = ImGui::IsMouseHoveringRect(cl_pos, ImVec2(cl_pos.x + mn_sz, cl_pos.y + mn_sz));
        float& ch = AnimState(btn_hover_anim, "tb_cl", 0.0f);
        ch = SmoothLerp(ch, cl_hov ? 1.0f : 0.0f, 20.0f, dt);
        dl->AddRectFilled(cl_pos, ImVec2(cl_pos.x + mn_sz, cl_pos.y + mn_sz),
                          ScaleAlpha(C_ACCENT, ch * 0.3f), 4.0f);
        DrawCloseIcon(dl, ImVec2(cl_pos.x + mn_sz * 0.5f, cl_pos.y + mn_sz * 0.5f),
                      LerpColor(C_TEXT_DIM, C_WHITE, ch));

        ImGui::SetCursorScreenPos(cl_pos);
        ImGui::InvisibleButton("##cl", ImVec2(mn_sz, mn_sz));
        if (ImGui::IsItemClicked()) { should_unload = true; ShowToast("unloading cheat...", C_WARNING, 2.0f); }
    }

    // --- SIDEBAR ---
    ImVec2 sb_pos(wp.x, wp.y + TOPBAR_H);
    dl->AddRectFilled(sb_pos, ImVec2(sb_pos.x + SIDEBAR_W, wp.y + ws.y), C_BG_SIDEBAR, 0);
    dl->AddLine(ImVec2(sb_pos.x + SIDEBAR_W, sb_pos.y), ImVec2(sb_pos.x + SIDEBAR_W, wp.y + ws.y),
                ScaleAlpha(C_BORDER_SOFT, 0.6f), 1.0f);

    // small label above nav
    dl->AddText(ImVec2(sb_pos.x + 18, sb_pos.y + 14), C_TEXT_HINT, "NAVIGATION");

    float nav_y = sb_pos.y + 34;
    float nav_item_h = 38;
    float nav_gap = 4;
    float active_y = 0, active_h = 0;
    bool any_active = false;

    for (int i = 0; i < NAV_COUNT; i++) {
        if (RenderNavItem(dl, ImVec2(sb_pos.x, nav_y), i, active_tab == i, dt)) {
            active_tab = i;
        }
        if (active_tab == i) {
            active_y = nav_y;
            active_h = nav_item_h;
            any_active = true;
        }
        nav_y += nav_item_h + nav_gap;
    }

    // Active pill indicator (smooth spring)
    if (any_active) {
        if (!nav_pill_init) {
            nav_pill_y = active_y;
            nav_pill_h = active_h;
            nav_pill_alpha = 1.0f;
            nav_pill_init = true;
        } else {
            nav_pill_y = SmoothLerp(nav_pill_y, active_y, 18.0f, dt);
            nav_pill_h = SmoothLerp(nav_pill_h, active_h, 18.0f, dt);
            nav_pill_alpha = SmoothLerp(nav_pill_alpha, 1.0f, 14.0f, dt);
        }
        float py = nav_pill_y + (nav_pill_h - 20) * 0.5f;
        dl->AddRectFilled(ImVec2(sb_pos.x + 4, py),
                          ImVec2(sb_pos.x + 7, py + 20),
                          ScaleAlpha(C_ACCENT, nav_pill_alpha), 1.5f);
        dl->AddRectFilled(ImVec2(sb_pos.x + 2, py),
                          ImVec2(sb_pos.x + 7, py + 20),
                          ScaleAlpha(C_ACCENT_GLOW, nav_pill_alpha * 0.5f), 1.5f);
    }

    // Sidebar footer: separator + version info
    float footer_y = wp.y + ws.y - 64;
    dl->AddLine(ImVec2(sb_pos.x + 14, footer_y), ImVec2(sb_pos.x + SIDEBAR_W - 14, footer_y),
                ScaleAlpha(C_BORDER_SOFT, 0.5f), 1.0f);
    dl->AddText(ImVec2(sb_pos.x + 18, footer_y + 12), C_TEXT_HINT, "SESSION");

    DrawStatusDot(dl, ImVec2(sb_pos.x + 22, footer_y + 36), C_SUCCESS, (float)ImGui::GetTime());
    dl->AddText(ImVec2(sb_pos.x + 36, footer_y + 32), C_TEXT, "premium");

    // --- CONTENT ---
    if (active_tab != prev_active_tab) { tab_content_anim = 0.0f; prev_active_tab = active_tab; }
    tab_content_anim = SmoothLerp(tab_content_anim, 1.0f, 14.0f, dt);
    float off_x = (1.0f - EaseOutCubic(tab_content_anim)) * 30.0f;
    float content_alpha = EaseOutCubic(tab_content_anim);

    float content_x = wp.x + SIDEBAR_W;
    float content_y = wp.y + TOPBAR_H;
    float content_w = ws.x - SIDEBAR_W;
    float content_h = ws.y - TOPBAR_H;

    float pad = 18, col_gap = 14, v_gap = 12;
    float cw = (content_w - pad * 2 - col_gap) / 2;

    // Tab title bar
    const char* tab_title = NAV[active_tab].label;
    static const char* tab_subs[] = {
        "precision aiming and target acquisition",
        "world rendering and player visualization",
        "on-screen overlays and indicators",
        "movement and weapon exploits",
        "configuration management",
        "menu preferences and session controls",
    };
    dl->AddText(ImVec2(content_x + pad, content_y + 14), C_TEXT_BRIGHT, tab_title);
    ImVec2 tt_sz = ImGui::CalcTextSize(tab_title);
    dl->AddText(ImVec2(content_x + pad, content_y + 14 + tt_sz.y + 3), C_TEXT_HINT, tab_subs[active_tab]);

    // Right-side breadcrumb pill
    {
        char crumb[64];
        sprintf_s(crumb, "TAB %d / %d", active_tab + 1, NAV_COUNT);
        ImVec2 cs = ImGui::CalcTextSize(crumb);
        float pw = cs.x + 20, ph = 22;
        ImVec2 cp(content_x + content_w - pad - pw, content_y + 22);
        DrawPill(dl, cp, pw, ph, C_BG_PILL, C_BORDER);
        dl->AddText(ImVec2(cp.x + 10, cp.y + (ph - cs.y) * 0.5f), C_TEXT_DIM, crumb);
    }

    content_y += 60;
    content_h -= 60;

    // Subtle dot grid pattern behind cards
    {
        float dot_alpha = 0.04f * content_alpha;
        ImU32 dc = ScaleAlpha(C_WHITE, dot_alpha);
        float step = 22.0f;
        dl->PushClipRect(ImVec2(content_x + pad - 4, content_y - 4),
                         ImVec2(content_x + content_w - pad + 4, content_y + content_h - 4), true);
        for (float dy = content_y + 8; dy < content_y + content_h - 8; dy += step) {
            for (float dx = content_x + pad + 8; dx < content_x + content_w - pad - 8; dx += step) {
                dl->AddCircleFilled(ImVec2(dx, dy), 1.0f, dc);
            }
        }
        dl->PopClipRect();
    }

    // Reset stagger counter per tab
    card_render_index = 0;

    // Apply tab transition alpha
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, content_alpha);

    if (active_tab == 0) {
        // AIMBOT
        float top = 372, bot = 196;
        { Card c(dl, ImVec2(content_x + pad, content_y), cw, top, "aim", dt, off_x);
          c.Header("aim assist", &g_Options.LegitBot.AimBot.Enabled, "smart target lock");
          c.KB("keybind", &g_Options.LegitBot.AimBot.KeyBind);
          c.Section("targeting");
          c.Chk("target npcs", &g_Options.LegitBot.AimBot.TargetNPC);
          c.Chk("visible check", &g_Options.LegitBot.AimBot.VisibleCheck);
          c.Chk("prediction", &g_Options.LegitBot.AimBot.Prediction);
          static bool aim_bone = false;
          c.Chk("use closest bone", &aim_bone);
          c.Section("display");
          static bool aim_fov_tog = false;
          c.ChkTog("show fov", &g_Options.Misc.Screen.ShowAimbotFov, &aim_fov_tog, IM_COL32(240, 240, 240, 255));
          static std::vector<std::string> hb = {"head", "neck", "chest", "pelvis"};
          c.Combo("hitbox", &g_Options.LegitBot.AimBot.HitBox, hb);
          c.Sld("max distance", &g_Options.LegitBot.AimBot.MaxDistance, 0, 1000, "m");
        }
        { Card c(dl, ImVec2(content_x + pad + cw + col_gap, content_y), cw, top, "silent", dt, off_x);
          c.Header("silent aim", &g_Options.LegitBot.SilentAim.Enabled, "rage mode");
          c.KB("keybind", &g_Options.LegitBot.SilentAim.KeyBind);
          c.Section("targeting");
          c.Chk("target npcs", &g_Options.LegitBot.SilentAim.ShotNPC);
          c.Chk("visible check", &g_Options.LegitBot.SilentAim.VisibleCheck);
          c.Chk("magic bullet", &g_Options.LegitBot.SilentAim.MagicBullet);
          static bool sil_bone = false;
          c.Chk("use closest bone", &sil_bone);
          c.Section("display");
          static bool sil_fov_tog = true;
          c.ChkTog("show fov", &g_Options.Misc.Screen.ShowSilentAimFov, &sil_fov_tog);
          static int sil_hb = 0;
          static std::vector<std::string> hb2 = {"head", "neck", "chest", "pelvis"};
          c.Combo("hitbox", &sil_hb, hb2);
          c.Sld("max distance", &g_Options.LegitBot.SilentAim.MaxDistance, 0, 1000, "m");
        }
        { Card c(dl, ImVec2(content_x + pad, content_y + top + v_gap), cw, bot, "trigger", dt, off_x);
          c.Header("triggerbot", &g_Options.LegitBot.Trigger.Enabled, "auto-fire on target");
          c.KB("keybind", &g_Options.LegitBot.Trigger.KeyBind);
          c.Chk("target npcs", &g_Options.LegitBot.Trigger.ShotNPC);
          c.Chk("visible check", &g_Options.LegitBot.Trigger.VisibleCheck);
          c.Sld("max distance", &g_Options.LegitBot.Trigger.MaxDistance, 0, 1000, "m");
        }
        { Card c(dl, ImVec2(content_x + pad + cw + col_gap, content_y + top + v_gap), cw, bot, "info_aim", dt, off_x);
          c.Header("performance", nullptr, "live stats");
          char buf[64];
          c.Space(2);
          // Big FPS number
          sprintf_s(buf, "%.0f", fps_smoothed);
          ImVec2 big_sz = ImGui::CalcTextSize(buf);
          dl->AddText(ImVec2(c.pos.x + c.pad_x, c.pos.y + c.y), C_WHITE, buf);
          dl->AddText(ImVec2(c.pos.x + c.pad_x + big_sz.x + 4, c.pos.y + c.y + 3), C_TEXT_HINT, "fps");
          sprintf_s(buf, "%.1f ms / frame", 1000.0f / (fps_smoothed > 0 ? fps_smoothed : 60.0f));
          ImVec2 mt = ImGui::CalcTextSize(buf);
          dl->AddText(ImVec2(c.pos.x + c.w - c.pad_x - mt.x, c.pos.y + c.y + 4), C_TEXT_DIM, buf);
          c.y += 22;

          // FPS sparkline
          float gx0 = c.pos.x + c.pad_x;
          float gx1 = c.pos.x + c.w - c.pad_x;
          float gy0 = c.pos.y + c.y;
          float gy1 = c.pos.y + c.y + 90;
          dl->AddRectFilled(ImVec2(gx0, gy0), ImVec2(gx1, gy1), ScaleAlpha(C_BG_INPUT, 0.6f), 6.0f);
          dl->AddRect(ImVec2(gx0, gy0), ImVec2(gx1, gy1), ScaleAlpha(C_BORDER_SOFT, 0.8f), 6.0f, 0, 1.0f);
          float fmax = 1.0f, fmin = 1e9f;
          for (int i = 0; i < FPS_HIST_SIZE; i++) {
              float v = fps_history[i];
              if (v <= 0.0f) continue;
              if (v > fmax) fmax = v;
              if (v < fmin) fmin = v;
          }
          if (fmin > fmax) fmin = 0;
          if (fmax < 60) fmax = 60;
          float rng = fmax - fmin;
          if (rng < 30) { float mid = (fmax + fmin) * 0.5f; fmax = mid + 15; fmin = mid - 15; rng = 30; }
          // grid lines
          for (int i = 1; i < 3; i++) {
              float gy = gy0 + (gy1 - gy0) * i / 3.0f;
              dl->AddLine(ImVec2(gx0 + 4, gy), ImVec2(gx1 - 4, gy), ScaleAlpha(C_BORDER_SOFT, 0.5f), 1.0f);
          }
          // line graph
          ImVec2 prev(0, 0);
          bool first = true;
          for (int i = 0; i < FPS_HIST_SIZE; i++) {
              int idx = (fps_history_idx + i) % FPS_HIST_SIZE;
              float v = fps_history[idx];
              if (v <= 0) continue;
              float nx = gx0 + 6 + (gx1 - gx0 - 12) * (i / (float)(FPS_HIST_SIZE - 1));
              float ny = gy1 - 6 - (gy1 - gy0 - 12) * Clamp01((v - fmin) / rng);
              if (!first) {
                  dl->AddLine(prev, ImVec2(nx, ny), C_ACCENT, 1.6f);
                  // fill under line
                  dl->AddTriangleFilled(prev, ImVec2(nx, ny), ImVec2(nx, gy1 - 1), ScaleAlpha(C_ACCENT, 0.06f));
                  dl->AddTriangleFilled(prev, ImVec2(prev.x, gy1 - 1), ImVec2(nx, gy1 - 1), ScaleAlpha(C_ACCENT, 0.06f));
              }
              prev = ImVec2(nx, ny);
              first = false;
          }
          // current value dot
          if (!first) {
              dl->AddCircleFilled(prev, 3.0f, C_WHITE);
              dl->AddCircleFilled(prev, 5.5f, ScaleAlpha(C_ACCENT, 0.3f));
          }
          c.y += 100;

          // status row
          dl->AddText(ImVec2(c.pos.x + c.pad_x, c.pos.y + c.y), C_TEXT_DIM, "status");
          DrawStatusDot(dl, ImVec2(c.pos.x + c.w - c.pad_x - 56, c.pos.y + c.y + 6), C_SUCCESS, (float)ImGui::GetTime());
          dl->AddText(ImVec2(c.pos.x + c.w - c.pad_x - 44, c.pos.y + c.y), C_SUCCESS, "online");
        }
    }
    else if (active_tab == 1) {
        // VISUALS
        { Card c(dl, ImVec2(content_x + pad, content_y), cw, 388, "players", dt, off_x);
          c.Header("players", &g_Options.Visuals.ESP.Players.Enabled, "esp & overlays");
          c.Section("box & geometry");
          c.Chk("box", &g_Options.Visuals.ESP.Players.Box);
          c.Chk("corner box", &g_Options.Visuals.ESP.Players.CornerBox);
          c.Chk("skeleton", &g_Options.Visuals.ESP.Players.Skeleton);
          c.Chk("head", &g_Options.Visuals.ESP.Players.Head);
          c.Section("info text");
          c.Chk("name", &g_Options.Visuals.ESP.Players.Name);
          c.Chk("health bar", &g_Options.Visuals.ESP.Players.HealthBar);
          c.Chk("armor bar", &g_Options.Visuals.ESP.Players.ArmorBar);
          c.Chk("weapon name", &g_Options.Visuals.ESP.Players.WeaponName);
          c.Chk("distance", &g_Options.Visuals.ESP.Players.Distance);
          c.Chk("snap lines", &g_Options.Visuals.ESP.Players.SnapLines);
        }
        { Card c(dl, ImVec2(content_x + pad + cw + col_gap, content_y), cw, 196, "vehicles", dt, off_x);
          c.Header("vehicles", &g_Options.Visuals.ESP.Vehicles.Enabled);
          c.Chk("ignore occupied", &g_Options.Visuals.ESP.Vehicles.IgnoreOccupiedVehicles);
          c.Chk("name", &g_Options.Visuals.ESP.Vehicles.Name);
          c.Chk("distance", &g_Options.Visuals.ESP.Vehicles.Distance);
          c.Chk("marker", &g_Options.Visuals.ESP.Vehicles.Marker);
        }
        { Card c(dl, ImVec2(content_x + pad + cw + col_gap, content_y + 196 + v_gap), cw, 180, "set_esp", dt, off_x);
          c.Header("filters");
          c.Chk("show local player", &g_Options.Visuals.ESP.Players.ShowLocalPlayer);
          c.Chk("show npcs", &g_Options.Visuals.ESP.Players.ShowNPCs);
          c.Chk("visible only", &g_Options.Visuals.ESP.Players.VisibleOnly);
          c.Sld("render distance", &g_Options.Visuals.ESP.Players.RenderDistance, 0, 1000, "m");
        }
    }
    else if (active_tab == 2) {
        // OVERLAYS
        { Card c(dl, ImVec2(content_x + pad, content_y), cw, 252, "screen", dt, off_x);
          c.Header("screen overlays", nullptr, "hud elements");
          c.Chk("watermark", &g_Options.Misc.Screen.EnableWatermark);
          c.Chk("keybind list", &g_Options.Misc.Screen.EnableKeybindList);
          c.Section("fov indicators");
          c.Chk("aimbot fov", &g_Options.Misc.Screen.ShowAimbotFov);
          c.Chk("silent fov", &g_Options.Misc.Screen.ShowSilentAimFov);
          c.Chk("trigger fov", &g_Options.Misc.Screen.ShowTriggerFov);
        }
        { Card c(dl, ImVec2(content_x + pad + cw + col_gap, content_y), cw, 252, "info_ov", dt, off_x);
          c.Header("about", nullptr, "info");
          c.Space(6);
          dl->AddText(ImVec2(c.pos.x + c.pad_x, c.pos.y + c.y), C_TEXT_DIM, "build");
          dl->AddText(ImVec2(c.pos.x + c.w - c.pad_x - 96, c.pos.y + c.y), C_TEXT_BRIGHT, "1.2.0 (nov 29)");
          c.y += 22;
          dl->AddText(ImVec2(c.pos.x + c.pad_x, c.pos.y + c.y), C_TEXT_DIM, "subscription");
          dl->AddText(ImVec2(c.pos.x + c.w - c.pad_x - 60, c.pos.y + c.y), C_SUCCESS, "premium");
          c.y += 22;
          dl->AddText(ImVec2(c.pos.x + c.pad_x, c.pos.y + c.y), C_TEXT_DIM, "renews in");
          dl->AddText(ImVec2(c.pos.x + c.w - c.pad_x - 50, c.pos.y + c.y), C_TEXT_BRIGHT, "28 days");
          c.y += 32;
          dl->AddText(ImVec2(c.pos.x + c.pad_x, c.pos.y + c.y), C_TEXT_HINT, "thanks for using kenzo");
        }
    }
    else if (active_tab == 3) {
        // EXPLOITS
        { Card c(dl, ImVec2(content_x + pad, content_y), cw, 264, "local", dt, off_x);
          c.Header("local player", nullptr, "self modifications");
          c.Chk("god mode", &g_Options.Exploits.LocalPlayer.God);
          c.Chk("noclip", &g_Options.Exploits.LocalPlayer.Noclip);
          c.Chk("invisible", &g_Options.Exploits.LocalPlayer.Invisible);
          c.Chk("shrink", &g_Options.Exploits.LocalPlayer.Shrink);
          c.Chk("speed hack", &g_Options.Exploits.LocalPlayer.speed);
          static int spd = (int)g_Options.Exploits.LocalPlayer.Player_speed;
          if (spd < 1) spd = 5;
          c.Sld("player speed", &spd, 1, 10);
          g_Options.Exploits.LocalPlayer.Player_speed = (float)spd;
        }
        { Card c(dl, ImVec2(content_x + pad + cw + col_gap, content_y), cw, 240, "weapon", dt, off_x);
          c.Header("weapon", nullptr, "firearm tweaks");
          c.Chk("no reload", &g_Options.Exploits.Weapon.NoReload);
          c.Chk("no recoil", &g_Options.Exploits.Weapon.NoRecoil);
          c.Chk("no spread", &g_Options.Exploits.Weapon.NoSpread);
          c.Chk("rapid fire", &g_Options.Exploits.Weapon.RapidFire);
          c.Chk("one shot kill", &g_Options.Exploits.Weapon.OneShotKill);
          c.Chk("infinite ammo", &g_Options.Exploits.Weapon.InfiniteAmmo);
        }
    }
    else if (active_tab == 4) {
        // CONFIGS placeholder
        Card c(dl, ImVec2(content_x + pad, content_y), content_w - pad * 2, 240, "cfg", dt, off_x);
        c.Header("configurations", nullptr, "save & load presets");
        c.Space(8);
        dl->AddText(ImVec2(c.pos.x + c.pad_x, c.pos.y + c.y), C_TEXT_DIM, "preset management coming in v1.3");
        c.y += 24;
        dl->AddText(ImVec2(c.pos.x + c.pad_x, c.pos.y + c.y), C_TEXT_HINT,
                    "you'll be able to save, load, and share configs across sessions");
    }
    else if (active_tab == 5) {
        // SETTINGS
        {
            Card c(dl, ImVec2(content_x + pad, content_y), cw, 192, "general", dt, off_x);
            c.Header("general", nullptr, "core behavior");
            c.Chk("capture bypass", &g_Options.General.CaptureBypass);
            c.Chk("legit mode", &g_Options.Misc.Other.legit_mode);
            c.Chk("anti screenshot", &g_Options.Misc.Other.anti_screenshot);
            c.Sld("thread delay", &g_Options.General.ThreadDelay, 0, 32, "ms");
        }
        {
            Card c(dl, ImVec2(content_x + pad + cw + col_gap, content_y), cw, 192, "menu", dt, off_x);
            c.Header("menu", nullptr, "appearance");
            static int menu_scale_pct = 100;
            menu_scale_pct = (int)(menu_scale * 100.0f + 0.5f);
            int prev_pct = menu_scale_pct;
            c.Sld("scale", &menu_scale_pct, 60, 150, "%");
            if (menu_scale_pct != prev_pct) {
                menu_scale = menu_scale_pct / 100.0f;
            }
            c.Chk("pause game while open", &pause_game);
            c.Space(4);
        }
        {
            Card c(dl, ImVec2(content_x + pad, content_y + 192 + v_gap), cw * 2 + col_gap, 124, "danger", dt, off_x);
            c.Header("danger zone", nullptr, "irreversible actions");
            c.Space(2);
            bool unload_clicked = false;
            c.Btn("UNLOAD CHEAT", &unload_clicked);
            if (unload_clicked) {
                should_unload = true;
                ShowToast("unloading cheat...", C_WARNING, 2.0f);
            }
        }
    }

    ImGui::PopStyleVar();

    // --- Toasts (bottom-right, foreground) ---
    RenderToasts(fg, ImVec2(wp.x + ws.x - 18, wp.y + ws.y - 18), dt);

    ImGui::End();
}

} // namespace FrameWork
