#include "gui.hpp"
#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <imgui.h>
#include <map>
#include <string>
#include <vector>

namespace FrameWork {
namespace render_ui {

static std::map<std::string, bool> section_open;
static int active_tab = 0;
static bool key_listening = false;
static int* listening_key = nullptr;

static const ImU32 C_ACCENT      = IM_COL32(220, 45, 45, 255);
static const ImU32 C_ACCENT_DIM  = IM_COL32(180, 35, 35, 255);
static const ImU32 C_BG_WIN      = IM_COL32(18, 18, 22, 245);
static const ImU32 C_BG_CARD     = IM_COL32(22, 22, 26, 235);
static const ImU32 C_BG_HEADER   = IM_COL32(28, 28, 34, 255);
static const ImU32 C_BG_INPUT    = IM_COL32(14, 14, 18, 255);
static const ImU32 C_BORDER      = IM_COL32(45, 45, 52, 255);
static const ImU32 C_BORDER_SOFT = IM_COL32(35, 35, 42, 255);
static const ImU32 C_TEXT        = IM_COL32(210, 210, 215, 255);
static const ImU32 C_TEXT_DIM    = IM_COL32(130, 130, 140, 255);
static const ImU32 C_TEXT_BRIGHT = IM_COL32(240, 240, 245, 255);
static const ImU32 C_WHITE       = IM_COL32(245, 245, 245, 255);

static std::string KeyCodeToString(int key) {
    if (key == 0) return "none";
    switch (key) {
        case VK_LBUTTON: return "lmb";
        case VK_RBUTTON: return "rmb";
        case VK_MBUTTON: return "mmb";
        case VK_XBUTTON1: return "m4";
        case VK_XBUTTON2: return "m5";
        case VK_SHIFT: return "shift";
        case VK_CONTROL: return "ctrl";
        case VK_MENU: return "alt";
        case VK_SPACE: return "space";
        case VK_RETURN: return "enter";
        case VK_ESCAPE: return "esc";
        case VK_TAB: return "tab";
        default:
            if (key >= 'A' && key <= 'Z') {
                static char buf[2] = {0, 0};
                buf[0] = (char)(key + 32);
                return std::string(buf);
            }
            if (key >= VK_F1 && key <= VK_F12) {
                static char buf[4];
                sprintf_s(buf, sizeof(buf), "f%d", key - VK_F1 + 1);
                return std::string(buf);
            }
            if (key >= '0' && key <= '9') {
                static char buf[2] = {0, 0};
                buf[0] = (char)key;
                return std::string(buf);
            }
            return "key";
    }
}

void ApplyDarkTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 8.0f;
    s.FrameRounding = 4.0f;
    s.GrabRounding = 4.0f;
    s.PopupRounding = 4.0f;
    s.ChildRounding = 6.0f;
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = ImVec2(4, 3);
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.085f, 0.96f);
    s.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    s.Colors[ImGuiCol_Border] = ImVec4(0.18f, 0.18f, 0.21f, 1.0f);
}

static void DrawKeyboardIcon(ImDrawList* dl, ImVec2 pos, ImU32 col) {
    float w = 18.0f, h = 12.0f;
    dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), col, 2.0f, 0, 1.0f);
    for (int row = 0; row < 2; row++) {
        for (int c = 0; c < 4; c++) {
            float x = pos.x + 2.5f + c * 3.7f;
            float y = pos.y + 2.5f + row * 3.5f;
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 2.0f, y + 2.0f), col);
        }
    }
}

static void DrawCrosshairIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c, 7.0f, col, 16, 1.5f);
    dl->AddLine(ImVec2(c.x - 10, c.y), ImVec2(c.x - 3, c.y), col, 1.5f);
    dl->AddLine(ImVec2(c.x + 3, c.y), ImVec2(c.x + 10, c.y), col, 1.5f);
    dl->AddLine(ImVec2(c.x, c.y - 10), ImVec2(c.x, c.y - 3), col, 1.5f);
    dl->AddLine(ImVec2(c.x, c.y + 3), ImVec2(c.x, c.y + 10), col, 1.5f);
}

static void DrawEyeIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddBezierCubic(ImVec2(c.x - 10, c.y), ImVec2(c.x - 5, c.y - 6), ImVec2(c.x + 5, c.y - 6), ImVec2(c.x + 10, c.y), col, 1.5f);
    dl->AddBezierCubic(ImVec2(c.x - 10, c.y), ImVec2(c.x - 5, c.y + 6), ImVec2(c.x + 5, c.y + 6), ImVec2(c.x + 10, c.y), col, 1.5f);
    dl->AddCircle(c, 3.0f, col, 12, 1.2f);
}

static void DrawGlobeIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c, 9.0f, col, 24, 1.5f);
    dl->AddLine(ImVec2(c.x - 9, c.y), ImVec2(c.x + 9, c.y), col, 1.2f);
    dl->AddBezierCubic(ImVec2(c.x, c.y - 9), ImVec2(c.x - 5, c.y - 3), ImVec2(c.x - 5, c.y + 3), ImVec2(c.x, c.y + 9), col, 1.2f);
    dl->AddBezierCubic(ImVec2(c.x, c.y - 9), ImVec2(c.x + 5, c.y - 3), ImVec2(c.x + 5, c.y + 3), ImVec2(c.x, c.y + 9), col, 1.2f);
}

static void DrawExploitsIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddLine(ImVec2(c.x - 9, c.y - 4), ImVec2(c.x + 7, c.y - 4), col, 1.5f);
    dl->AddLine(ImVec2(c.x + 7, c.y - 4), ImVec2(c.x + 4, c.y - 7), col, 1.5f);
    dl->AddLine(ImVec2(c.x + 7, c.y - 4), ImVec2(c.x + 4, c.y - 1), col, 1.5f);
    dl->AddLine(ImVec2(c.x + 9, c.y + 4), ImVec2(c.x - 7, c.y + 4), col, 1.5f);
    dl->AddLine(ImVec2(c.x - 7, c.y + 4), ImVec2(c.x - 4, c.y + 1), col, 1.5f);
    dl->AddLine(ImVec2(c.x - 7, c.y + 4), ImVec2(c.x - 4, c.y + 7), col, 1.5f);
}

static void DrawListsIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    DrawKeyboardIcon(dl, ImVec2(c.x - 9, c.y - 6), col);
}

static void DrawSettingsIcon(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c, 8.0f, col, 20, 1.5f);
    dl->AddCircle(c, 3.0f, col, 12, 1.2f);
    for (int i = 0; i < 8; i++) {
        float a = i * 6.2831f / 8;
        ImVec2 p1(c.x + cosf(a) * 8.0f, c.y + sinf(a) * 8.0f);
        ImVec2 p2(c.x + cosf(a) * 10.5f, c.y + sinf(a) * 10.5f);
        dl->AddLine(p1, p2, col, 1.5f);
    }
}

struct Card {
    ImDrawList* dl;
    ImVec2 pos;
    float w, h;
    float y;
    const char* id;
    bool open;

    Card(ImDrawList* d, ImVec2 p, float width, float height, const char* sid)
        : dl(d), pos(p), w(width), h(height), y(0), id(sid) {
        if (section_open.find(sid) == section_open.end())
            section_open[sid] = true;
        open = section_open[sid];

        dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), C_BG_CARD, 6.0f);
        dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), C_BORDER_SOFT, 6.0f, 0, 1.0f);
    }

    void Header(const char* label) {
        float hh = 44.0f;
        dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + hh), C_BG_HEADER, 6.0f, ImDrawFlags_RoundCornersTop);
        dl->AddLine(ImVec2(pos.x, pos.y + hh), ImVec2(pos.x + w, pos.y + hh), C_BORDER_SOFT, 1.0f);
        dl->AddText(ImVec2(pos.x + 18, pos.y + 14), C_TEXT_BRIGHT, label);

        ImVec2 cv(pos.x + w - 22, pos.y + 18);
        if (open) {
            dl->AddTriangleFilled(ImVec2(cv.x - 5, cv.y), ImVec2(cv.x + 5, cv.y), ImVec2(cv.x, cv.y + 5), C_TEXT_DIM);
        } else {
            dl->AddTriangleFilled(ImVec2(cv.x, cv.y - 5), ImVec2(cv.x + 5, cv.y), ImVec2(cv.x, cv.y + 5), C_TEXT_DIM);
        }

        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton((std::string("##hdr_") + id).c_str(), ImVec2(w, hh));
        if (ImGui::IsItemClicked()) {
            section_open[id] = !section_open[id];
            open = section_open[id];
        }
        y = hh + 14;
    }

    bool Chk(const char* label, bool* v) {
        if (!open) return false;
        float sz = 14.0f;
        ImVec2 cp(pos.x + 18, pos.y + y);
        dl->AddRect(cp, ImVec2(cp.x + sz, cp.y + sz), *v ? C_ACCENT : C_BORDER, 3.0f, 0, 1.0f);
        if (*v) {
            dl->AddRectFilled(ImVec2(cp.x + 2, cp.y + 2), ImVec2(cp.x + sz - 2, cp.y + sz - 2), C_ACCENT, 2.0f);
            dl->AddLine(ImVec2(cp.x + 3, cp.y + 7), ImVec2(cp.x + 6, cp.y + 10), C_WHITE, 1.6f);
            dl->AddLine(ImVec2(cp.x + 6, cp.y + 10), ImVec2(cp.x + 11, cp.y + 4), C_WHITE, 1.6f);
        }
        dl->AddText(ImVec2(cp.x + sz + 10, cp.y - 1), C_TEXT, label);

        ImGui::SetCursorScreenPos(ImVec2(pos.x + 14, pos.y + y - 4));
        ImGui::InvisibleButton((std::string("##chk_") + id + label).c_str(), ImVec2(w - 28, 24));
        if (ImGui::IsItemClicked()) *v = !*v;
        y += 32;
        return true;
    }

    bool ChkTog(const char* label, bool* v, bool* tog, ImU32 tog_color = 0) {
        if (!open) return false;
        float sz = 14.0f;
        ImVec2 cp(pos.x + 18, pos.y + y);
        dl->AddRect(cp, ImVec2(cp.x + sz, cp.y + sz), *v ? C_ACCENT : C_BORDER, 3.0f, 0, 1.0f);
        if (*v) {
            dl->AddRectFilled(ImVec2(cp.x + 2, cp.y + 2), ImVec2(cp.x + sz - 2, cp.y + sz - 2), C_ACCENT, 2.0f);
            dl->AddLine(ImVec2(cp.x + 3, cp.y + 7), ImVec2(cp.x + 6, cp.y + 10), C_WHITE, 1.6f);
            dl->AddLine(ImVec2(cp.x + 6, cp.y + 10), ImVec2(cp.x + 11, cp.y + 4), C_WHITE, 1.6f);
        }
        dl->AddText(ImVec2(cp.x + sz + 10, cp.y - 1), C_TEXT, label);

        ImGui::SetCursorScreenPos(ImVec2(pos.x + 14, pos.y + y - 4));
        ImGui::InvisibleButton((std::string("##chk_") + id + label).c_str(), ImVec2(w - 100, 24));
        if (ImGui::IsItemClicked()) *v = !*v;

        ImVec2 tp(pos.x + w - 42, pos.y + y - 2);
        float tw = 26, th = 16;
        ImU32 fillCol = *tog ? (tog_color ? tog_color : C_ACCENT) : C_BG_INPUT;
        dl->AddRectFilled(tp, ImVec2(tp.x + tw, tp.y + th), fillCol, 3.0f);
        dl->AddRect(tp, ImVec2(tp.x + tw, tp.y + th), C_BORDER, 3.0f, 0, 1.0f);
        ImGui::SetCursorScreenPos(tp);
        ImGui::InvisibleButton((std::string("##tog_") + id + label).c_str(), ImVec2(tw, th));
        if (ImGui::IsItemClicked()) *tog = !*tog;
        y += 32;
        return true;
    }

    void KB(const char* label, int* key) {
        if (!open) return;
        ImVec2 lp(pos.x + 18, pos.y + y + 4);
        dl->AddText(lp, C_TEXT, label);

        float bw = 88, bh = 26;
        ImVec2 bp(pos.x + w - bw - 18, pos.y + y);
        dl->AddRectFilled(bp, ImVec2(bp.x + bw, bp.y + bh), C_BG_INPUT, 4.0f);
        dl->AddRect(bp, ImVec2(bp.x + bw, bp.y + bh), C_BORDER, 4.0f, 0, 1.0f);

        DrawKeyboardIcon(dl, ImVec2(bp.x + 8, bp.y + 7), C_ACCENT);

        std::string s = (key_listening && listening_key == key) ? "..." : KeyCodeToString(*key);
        ImVec2 ts = ImGui::CalcTextSize(s.c_str());
        dl->AddText(ImVec2(bp.x + 34, bp.y + (bh - ts.y) / 2), C_TEXT, s.c_str());

        ImGui::SetCursorScreenPos(bp);
        ImGui::InvisibleButton((std::string("##kb_") + id + label).c_str(), ImVec2(bw, bh));
        if (ImGui::IsItemClicked()) {
            key_listening = true;
            listening_key = key;
        }
        if (key_listening && listening_key == key) {
            for (int i = 1; i < 256; i++) {
                if (GetAsyncKeyState(i) & 0x8000) {
                    *key = (i == VK_ESCAPE) ? 0 : i;
                    key_listening = false;
                    listening_key = nullptr;
                    break;
                }
            }
        }
        y += 36;
    }

    void Combo(const char* label, int* v, const std::vector<std::string>& items) {
        if (!open) return;
        dl->AddText(ImVec2(pos.x + 18, pos.y + y), C_TEXT, label);
        y += 22;
        ImVec2 bp(pos.x + 18, pos.y + y);
        float bw = w - 36, bh = 26;
        dl->AddRectFilled(bp, ImVec2(bp.x + bw, bp.y + bh), C_BG_INPUT, 4.0f);
        dl->AddRect(bp, ImVec2(bp.x + bw, bp.y + bh), C_BORDER, 4.0f, 0, 1.0f);
        if (*v >= 0 && *v < (int)items.size())
            dl->AddText(ImVec2(bp.x + 10, bp.y + 6), C_TEXT, items[*v].c_str());

        ImVec2 av(bp.x + bw - 14, bp.y + 10);
        dl->AddTriangleFilled(ImVec2(av.x - 4, av.y), ImVec2(av.x + 4, av.y), ImVec2(av.x, av.y + 5), C_TEXT_DIM);

        ImGui::SetCursorScreenPos(bp);
        ImGui::InvisibleButton((std::string("##cmb_") + id + label).c_str(), ImVec2(bw, bh));
        if (ImGui::IsItemClicked()) *v = (*v + 1) % items.size();
        y += bh + 12;
    }

    void Sld(const char* label, int* v, int vmin, int vmax, const char* suffix = "") {
        if (!open) return;
        ImVec2 lp(pos.x + 18, pos.y + y);
        dl->AddText(lp, C_TEXT, label);

        char val[32];
        sprintf_s(val, "%d%s", *v, suffix);
        ImVec2 vs = ImGui::CalcTextSize(val);
        dl->AddText(ImVec2(pos.x + w - vs.x - 18, pos.y + y), C_TEXT_DIM, val);

        y += 20;
        ImVec2 tp(pos.x + 18, pos.y + y);
        float tw = w - 36, th = 4;
        dl->AddRectFilled(tp, ImVec2(tp.x + tw, tp.y + th), C_BG_INPUT, 2.0f);
        float r = (float)(*v - vmin) / (vmax - vmin);
        if (r < 0) r = 0; if (r > 1) r = 1;
        dl->AddRectFilled(tp, ImVec2(tp.x + tw * r, tp.y + th), C_ACCENT, 2.0f);
        dl->AddCircleFilled(ImVec2(tp.x + tw * r, tp.y + th / 2), 6.0f, C_WHITE);

        ImGui::SetCursorScreenPos(ImVec2(tp.x, tp.y - 6));
        ImGui::InvisibleButton((std::string("##sld_") + id + label).c_str(), ImVec2(tw, 18));
        if (ImGui::IsItemActive()) {
            float mx = ImGui::GetIO().MousePos.x;
            float nr = (mx - tp.x) / tw;
            if (nr < 0) nr = 0; if (nr > 1) nr = 1;
            *v = (int)(vmin + nr * (vmax - vmin));
        }
        y += 22;
    }
};

void RenderGui() {
    ApplyDarkTheme();

    ImGui::SetNextWindowSize(ImVec2(1080, 780), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(80, 60), ImGuiCond_FirstUseEver);
    ImGui::Begin("##traceless", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();

    dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), C_BG_WIN, 8.0f);
    dl->AddRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), C_BORDER, 8.0f, 0, 1.0f);

    float HDR_H = 44.0f;
    float TAB_H = 52.0f;

    dl->AddText(ImVec2(wp.x + 22, wp.y + 16), C_ACCENT, "severance");
    float sw_x = wp.x + 22 + ImGui::CalcTextSize("severance").x;
    dl->AddText(ImVec2(sw_x, wp.y + 16), C_TEXT_DIM, ".today");

    const char* build_str = "build: nov 29 2025";
    ImVec2 bs = ImGui::CalcTextSize(build_str);
    dl->AddText(ImVec2(wp.x + ws.x - bs.x - 22, wp.y + 16), C_TEXT_DIM, build_str);

    dl->AddLine(ImVec2(wp.x, wp.y + HDR_H), ImVec2(wp.x + ws.x, wp.y + HDR_H), C_BORDER_SOFT, 1.0f);

    float content_y = wp.y + HDR_H + 14;
    float content_h = ws.y - HDR_H - TAB_H - 14;
    float pad = 14.0f;
    float col_gap = 14.0f;

    if (active_tab == 0) {
        float card_w = (ws.x - pad * 2 - col_gap) / 2;
        float top_card_h = 540;
        float bot_card_h = 280;

        {
            Card c(dl, ImVec2(wp.x + pad, content_y), card_w, top_card_h, "aim");
            c.Header("aim");
            if (c.open) {
                c.Chk("enabled", &g_Options.LegitBot.AimBot.Enabled);
                c.KB("keybind", &g_Options.LegitBot.AimBot.KeyBind);
                c.Chk("target npcs", &g_Options.LegitBot.AimBot.TargetNPC);
                c.Chk("visible check", &g_Options.LegitBot.AimBot.VisibleCheck);
                static bool aim_fov_tog = false;
                c.ChkTog("show fov", &g_Options.Misc.Screen.ShowAimbotFov, &aim_fov_tog, IM_COL32(245, 245, 245, 255));
                c.Chk("prediction", &g_Options.LegitBot.AimBot.Prediction);
                static bool aim_bone = false;
                c.Chk("use closest bone", &aim_bone);
                std::vector<std::string> hb = {"head", "neck", "chest", "pelvis"};
                c.Combo("hitbox", &g_Options.LegitBot.AimBot.HitBox, hb);
                c.Sld("max distance", &g_Options.LegitBot.AimBot.MaxDistance, 0, 1000, "m");
            }
        }

        {
            Card c(dl, ImVec2(wp.x + pad + card_w + col_gap, content_y), card_w, top_card_h, "silent");
            c.Header("silent");
            if (c.open) {
                c.Chk("enabled", &g_Options.LegitBot.SilentAim.Enabled);
                c.KB("keybind", &g_Options.LegitBot.SilentAim.KeyBind);
                c.Chk("target npcs", &g_Options.LegitBot.SilentAim.ShotNPC);
                c.Chk("visible check", &g_Options.LegitBot.SilentAim.VisibleCheck);
                static bool sil_fov_tog = true;
                c.ChkTog("show fov", &g_Options.Misc.Screen.ShowSilentAimFov, &sil_fov_tog);
                static bool sil_line = true;
                static bool sil_line_tog = true;
                c.ChkTog("aim line", &sil_line, &sil_line_tog);
                c.Chk("magic bullet", &g_Options.LegitBot.SilentAim.MagicBullet);
                static bool sil_bone = false;
                c.Chk("use closest bone", &sil_bone);
                static int sil_hb = 0;
                std::vector<std::string> hb = {"head", "neck", "chest", "pelvis"};
                c.Combo("hitbox", &sil_hb, hb);
                c.Sld("max distance", &g_Options.LegitBot.SilentAim.MaxDistance, 0, 1000, "m");
            }
        }

        {
            Card c(dl, ImVec2(wp.x + pad, content_y + top_card_h + col_gap), card_w, bot_card_h, "trigger");
            c.Header("trigger");
            if (c.open) {
                c.Chk("enabled", &g_Options.LegitBot.Trigger.Enabled);
                c.KB("keybind", &g_Options.LegitBot.Trigger.KeyBind);
                c.Chk("target npcs", &g_Options.LegitBot.Trigger.ShotNPC);
                c.Chk("visible check", &g_Options.LegitBot.Trigger.VisibleCheck);
                c.Sld("max distance", &g_Options.LegitBot.Trigger.MaxDistance, 0, 1000, "m");
            }
        }
    }
    else if (active_tab == 1) {
        float card_w = (ws.x - pad * 2 - col_gap) / 2;
        {
            Card c(dl, ImVec2(wp.x + pad, content_y), card_w, 500, "players");
            c.Header("players");
            if (c.open) {
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
        }
        {
            Card c(dl, ImVec2(wp.x + pad + card_w + col_gap, content_y), card_w, 320, "vehicles");
            c.Header("vehicles");
            if (c.open) {
                c.Chk("enabled", &g_Options.Visuals.ESP.Vehicles.Enabled);
                c.Chk("ignore occupied", &g_Options.Visuals.ESP.Vehicles.IgnoreOccupiedVehicles);
                c.Chk("name", &g_Options.Visuals.ESP.Vehicles.Name);
                c.Chk("distance", &g_Options.Visuals.ESP.Vehicles.Distance);
                c.Chk("marker", &g_Options.Visuals.ESP.Vehicles.Marker);
            }
        }
        {
            Card c(dl, ImVec2(wp.x + pad + card_w + col_gap, content_y + 320 + col_gap), card_w, 170, "settings_esp");
            c.Header("settings");
            if (c.open) {
                c.Chk("show local player", &g_Options.Visuals.ESP.Players.ShowLocalPlayer);
                c.Chk("show npcs", &g_Options.Visuals.ESP.Players.ShowNPCs);
                c.Chk("visible only", &g_Options.Visuals.ESP.Players.VisibleOnly);
                c.Sld("render distance", &g_Options.Visuals.ESP.Players.RenderDistance, 0, 1000, "m");
            }
        }
    }
    else if (active_tab == 2) {
        float card_w = (ws.x - pad * 2 - col_gap) / 2;
        Card c(dl, ImVec2(wp.x + pad, content_y), card_w, 260, "screen");
        c.Header("screen");
        if (c.open) {
            c.Chk("watermark", &g_Options.Misc.Screen.EnableWatermark);
            c.Chk("keybind list", &g_Options.Misc.Screen.EnableKeybindList);
            c.Chk("aimbot fov", &g_Options.Misc.Screen.ShowAimbotFov);
            c.Chk("silent fov", &g_Options.Misc.Screen.ShowSilentAimFov);
            c.Chk("trigger fov", &g_Options.Misc.Screen.ShowTriggerFov);
        }
    }
    else if (active_tab == 3) {
        float card_w = (ws.x - pad * 2 - col_gap) / 2;
        {
            Card c(dl, ImVec2(wp.x + pad, content_y), card_w, 320, "local");
            c.Header("local player");
            if (c.open) {
                c.Chk("god mode", &g_Options.Exploits.LocalPlayer.God);
                c.Chk("noclip", &g_Options.Exploits.LocalPlayer.Noclip);
                c.Chk("invisible", &g_Options.Exploits.LocalPlayer.Invisible);
                c.Chk("shrink", &g_Options.Exploits.LocalPlayer.Shrink);
                c.Chk("speed hack", &g_Options.Exploits.LocalPlayer.speed);
                static int spd = 5;
                c.Sld("player speed", &spd, 1, 10);
                g_Options.Exploits.LocalPlayer.Player_speed = (float)spd;
            }
        }
        {
            Card c(dl, ImVec2(wp.x + pad + card_w + col_gap, content_y), card_w, 320, "weapon");
            c.Header("weapon");
            if (c.open) {
                c.Chk("no reload", &g_Options.Exploits.Weapon.NoReload);
                c.Chk("no recoil", &g_Options.Exploits.Weapon.NoRecoil);
                c.Chk("no spread", &g_Options.Exploits.Weapon.NoSpread);
                c.Chk("rapid fire", &g_Options.Exploits.Weapon.RapidFire);
                c.Chk("one shot kill", &g_Options.Exploits.Weapon.OneShotKill);
                c.Chk("infinite ammo", &g_Options.Exploits.Weapon.InfiniteAmmo);
            }
        }
    }
    else if (active_tab == 4) {
        dl->AddText(ImVec2(wp.x + pad + 8, content_y + 12), C_TEXT_DIM, "lists coming soon...");
    }
    else if (active_tab == 5) {
        float card_w = (ws.x - pad * 2 - col_gap) / 2;
        {
            Card c(dl, ImVec2(wp.x + pad, content_y), card_w, 240, "general");
            c.Header("general");
            if (c.open) {
                c.Chk("capture bypass", &g_Options.General.CaptureBypass);
                c.Chk("legit mode", &g_Options.Misc.Other.legit_mode);
                c.Chk("anti screenshot", &g_Options.Misc.Other.anti_screenshot);
                c.Sld("thread delay", &g_Options.General.ThreadDelay, 0, 32);
            }
        }
    }

    float tab_y = wp.y + ws.y - TAB_H;
    dl->AddLine(ImVec2(wp.x, tab_y), ImVec2(wp.x + ws.x, tab_y), C_BORDER_SOFT, 1.0f);

    const char* tab_names[] = {"aimbot", "visuals", "overlays", "exploits", "lists", "settings"};
    typedef void (*IconFn)(ImDrawList*, ImVec2, ImU32);
    IconFn icons[] = {DrawCrosshairIcon, DrawEyeIcon, DrawGlobeIcon, DrawExploitsIcon, DrawListsIcon, DrawSettingsIcon};

    float cursor_x = wp.x + 22;
    for (int i = 0; i < 6; i++) {
        ImU32 col = (active_tab == i) ? C_ACCENT : C_TEXT_DIM;
        ImVec2 ic_pos(cursor_x + 12, tab_y + TAB_H / 2);
        icons[i](dl, ic_pos, col);
        ImVec2 ts = ImGui::CalcTextSize(tab_names[i]);
        dl->AddText(ImVec2(cursor_x + 28, tab_y + (TAB_H - ts.y) / 2), col, tab_names[i]);

        float tw = 28 + ts.x + 32;
        if (active_tab == i) {
            dl->AddLine(ImVec2(cursor_x, tab_y + TAB_H - 2), ImVec2(cursor_x + tw - 16, tab_y + TAB_H - 2), C_ACCENT, 2.0f);
        }

        ImGui::SetCursorScreenPos(ImVec2(cursor_x, tab_y));
        ImGui::InvisibleButton((std::string("##tab_") + tab_names[i]).c_str(), ImVec2(tw, TAB_H));
        if (ImGui::IsItemClicked()) active_tab = i;

        cursor_x += tw;
    }

    const char* brand = "severance";
    ImVec2 br = ImGui::CalcTextSize(brand);
    dl->AddText(ImVec2(wp.x + ws.x - br.x - 22, tab_y + (TAB_H - br.y) / 2), C_TEXT_DIM, brand);

    ImGui::End();
}

}
}
