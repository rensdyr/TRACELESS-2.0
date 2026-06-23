#include "gui.hpp"
#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <imgui.h>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

namespace FrameWork {
namespace render_ui {

static std::map<std::string, bool>    section_open;
static std::map<std::string, float>   section_anim;
static std::map<std::string, float>   chk_anim;
static std::map<std::string, float>   tog_anim;
static std::map<std::string, float>   sld_hover_anim;
static std::map<std::string, float>   hdr_hover_anim;
static std::map<std::string, float>   kb_press_anim;
static std::map<std::string, float>   cmb_hover_anim;
static std::map<std::string, double> chk_last_change_time;
static std::map<std::string, double> sld_last_change_time;

static int   active_tab = 0;
static int   prev_active_tab = 0;
static float tab_content_anim = 1.0f;
static float tab_indicator_anim = 0.0f;
static float tab_indicator_x = 0.0f;
static float tab_indicator_width = 0.0f;
static bool  tab_indicator_init = false;
static float gui_fade_anim = 0.0f;

static bool  gui_visible = false;
static bool  gui_was_visible = false;
static float menu_scale = 1.0f;
static int   menu_scale_pct = 100;
static double cursor_last_move_time = -10.0;
static bool  unload_pressed = false;
static double unload_last_press_time = -10.0;

static bool key_listening = false;
static int* listening_key = nullptr;
static int  last_listening_key = 0;
static double key_press_debounce_time = 0.0;

static bool  should_unload = false;
static bool  pause_game = false;

static const ImU32 C_ACCENT       = IM_COL32(205, 60, 60, 255);
static const ImU32 C_ACCENT_DIM   = IM_COL32(170, 45, 45, 255);
static const ImU32 C_ACCENT_LIGHT = IM_COL32(230, 100, 100, 180);
static const ImU32 C_BG_CARD      = IM_COL32(30, 30, 35, 190);
static const ImU32 C_BG_CARD_HOV  = IM_COL32(38, 38, 44, 200);
static const ImU32 C_BG_WIN       = IM_COL32(12, 12, 15, 175);
static const ImU32 C_BG_INPUT     = IM_COL32(16, 16, 20, 215);
static const ImU32 C_BORDER       = IM_COL32(60, 60, 68, 70);
static const ImU32 C_BORDER_SOFT  = IM_COL32(50, 50, 58, 55);
static const ImU32 C_TEXT         = IM_COL32(185, 185, 192, 235);
static const ImU32 C_TEXT_DIM     = IM_COL32(115, 115, 125, 235);
static const ImU32 C_TEXT_BRIGHT  = IM_COL32(215, 215, 222, 245);
static const ImU32 C_WHITE        = IM_COL32(235, 235, 240, 255);

static const float BASE_WINDOW_W = 920.0f;
static const float BASE_WINDOW_H = 700.0f;
static const float MIN_SCALE = 0.60f;
static const float MAX_SCALE = 1.50f;
static const float SMOOTH_SPEED_MIN = 8.0f;
static const float SMOOTH_SPEED_MAX = 100.0f;

static float Clamp01(float t) { return t < 0 ? 0 : (t > 1 ? 1 : t); }
static float Clamp(float v, float mn, float mx) { return v < mn ? mn : (v > mx ? mx : v); }

static float SmoothLerp(float cur, float tgt, float speed, float dt) {
    if (dt > 0.1f) dt = 0.1f;
    speed = Clamp(speed, SMOOTH_SPEED_MIN, SMOOTH_SPEED_MAX);
    float t = 1.0f - expf(-speed * dt);
    return cur + (tgt - cur) * Clamp01(t);
}

static float EaseOutCubic(float t) {
    t = Clamp01(t);
    float f = t - 1.0f;
    return f * f * f + 1.0f;
}

static float EaseInOutQuad(float t) {
    t = Clamp01(t);
    return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
}

static ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    t = Clamp01(t);
    int ra = a & 0xFF, ga = (a >> 8) & 0xFF, ba = (a >> 16) & 0xFF, aa = (a >> 24) & 0xFF;
    int rb = b & 0xFF, gb = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF, ab = (b >> 24) & 0xFF;
    return IM_COL32(
        (int)std::round(ra + (rb - ra) * t),
        (int)std::round(ga + (gb - ga) * t),
        (int)std::round(ba + (bb - ba) * t),
        (int)std::round(aa + (ab - aa) * t)
    );
}

static ImU32 ScaleAlpha(ImU32 c, float s) {
    int a = (int)std::round(((c >> 24) & 0xFF) * Clamp01(s));
    return (c & 0x00FFFFFF) | ((ImU32)a << 24);
}

static float& AnimState(std::map<std::string, float>& m, const std::string& k, float def) {
    auto it = m.find(k);
    if (it == m.end()) m[k] = def;
    return m[k];
}

static std::string KeyCodeToString(int key) {
    if (key == 0) return "none";
    switch (key) {
        case VK_LBUTTON: return "lmb"; case VK_RBUTTON: return "rmb"; case VK_MBUTTON: return "mmb";
        case VK_XBUTTON1: return "m4"; case VK_XBUTTON2: return "m5";
        case VK_SHIFT: return "shift"; case VK_CONTROL: return "ctrl"; case VK_MENU: return "alt";
        case VK_SPACE: return "space"; case VK_RETURN: return "enter"; case VK_ESCAPE: return "esc"; case VK_TAB: return "tab";
        default:
            if (key >= 'A' && key <= 'Z') { static char b[2] = {0, 0}; b[0] = (char)(key + 32); return std::string(b); }
            if (key >= VK_F1 && key <= VK_F12) { static char b[4]; sprintf_s(b, sizeof(b), "f%d", key - VK_F1 + 1); return std::string(b); }
            if (key >= '0' && key <= '9') { static char b[2] = {0, 0}; b[0] = (char)key; return std::string(b); }
            return "key";
    }
}

static void MoveCursorToMenu(int x, int y) {
    HWND game_window = ::GetForegroundWindow();
    if (game_window == nullptr) return;

    if (::SetCursorPos(x, y)) {
        cursor_last_move_time = ImGui::GetTime();
    }
}

void ApplyDarkTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 10; s.FrameRounding = 4; s.GrabRounding = 4; s.PopupRounding = 4; s.ChildRounding = 8;
    s.WindowPadding = ImVec2(0, 0); s.FramePadding = ImVec2(4, 3);
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    s.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    s.Colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);
}

static void DrawKeyboardIcon(ImDrawList* dl, ImVec2 p, ImU32 col) {
    dl->AddRect(p, ImVec2(p.x + 15, p.y + 10), col, 2.0f, 0, 1.0f);
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 4; c++) {
            float x = p.x + 2 + c * 3.1f, y = p.y + 2 + r * 3.0f;
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 1.6f, y + 1.6f), col);
        }
}

static void DrawCrosshairIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c, 7, col, 22, 1.3f);
    dl->AddLine(ImVec2(c.x - 10, c.y), ImVec2(c.x - 3, c.y), col, 1.3f);
    dl->AddLine(ImVec2(c.x + 3, c.y), ImVec2(c.x + 10, c.y), col, 1.3f);
    dl->AddLine(ImVec2(c.x, c.y - 10), ImVec2(c.x, c.y - 3), col, 1.3f);
    dl->AddLine(ImVec2(c.x, c.y + 3), ImVec2(c.x, c.y + 10), col, 1.3f);
}

static void DrawEyeIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddBezierCubic(ImVec2(c.x - 10, c.y), ImVec2(c.x - 5, c.y - 6), ImVec2(c.x + 5, c.y - 6), ImVec2(c.x + 10, c.y), col, 1.3f);
    dl->AddBezierCubic(ImVec2(c.x - 10, c.y), ImVec2(c.x - 5, c.y + 6), ImVec2(c.x + 5, c.y + 6), ImVec2(c.x + 10, c.y), col, 1.3f);
    dl->AddCircle(c, 3, col, 14, 1.2f);
}

static void DrawGlobeIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c, 9, col, 28, 1.3f);
    dl->AddLine(ImVec2(c.x - 9, c.y), ImVec2(c.x + 9, c.y), col, 1.1f);
    dl->AddBezierCubic(ImVec2(c.x, c.y - 9), ImVec2(c.x - 5, c.y - 3), ImVec2(c.x - 5, c.y + 3), ImVec2(c.x, c.y + 9), col, 1.1f);
    dl->AddBezierCubic(ImVec2(c.x, c.y - 9), ImVec2(c.x + 5, c.y - 3), ImVec2(c.x + 5, c.y + 3), ImVec2(c.x, c.y + 9), col, 1.1f);
}

static void DrawExploitsIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddLine(ImVec2(c.x - 9, c.y - 4), ImVec2(c.x + 7, c.y - 4), col, 1.3f);
    dl->AddLine(ImVec2(c.x + 7, c.y - 4), ImVec2(c.x + 4, c.y - 7), col, 1.3f);
    dl->AddLine(ImVec2(c.x + 7, c.y - 4), ImVec2(c.x + 4, c.y - 1), col, 1.3f);
    dl->AddLine(ImVec2(c.x + 9, c.y + 4), ImVec2(c.x - 7, c.y + 4), col, 1.3f);
    dl->AddLine(ImVec2(c.x - 7, c.y + 4), ImVec2(c.x - 4, c.y + 1), col, 1.3f);
    dl->AddLine(ImVec2(c.x - 7, c.y + 4), ImVec2(c.x - 4, c.y + 7), col, 1.3f);
}

static void DrawListsIcon(ImDrawList* dl, ImVec2 c, ImU32 col) { DrawKeyboardIcon(dl, ImVec2(c.x - 7, c.y - 5), col); }

static void DrawSettingsIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c, 8, col, 24, 1.3f); dl->AddCircle(c, 3, col, 14, 1.2f);
    for (int i = 0; i < 8; i++) {
        float a = i * 6.2831f / 8;
        dl->AddLine(ImVec2(c.x + cosf(a) * 8, c.y + sinf(a) * 8), ImVec2(c.x + cosf(a) * 10.5f, c.y + sinf(a) * 10.5f), col, 1.3f);
    }
}

static void DrawCardShadow(ImDrawList* dl, ImVec2 pos, float w, float h, float intensity) {
    if (intensity < 0.01f) return;
    ImU32 shadow = ScaleAlpha(IM_COL32(0, 0, 0, 100), intensity * 0.5f);
    dl->AddRectFilled(ImVec2(pos.x + 2, pos.y + h - 1), ImVec2(pos.x + w - 2, pos.y + h + 3), shadow, 4.0f);
    ImU32 shadow2 = ScaleAlpha(IM_COL32(0, 0, 0, 50), intensity * 0.25f);
    dl->AddRectFilled(ImVec2(pos.x + 1, pos.y + h + 3), ImVec2(pos.x + w - 1, pos.y + h + 5), shadow2, 2.0f);
}

static void DrawAnimatedCheckmark(ImDrawList* dl, ImVec2 cp, float sz, float ca) {
    if (ca < 0.02f) return;
    dl->AddRectFilled(ImVec2(cp.x + 2, cp.y + 2), ImVec2(cp.x + sz - 2, cp.y + sz - 2), ScaleAlpha(C_ACCENT, ca), 2.0f);
    ImU32 mk = ScaleAlpha(C_WHITE, ca);
    float prog = EaseOutCubic(ca);
    ImVec2 a0(cp.x + 2.5f, cp.y + 6), a1(cp.x + 5, cp.y + 8.5f), a2(cp.x + 9.5f, cp.y + 3.5f);
    float s1 = prog < 0.4f ? prog / 0.4f : 1.0f;
    float s2 = prog < 0.4f ? 0.0f : (prog - 0.4f) / 0.6f;
    ImVec2 e1(a0.x + (a1.x - a0.x) * s1, a0.y + (a1.y - a0.y) * s1);
    dl->AddLine(a0, e1, mk, 1.4f);
    if (s2 > 0) {
        ImVec2 e2(a1.x + (a2.x - a1.x) * s2, a1.y + (a2.y - a1.y) * s2);
        dl->AddLine(a1, e2, mk, 1.4f);
    }
}

static void DrawPulsingDot(ImDrawList* dl, ImVec2 pos, float pulse, ImU32 col) {
    float r = 3.0f + sinf(pulse * 6.2831f) * 1.5f;
    dl->AddCircleFilled(pos, r, ScaleAlpha(col, 0.7f + sinf(pulse * 6.2831f) * 0.3f));
}

struct Card {
    ImDrawList* dl;
    ImVec2 pos;
    float w, h, h_target, y, dt;
    const char* id;
    bool open;
    float pad_x, anim_progress;
    bool clip_pushed;
    float scale;

    static constexpr float BASE_HEADER_H = 42.0f;
    static constexpr float BASE_CHK_STEP = 28.0f;
    static constexpr float BASE_KB_STEP = 30.0f;
    static constexpr float BASE_CMB_STEP = 52.0f;
    static constexpr float BASE_SLD_STEP = 40.0f;

    float get_header_h() { return BASE_HEADER_H * scale; }
    float get_chk_step() { return BASE_CHK_STEP * scale; }
    float get_kb_step() { return BASE_KB_STEP * scale; }
    float get_cmb_step() { return BASE_CMB_STEP * scale; }
    float get_sld_step() { return BASE_SLD_STEP * scale; }

    Card(ImDrawList* d, ImVec2 p, float width, float full_h, const char* sid, float deltaTime, float off_x = 0.0f, float sc = 1.0f)
        : dl(d), pos(ImVec2(p.x + off_x, p.y)), w(width), h_target(full_h), y(0), id(sid),
          pad_x(16.0f * sc), dt(deltaTime), clip_pushed(false), scale(sc) {
        if (section_open.find(sid) == section_open.end())
            section_open[sid] = true;
        open = section_open[sid];

        float& a = AnimState(section_anim, sid, open ? 1.0f : 0.0f);
        a = SmoothLerp(a, open ? 1.0f : 0.0f, 16.0f, dt);
        anim_progress = a;

        float min_h = get_header_h() + 2;
        h = min_h + (h_target - min_h) * EaseOutCubic(anim_progress);

        bool hovered = ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + w, pos.y + h));
        float& hv = AnimState(hdr_hover_anim, std::string("c_") + sid, 0.0f);
        hv = SmoothLerp(hv, hovered ? 1.0f : 0.0f, 18.0f, dt);

        ImU32 bg = LerpColor(C_BG_CARD, C_BG_CARD_HOV, hv * 0.5f);
        ImU32 br = LerpColor(C_BORDER_SOFT, ScaleAlpha(C_ACCENT, 0.4f), hv * 0.3f);

        float content_fade = tab_content_anim > 0.6f ? 1.0f : 0.0f;
        DrawCardShadow(dl, pos, w, h, hv * content_fade * 0.6f);

        dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), bg, 8.0f);
        dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), br, 8.0f, 0, 1.0f);

        dl->PushClipRect(ImVec2(pos.x, pos.y), ImVec2(pos.x + w, pos.y + h), true);
        clip_pushed = true;
    }

    ~Card() { if (clip_pushed) dl->PopClipRect(); }

    void Header(const char* label) {
        float hdr_h = get_header_h();
        float& hv = AnimState(hdr_hover_anim, std::string("ch_") + id, 0.0f);
        ImU32 txt_col = LerpColor(C_TEXT_BRIGHT, C_ACCENT_LIGHT, hv * 0.2f);
        dl->AddText(ImVec2(pos.x + pad_x, pos.y + hdr_h * 0.35f), txt_col, label);

        ImVec2 cv(pos.x + w - pad_x - 1, pos.y + hdr_h * 0.5f);
        float rot = (anim_progress - 1.0f) * 1.5708f;
        float cr = cosf(rot), sr = sinf(rot);
        auto rp = [&](float dx, float dy) {
            return ImVec2(cv.x + dx * cr - dy * sr, cv.y + dx * sr + dy * cr);
        };

        ImU32 chev_col = LerpColor(C_TEXT_DIM, C_ACCENT, anim_progress * 0.3f);
        dl->AddTriangleFilled(rp(-4, -2), rp(4, -2), rp(0, 3), chev_col);

        if (anim_progress > 0.4f) {
            float line_alpha = (anim_progress - 0.4f) / 0.6f;
            dl->AddLine(ImVec2(pos.x + pad_x, pos.y + hdr_h - 1),
                        ImVec2(pos.x + w - pad_x, pos.y + hdr_h - 1),
                        ScaleAlpha(C_ACCENT, line_alpha * 0.3f), 0.5f);
        }

        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton((std::string("##hdr_") + id).c_str(), ImVec2(w, hdr_h));
        if (ImGui::IsItemClicked()) {
            section_open[id] = !section_open[id];
            open = section_open[id];
        }
        y = hdr_h;
    }

    bool Chk(const char* label, bool* v) {
        float chk_step = get_chk_step();
        if (anim_progress < 0.01f) { y += chk_step; return false; }
        std::string key = std::string(id) + "_chk_" + label;
        float& ca = AnimState(chk_anim, key, *v ? 1.0f : 0.0f);

        bool prev_ca = ca > 0.5f;
        ca = SmoothLerp(ca, *v ? 1.0f : 0.0f, 24.0f, dt);
        bool now_ca = ca > 0.5f;

        if (prev_ca != now_ca) {
            chk_last_change_time[key] = ImGui::GetTime();
        }

        double time_since_change = ImGui::GetTime() - (chk_last_change_time.find(key) != chk_last_change_time.end() ? chk_last_change_time[key] : -10.0);
        float change_highlight = Clamp01(1.0f - (float)(time_since_change / 0.4f));

        float sz = 12.0f * scale;
        ImVec2 cp(pos.x + pad_x, pos.y + y + 3 * scale);
        ImU32 brd = LerpColor(C_BORDER, C_ACCENT, ca);
        dl->AddRect(cp, ImVec2(cp.x + sz, cp.y + sz), brd, 3.0f, 0, 1.0f);

        if (change_highlight > 0.1f) {
            dl->AddRect(ImVec2(cp.x - 2, cp.y - 2), ImVec2(cp.x + sz + 2, cp.y + sz + 2),
                        ScaleAlpha(C_ACCENT, change_highlight * 0.4f), 3.0f, 0, 1.0f);
        }

        DrawAnimatedCheckmark(dl, cp, sz, ca);
        ImU32 lbl_col = LerpColor(C_TEXT, C_ACCENT_LIGHT, change_highlight * 0.3f);
        dl->AddText(ImVec2(cp.x + sz + 7 * scale, cp.y - 2), lbl_col, label);

        if (anim_progress > 0.65f) {
            ImGui::SetCursorScreenPos(ImVec2(pos.x + pad_x - 4, pos.y + y));
            ImGui::InvisibleButton((std::string("##chk_") + id + label).c_str(), ImVec2(w - pad_x * 2, chk_step - 2));
            if (ImGui::IsItemClicked()) *v = !*v;
        }
        y += chk_step;
        return true;
    }

    bool ChkTog(const char* label, bool* v, bool* tog, ImU32 tog_color = 0) {
        float chk_step = get_chk_step();
        if (anim_progress < 0.01f) { y += chk_step; return false; }
        std::string ck = std::string(id) + "_ct_" + label;
        float& ca = AnimState(chk_anim, ck, *v ? 1.0f : 0.0f);
        ca = SmoothLerp(ca, *v ? 1.0f : 0.0f, 24.0f, dt);

        float sz = 12.0f * scale;
        ImVec2 cp(pos.x + pad_x, pos.y + y + 3 * scale);
        dl->AddRect(cp, ImVec2(cp.x + sz, cp.y + sz), LerpColor(C_BORDER, C_ACCENT, ca), 3.0f, 0, 1.0f);
        DrawAnimatedCheckmark(dl, cp, sz, ca);
        dl->AddText(ImVec2(cp.x + sz + 7 * scale, cp.y - 2), C_TEXT, label);

        std::string tk = std::string(id) + "_tg_" + label;
        float& ta = AnimState(tog_anim, tk, *tog ? 1.0f : 0.0f);
        ta = SmoothLerp(ta, *tog ? 1.0f : 0.0f, 22.0f, dt);

        float tw = 28 * scale, th = 14 * scale;
        ImVec2 tp(pos.x + w - pad_x - tw, pos.y + y + 2 * scale);
        ImU32 fill = LerpColor(C_BG_INPUT, tog_color ? tog_color : C_ACCENT, ta);
        dl->AddRectFilled(tp, ImVec2(tp.x + tw, tp.y + th), fill, th * 0.5f);

        ImU32 border_col = LerpColor(C_BORDER, tog_color ? tog_color : C_ACCENT, ta * 0.5f);
        dl->AddRect(tp, ImVec2(tp.x + tw, tp.y + th), border_col, th * 0.5f, 0, 1.0f);

        if (ta > 0.3f) {
            ImU32 glow = ScaleAlpha(tog_color ? tog_color : C_ACCENT, (ta - 0.3f) * 0.25f);
            dl->AddRect(ImVec2(tp.x - 2, tp.y - 2), ImVec2(tp.x + tw + 2, tp.y + th + 2), glow, th * 0.5f + 2, 0, 1.0f);
        }

        float kr = (th - 4) * 0.5f;
        float kx = tp.x + 2 + kr + (tw - 4 - kr * 2) * ta;
        dl->AddCircleFilled(ImVec2(kx, tp.y + th * 0.5f), kr, C_WHITE);

        if (anim_progress > 0.65f) {
            ImGui::SetCursorScreenPos(ImVec2(pos.x + pad_x - 4, pos.y + y));
            ImGui::InvisibleButton((std::string("##ct_") + id + label).c_str(), ImVec2(w - pad_x * 2 - tw - 6, chk_step - 2));
            if (ImGui::IsItemClicked()) *v = !*v;
            ImGui::SetCursorScreenPos(tp);
            ImGui::InvisibleButton((std::string("##tg_") + id + label).c_str(), ImVec2(tw, th));
            if (ImGui::IsItemClicked()) *tog = !*tog;
        }
        y += chk_step;
        return true;
    }

    void KB(const char* label, int* key) {
        float kb_step = get_kb_step();
        if (anim_progress < 0.01f) { y += kb_step; return; }
        dl->AddText(ImVec2(pos.x + pad_x, pos.y + y + 3 * scale), C_TEXT, label);

        float bw = 68 * scale, bh = 19 * scale;
        ImVec2 bp(pos.x + w - bw - pad_x, pos.y + y + 1 * scale);

        bool listening = (key_listening && listening_key == key);
        std::string kk = std::string(id) + "_kb_" + label;
        float& kp = AnimState(kb_press_anim, kk, 0.0f);
        kp = SmoothLerp(kp, listening ? 1.0f : 0.0f, 20.0f, dt);

        ImU32 bg = LerpColor(C_BG_INPUT, ScaleAlpha(C_ACCENT, 0.4f), kp);
        ImU32 brd = LerpColor(C_BORDER, C_ACCENT, kp);
        dl->AddRectFilled(bp, ImVec2(bp.x + bw, bp.y + bh), bg, 3.0f);
        dl->AddRect(bp, ImVec2(bp.x + bw, bp.y + bh), brd, 3.0f, 0, 1.0f);

        DrawKeyboardIcon(dl, ImVec2(bp.x + 5 * scale, bp.y + 4 * scale), C_ACCENT);
        std::string s = listening ? "..." : KeyCodeToString(*key);
        ImVec2 ts = ImGui::CalcTextSize(s.c_str());
        dl->AddText(ImVec2(bp.x + 25 * scale, bp.y + (bh - ts.y) / 2), listening ? C_ACCENT : C_TEXT, s.c_str());

        if (listening) {
            DrawPulsingDot(dl, ImVec2(bp.x + bw - 6 * scale, bp.y + bh / 2), ImGui::GetTime() * 2.0f, C_ACCENT);
        }

        if (anim_progress > 0.65f) {
            ImGui::SetCursorScreenPos(bp);
            ImGui::InvisibleButton((std::string("##kbb_") + id + label).c_str(), ImVec2(bw, bh));
            if (ImGui::IsItemClicked()) {
                key_listening = true;
                listening_key = key;
                key_press_debounce_time = ImGui::GetTime() + 0.1;
            }
        }
        if (listening && ImGui::GetTime() > key_press_debounce_time) {
            for (int i = 1; i < 256; i++) {
                if ((GetAsyncKeyState(i) & 0x8000) && i != last_listening_key) {
                    last_listening_key = i;
                    *key = (i == VK_ESCAPE) ? 0 : i;
                    key_listening = false;
                    listening_key = nullptr;
                    key_press_debounce_time = ImGui::GetTime() + 0.2;
                    break;
                }
            }
        }
        y += kb_step;
    }

    void Combo(const char* label, int* v, const std::vector<std::string>& items) {
        float cmb_step = get_cmb_step();
        if (anim_progress < 0.01f) { y += cmb_step; return; }
        dl->AddText(ImVec2(pos.x + pad_x, pos.y + y), C_TEXT, label);
        y += 16 * scale;
        ImVec2 bp(pos.x + pad_x, pos.y + y);
        float bw = w - pad_x * 2, bh = 26 * scale;

        std::string chk = std::string(id) + "_cmb_" + label;
        float& hv = AnimState(cmb_hover_anim, chk, 0.0f);
        bool bhov = ImGui::IsMouseHoveringRect(bp, ImVec2(bp.x + bw, bp.y + bh));
        hv = SmoothLerp(hv, bhov ? 1.0f : 0.0f, 20.0f, dt);

        ImU32 bg = LerpColor(C_BG_INPUT, ScaleAlpha(C_ACCENT, 0.2f), hv * 0.3f);
        dl->AddRectFilled(bp, ImVec2(bp.x + bw, bp.y + bh), bg, 3.0f);
        ImU32 br = LerpColor(C_BORDER, LerpColor(C_BORDER, C_ACCENT, 0.4f), hv);
        dl->AddRect(bp, ImVec2(bp.x + bw, bp.y + bh), br, 3.0f, 0, 1.0f);

        if (*v >= 0 && *v < (int)items.size()) {
            ImU32 txt_col = LerpColor(C_TEXT, C_ACCENT_LIGHT, hv * 0.3f);
            dl->AddText(ImVec2(bp.x + 10 * scale, bp.y + 6 * scale), txt_col, items[*v].c_str());
        }

        ImVec2 av(bp.x + bw - 10 * scale, bp.y + bh / 2 - 1);
        ImU32 arr_col = LerpColor(C_TEXT_DIM, C_ACCENT, hv * 0.5f);
        dl->AddTriangleFilled(ImVec2(av.x - 3, av.y - 2), ImVec2(av.x + 3, av.y - 2), ImVec2(av.x, av.y + 2.5f), arr_col);

        if (anim_progress > 0.65f) {
            ImGui::SetCursorScreenPos(bp);
            ImGui::InvisibleButton((std::string("##cmb_") + id + label).c_str(), ImVec2(bw, bh));
            if (ImGui::IsItemClicked()) *v = (*v + 1) % items.size();
        }
        y += bh + 8 * scale;
    }

    void Sld(const char* label, int* v, int vmin, int vmax, const char* suffix = "") {
        float sld_step = get_sld_step();
        if (anim_progress < 0.01f) { y += sld_step; return; }
        static std::map<std::string, int> sld_last_val;
        std::string sld_key = std::string(id) + "_sld_val_" + label;
        int& last_v = (sld_last_val.find(sld_key) == sld_last_val.end()) ? (sld_last_val[sld_key] = *v) : sld_last_val[sld_key];
        bool val_changed = (*v != last_v);
        if (val_changed) {
            last_v = *v;
            sld_last_change_time[sld_key] = ImGui::GetTime();
        }

        double time_since_change = ImGui::GetTime() - (sld_last_change_time.find(sld_key) != sld_last_change_time.end() ? sld_last_change_time[sld_key] : -10.0);
        float change_highlight = Clamp01(1.0f - (float)(time_since_change / 0.5f));

        dl->AddText(ImVec2(pos.x + pad_x, pos.y + y), C_TEXT, label);
        char val[32];
        sprintf_s(val, "%d%s", *v, suffix);
        ImVec2 vs = ImGui::CalcTextSize(val);
        ImU32 val_col = LerpColor(C_TEXT_DIM, C_ACCENT_LIGHT, change_highlight * 0.6f);
        dl->AddText(ImVec2(pos.x + w - vs.x - pad_x, pos.y + y), val_col, val);

        y += 16 * scale;
        float th = 3 * scale;
        ImVec2 tp(pos.x + pad_x, pos.y + y + 3 * scale);
        float tw = w - pad_x * 2;
        dl->AddRectFilled(tp, ImVec2(tp.x + tw, tp.y + th), C_BG_INPUT, 1.5f);
        float r = Clamp01((float)(*v - vmin) / (float)(vmax - vmin));
        dl->AddRectFilled(tp, ImVec2(tp.x + tw * r, tp.y + th), C_ACCENT, 1.5f);

        bool hovered = ImGui::IsMouseHoveringRect(ImVec2(tp.x, tp.y - 8 * scale), ImVec2(tp.x + tw, tp.y + 12 * scale));
        std::string sk = std::string(id) + "_sld_" + label;
        float& sa = AnimState(sld_hover_anim, sk, 1.0f);
        sa = SmoothLerp(sa, hovered ? 1.35f : 1.0f, 24.0f, dt);
        float knob = 5.0f * sa * scale;
        if (sa > 1.05f) {
            float glow = (sa - 1.0f) / 0.35f;
            dl->AddCircleFilled(ImVec2(tp.x + tw * r, tp.y + th / 2), knob + 3, ScaleAlpha(C_ACCENT, glow * 0.3f));
        }
        dl->AddCircleFilled(ImVec2(tp.x + tw * r, tp.y + th / 2), knob, C_WHITE);

        if (anim_progress > 0.65f) {
            ImGui::SetCursorScreenPos(ImVec2(tp.x, tp.y - 8 * scale));
            ImGui::InvisibleButton((std::string("##sld_") + id + label).c_str(), ImVec2(tw, 16 * scale));
            if (ImGui::IsItemActive()) {
                float mx = ImGui::GetIO().MousePos.x;
                *v = (int)(vmin + Clamp01((mx - tp.x) / tw) * (vmax - vmin));
            }
        }
        y += sld_step - 16 * scale;
    }
};

void RenderGui() {
    ApplyDarkTheme();
    float dt = ImGui::GetIO().DeltaTime;
    if (dt < 0.001f) dt = 0.016f;
    if (dt > 0.1f) dt = 0.1f;
    gui_fade_anim = SmoothLerp(gui_fade_anim, 1.0f, 8.0f, dt);

    menu_scale = Clamp(menu_scale, MIN_SCALE, MAX_SCALE);
    ImVec2 scaled_size(BASE_WINDOW_W * menu_scale, BASE_WINDOW_H * menu_scale);

    ImGui::SetNextWindowSize(scaled_size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(80, 50), ImGuiCond_FirstUseEver);

    int window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                       ImGuiWindowFlags_NoBackground;

    ImGui::Begin("##traceless", nullptr, window_flags);

    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();

    if (gui_visible && !gui_was_visible) {
        double cur_time = ImGui::GetTime();
        if (cur_time - cursor_last_move_time > 0.2) {
            ImVec2 center(wp.x + ws.x * 0.5f, wp.y + ws.y * 0.5f);
            MoveCursorToMenu((int)std::round(center.x), (int)std::round(center.y));
        }
        gui_was_visible = true;
    }
    if (!gui_visible) gui_was_visible = false;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    float fade = EaseOutCubic(gui_fade_anim);
    ImVec2 wp_offset(0, (1.0f - fade) * 14.0f * menu_scale);

    dl->AddRectFilled(ImVec2(wp.x, wp.y + wp_offset.y), ImVec2(wp.x + ws.x, wp.y + ws.y + wp_offset.y), C_BG_WIN, 10.0f);

    float HDR_H = 38.0f * menu_scale, TAB_H = 40.0f * menu_scale;
    dl->AddText(ImVec2(wp.x + 20 * menu_scale, wp.y + 12 * menu_scale + wp_offset.y), C_ACCENT, "severance");
    float sw_x = wp.x + 20 * menu_scale + ImGui::CalcTextSize("severance").x;
    dl->AddText(ImVec2(sw_x, wp.y + 12 * menu_scale + wp_offset.y), C_TEXT_DIM, ".today");

    const char* build_str = "build: nov 29 2025";
    ImVec2 bs = ImGui::CalcTextSize(build_str);
    dl->AddText(ImVec2(wp.x + ws.x - bs.x - 20 * menu_scale, wp.y + 12 * menu_scale + wp_offset.y), C_TEXT_DIM, build_str);

    if (active_tab != prev_active_tab) { tab_content_anim = 0.0f; prev_active_tab = active_tab; }
    tab_content_anim = SmoothLerp(tab_content_anim, 1.0f, 14.0f, dt);
    float off_x = (1.0f - EaseOutCubic(tab_content_anim)) * 26.0f * menu_scale;

    float content_y = wp.y + HDR_H + 8 * menu_scale + wp_offset.y;
    float pad = 14 * menu_scale, col_gap = 14 * menu_scale, v_gap = 12 * menu_scale;

    if (active_tab == 0) {
        float cw = (ws.x - pad * 2 - col_gap) / 2;
        float top = 360 * menu_scale, bot = 200 * menu_scale;
        { Card c(dl, ImVec2(wp.x + pad, content_y), cw, top, "aim", dt, off_x, menu_scale);
          c.Header("aim");
          c.Chk("enabled", &g_Options.LegitBot.AimBot.Enabled);
          c.KB("keybind", &g_Options.LegitBot.AimBot.KeyBind);
          c.Chk("target npcs", &g_Options.LegitBot.AimBot.TargetNPC);
          c.Chk("visible check", &g_Options.LegitBot.AimBot.VisibleCheck);
          static bool aim_fov_tog = false;
          c.ChkTog("show fov", &g_Options.Misc.Screen.ShowAimbotFov, &aim_fov_tog, IM_COL32(240, 240, 240, 255));
          c.Chk("prediction", &g_Options.LegitBot.AimBot.Prediction);
          static bool aim_bone = false;
          c.Chk("use closest bone", &aim_bone);
          static std::vector<std::string> hb = {"head", "neck", "chest", "pelvis"};
          c.Combo("hitbox", &g_Options.LegitBot.AimBot.HitBox, hb);
          c.Sld("max distance", &g_Options.LegitBot.AimBot.MaxDistance, 0, 1000, "m");
        }
        { Card c(dl, ImVec2(wp.x + pad + cw + col_gap, content_y), cw, top, "silent", dt, off_x, menu_scale);
          c.Header("silent");
          c.Chk("enabled", &g_Options.LegitBot.SilentAim.Enabled);
          c.KB("keybind", &g_Options.LegitBot.SilentAim.KeyBind);
          c.Chk("target npcs", &g_Options.LegitBot.SilentAim.ShotNPC);
          c.Chk("visible check", &g_Options.LegitBot.SilentAim.VisibleCheck);
          static bool sil_fov_tog = true;
          c.ChkTog("show fov", &g_Options.Misc.Screen.ShowSilentAimFov, &sil_fov_tog);
          static bool sil_line = true, sil_line_tog = true;
          c.ChkTog("aim line", &sil_line, &sil_line_tog);
          c.Chk("magic bullet", &g_Options.LegitBot.SilentAim.MagicBullet);
          static bool sil_bone = false;
          c.Chk("use closest bone", &sil_bone);
          static int sil_hb = 0;
          static std::vector<std::string> hb2 = {"head", "neck", "chest", "pelvis"};
          c.Combo("hitbox", &sil_hb, hb2);
          c.Sld("max distance", &g_Options.LegitBot.SilentAim.MaxDistance, 0, 1000, "m");
        }
        { Card c(dl, ImVec2(wp.x + pad, content_y + top + v_gap), cw, bot, "trigger", dt, off_x, menu_scale);
          c.Header("trigger");
          c.Chk("enabled", &g_Options.LegitBot.Trigger.Enabled);
          c.KB("keybind", &g_Options.LegitBot.Trigger.KeyBind);
          c.Chk("target npcs", &g_Options.LegitBot.Trigger.ShotNPC);
          c.Chk("visible check", &g_Options.LegitBot.Trigger.VisibleCheck);
          c.Sld("max distance", &g_Options.LegitBot.Trigger.MaxDistance, 0, 1000, "m");
        }
    }
    else if (active_tab == 1) {
        float cw = (ws.x - pad * 2 - col_gap) / 2;
        { Card c(dl, ImVec2(wp.x + pad, content_y), cw, 360 * menu_scale, "players", dt, off_x, menu_scale);
          c.Header("players");
          c.Chk("enabled", &g_Options.Visuals.ESP.Players.Enabled);
          c.Chk("box", &g_Options.Visuals.ESP.Players.Box);
          c.Chk("corner box", &g_Options.Visuals.ESP.Players.CornerBox);
          c.Chk("skeleton", &g_Options.Visuals.ESP.Players.Skeleton);
          c.Chk("head", &g_Options.Visuals.ESP.Players.Head);
          c.Chk("name", &g_Options.Visuals.ESP.Players.Name);
          c.Chk("health bar", &g_Options.Visuals.ESP.Players.HealthBar);
          c.Chk("armor bar", &g_Options.Visuals.ESP.Players.ArmorBar);
          c.Chk("weapon name", &g_Options.Visuals.ESP.Players.WeaponName);
          c.Chk("distance", &g_Options.Visuals.ESP.Players.Distance);
          c.Chk("snap lines", &g_Options.Visuals.ESP.Players.SnapLines);
        }
        { Card c(dl, ImVec2(wp.x + pad + cw + col_gap, content_y), cw, 200 * menu_scale, "vehicles", dt, off_x, menu_scale);
          c.Header("vehicles");
          c.Chk("enabled", &g_Options.Visuals.ESP.Vehicles.Enabled);
          c.Chk("ignore occupied", &g_Options.Visuals.ESP.Vehicles.IgnoreOccupiedVehicles);
          c.Chk("name", &g_Options.Visuals.ESP.Vehicles.Name);
          c.Chk("distance", &g_Options.Visuals.ESP.Vehicles.Distance);
          c.Chk("marker", &g_Options.Visuals.ESP.Vehicles.Marker);
        }
        { Card c(dl, ImVec2(wp.x + pad + cw + col_gap, content_y + 200 * menu_scale + v_gap), cw, 148 * menu_scale, "settings_esp", dt, off_x, menu_scale);
          c.Header("settings");
          c.Chk("show local player", &g_Options.Visuals.ESP.Players.ShowLocalPlayer);
          c.Chk("show npcs", &g_Options.Visuals.ESP.Players.ShowNPCs);
          c.Chk("visible only", &g_Options.Visuals.ESP.Players.VisibleOnly);
          c.Sld("render distance", &g_Options.Visuals.ESP.Players.RenderDistance, 0, 1000, "m");
        }
    }
    else if (active_tab == 2) {
        float cw = (ws.x - pad * 2 - col_gap) / 2;
        Card c(dl, ImVec2(wp.x + pad, content_y), cw, 200 * menu_scale, "screen", dt, off_x, menu_scale);
        c.Header("screen");
        c.Chk("watermark", &g_Options.Misc.Screen.EnableWatermark);
        c.Chk("keybind list", &g_Options.Misc.Screen.EnableKeybindList);
        c.Chk("aimbot fov", &g_Options.Misc.Screen.ShowAimbotFov);
        c.Chk("silent fov", &g_Options.Misc.Screen.ShowSilentAimFov);
        c.Chk("trigger fov", &g_Options.Misc.Screen.ShowTriggerFov);
    }
    else if (active_tab == 3) {
        float cw = (ws.x - pad * 2 - col_gap) / 2;
        { Card c(dl, ImVec2(wp.x + pad, content_y), cw, 240 * menu_scale, "local", dt, off_x, menu_scale);
          c.Header("local player");
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
        { Card c(dl, ImVec2(wp.x + pad + cw + col_gap, content_y), cw, 240 * menu_scale, "weapon", dt, off_x, menu_scale);
          c.Header("weapon");
          c.Chk("no reload", &g_Options.Exploits.Weapon.NoReload);
          c.Chk("no recoil", &g_Options.Exploits.Weapon.NoRecoil);
          c.Chk("no spread", &g_Options.Exploits.Weapon.NoSpread);
          c.Chk("rapid fire", &g_Options.Exploits.Weapon.RapidFire);
          c.Chk("one shot kill", &g_Options.Exploits.Weapon.OneShotKill);
          c.Chk("infinite ammo", &g_Options.Exploits.Weapon.InfiniteAmmo);
        }
    }
    else if (active_tab == 4) {
        dl->AddText(ImVec2(wp.x + pad + 8 * menu_scale + off_x, content_y + 12 * menu_scale), C_TEXT_DIM, "lists coming soon...");
    }
    else if (active_tab == 5) {
        float cw = (ws.x - pad * 2 - col_gap) / 2;
        {
            Card c(dl, ImVec2(wp.x + pad, content_y), cw, 280 * menu_scale, "general", dt, off_x, menu_scale);
            c.Header("general");
            c.Chk("capture bypass", &g_Options.General.CaptureBypass);
            c.Chk("legit mode", &g_Options.Misc.Other.legit_mode);
            c.Chk("anti screenshot", &g_Options.Misc.Other.anti_screenshot);
            c.Sld("thread delay", &g_Options.General.ThreadDelay, 0, 32);
        }
        {
            Card c(dl, ImVec2(wp.x + pad + cw + col_gap, content_y), cw, 280 * menu_scale, "menu", dt, off_x, menu_scale);
            c.Header("menu");
            menu_scale_pct = (int)std::round(menu_scale * 100.0f);
            c.Sld("scale", &menu_scale_pct, 60, 150, "%");
            menu_scale = Clamp(menu_scale_pct / 100.0f, MIN_SCALE, MAX_SCALE);

            c.Chk("pause game", &pause_game);

            c.y += 8 * menu_scale;
            if (c.anim_progress > 0.65f) {
                ImVec2 btn_pos(c.pos.x + c.pad_x, c.pos.y + c.y);
                float btn_w = c.w - c.pad_x * 2;
                float btn_h = 28 * menu_scale;
                dl->AddRectFilled(btn_pos, ImVec2(btn_pos.x + btn_w, btn_pos.y + btn_h),
                                  IM_COL32(180, 50, 50, 200), 4.0f);
                dl->AddRect(btn_pos, ImVec2(btn_pos.x + btn_w, btn_pos.y + btn_h),
                            IM_COL32(200, 60, 60, 255), 4.0f, 0, 1.0f);

                const char* unload_text = "UNLOAD CHEAT";
                ImVec2 text_size = ImGui::CalcTextSize(unload_text);
                dl->AddText(ImVec2(btn_pos.x + (btn_w - text_size.x) * 0.5f, btn_pos.y + (btn_h - text_size.y) * 0.5f),
                            IM_COL32(255, 255, 255, 255), unload_text);

                ImGui::SetCursorScreenPos(btn_pos);
                ImGui::InvisibleButton("##unload_btn", ImVec2(btn_w, btn_h));
                if (ImGui::IsItemClicked()) {
                    double cur_time = ImGui::GetTime();
                    if (cur_time - unload_last_press_time > 0.3) {
                        should_unload = true;
                        unload_last_press_time = cur_time;
                    }
                }
            }
        }
    }

    float tab_y = wp.y + ws.y - TAB_H + wp_offset.y;
    dl->AddLine(ImVec2(wp.x + 14 * menu_scale, tab_y), ImVec2(wp.x + ws.x - 14 * menu_scale, tab_y), C_BORDER_SOFT, 1.0f);

    const char* tab_names[] = {"aimbot", "visuals", "overlays", "exploits", "lists", "settings"};
    typedef void (*IconFn)(ImDrawList*, ImVec2, ImU32);
    IconFn icons[] = {DrawCrosshairIcon, DrawEyeIcon, DrawGlobeIcon, DrawExploitsIcon, DrawListsIcon, DrawSettingsIcon};

    float cx = wp.x + 22 * menu_scale;
    float a_l = 0, a_r = 0;
    for (int i = 0; i < 6; i++) {
        float tab_target = (active_tab == i) ? 1.0f : 0.0f;
        float& tab_a = AnimState(sld_hover_anim, std::string("tab_") + tab_names[i], tab_target);
        tab_a = SmoothLerp(tab_a, tab_target, 12.0f, dt);

        ImU32 col = LerpColor(C_TEXT_DIM, C_ACCENT, tab_a);
        float icon_scale = 1.0f + tab_a * 0.15f;
        ImGui::SetCursorScreenPos(ImVec2(cx + 9 * menu_scale, tab_y + TAB_H / 2 + 1));
        ImGui::InvisibleButton((std::string("##icon_") + tab_names[i]).c_str(), ImVec2(14 * menu_scale, 14 * menu_scale));
        ImVec2 ic_center(cx + 9 * menu_scale + 7 * menu_scale, tab_y + TAB_H / 2 + 1 + 7 * menu_scale);
        dl->PushClipRect(ImVec2(ic_center.x - 7 * icon_scale * menu_scale - 1, ic_center.y - 7 * icon_scale * menu_scale - 1),
                         ImVec2(ic_center.x + 7 * icon_scale * menu_scale + 1, ic_center.y + 7 * icon_scale * menu_scale + 1), true);
        icons[i](dl, ImVec2(cx + 9 * menu_scale, tab_y + TAB_H / 2 + 1), col);
        dl->PopClipRect();
        ImVec2 ts = ImGui::CalcTextSize(tab_names[i]);
        dl->AddText(ImVec2(cx + 24 * menu_scale, tab_y + (TAB_H - ts.y) / 2), col, tab_names[i]);
        float tw = 24 * menu_scale + ts.x + 24 * menu_scale;
        if (active_tab == i) { a_l = cx; a_r = cx + 24 * menu_scale + ts.x; }
        ImGui::SetCursorScreenPos(ImVec2(cx, tab_y));
        ImGui::InvisibleButton((std::string("##tab_") + tab_names[i]).c_str(), ImVec2(tw, TAB_H));
        if (ImGui::IsItemClicked()) active_tab = i;
        cx += tw;
    }

    if (a_l != 0) {
        if (!tab_indicator_init) {
            tab_indicator_x = a_l; tab_indicator_width = a_r - a_l; tab_indicator_init = true;
        } else {
            tab_indicator_anim = SmoothLerp(tab_indicator_anim, 1.0f, 16.0f, dt);
            tab_indicator_x = SmoothLerp(tab_indicator_x, a_l, 16.0f, dt);
            tab_indicator_width = SmoothLerp(tab_indicator_width, a_r - a_l, 16.0f, dt);
        }
        dl->AddLine(ImVec2(tab_indicator_x, tab_y + TAB_H - 2),
                    ImVec2(tab_indicator_x + tab_indicator_width, tab_y + TAB_H - 2), C_ACCENT, 2.0f);
    }

    const char* brand = "severance";
    ImVec2 br = ImGui::CalcTextSize(brand);
    dl->AddText(ImVec2(wp.x + ws.x - br.x - 22 * menu_scale, tab_y + (TAB_H - br.y) / 2), C_TEXT_DIM, brand);

    ImGui::End();
}

}
}
