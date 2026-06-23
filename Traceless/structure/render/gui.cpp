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
static int temp_key = 0;

static const ImVec4 COLOR_ACCENT = ImVec4(0.86f, 0.18f, 0.18f, 1.0f);
static const ImVec4 COLOR_BG_DARK = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
static const ImVec4 COLOR_BG_MID = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
static const ImVec4 COLOR_TEXT = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
static const ImVec4 COLOR_TEXT_BRIGHT = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);

static std::string KeyCodeToString(int key) {
    if (key == 0) return "None";
    switch (key) {
        case VK_LBUTTON: return "LMB";
        case VK_RBUTTON: return "RMB";
        case VK_MBUTTON: return "MMB";
        case VK_XBUTTON1: return "M4";
        case VK_XBUTTON2: return "M5";
        case VK_SHIFT: return "Shift";
        case VK_CONTROL: return "Ctrl";
        case VK_MENU: return "Alt";
        case VK_SPACE: return "Space";
        case VK_RETURN: return "Enter";
        case VK_ESCAPE: return "Esc";
        case VK_TAB: return "Tab";
        default:
            if (key >= 'A' && key <= 'Z') {
                static char buf[2] = {0, 0};
                buf[0] = (char)key;
                return std::string(buf);
            }
            if (key >= VK_F1 && key <= VK_F12) {
                static char buf[4];
                sprintf_s(buf, sizeof(buf), "F%d", key - VK_F1 + 1);
                return std::string(buf);
            }
            if (key >= '0' && key <= '9') {
                static char buf[2] = {0, 0};
                buf[0] = (char)key;
                return std::string(buf);
            }
            return "Key";
    }
}

void ApplyDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(4, 3);

    colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.09f, 0.95f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(0.2f, 0.2f, 0.2f, 0.5f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
}

struct Col {
    ImDrawList* draw;
    ImVec2 base;
    float w;
    float y;

    Col(ImDrawList* d, ImVec2 b, float width) : draw(d), base(b), w(width), y(0) {}

    void Spacing(float h = 8.0f) {
        y += h;
    }

    bool Chk(const char* label, bool* v) {
        ImVec2 pos = ImVec2(base.x, base.y + y);
        float sz = 16.0f;

        ImU32 bg_col = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImU32 border_col = ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImU32 check_col = ImGui::GetColorU32(COLOR_ACCENT);

        draw->AddRect(pos, ImVec2(pos.x + sz, pos.y + sz), border_col, 2.0f, 0, 1.0f);
        draw->AddRectFilled(pos, ImVec2(pos.x + sz, pos.y + sz), bg_col, 2.0f);

        if (*v) {
            draw->AddLine(ImVec2(pos.x + 3, pos.y + 8), ImVec2(pos.x + 6, pos.y + 11), check_col, 2.0f);
            draw->AddLine(ImVec2(pos.x + 6, pos.y + 11), ImVec2(pos.x + 13, pos.y + 4), check_col, 2.0f);
        }

        draw->AddText(ImVec2(pos.x + sz + 8, pos.y + 2), ImGui::GetColorU32(COLOR_TEXT_BRIGHT), label);

        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(("##chk_" + std::string(label)).c_str(), ImVec2(w, sz + 4));
        if (ImGui::IsItemClicked()) *v = !*v;

        y += 24.0f;
        return true;
    }

    bool Tog(const char* label, bool* v) {
        ImVec2 pos = ImVec2(base.x + w - 50, base.y + y + 2);
        float tog_w = 48.0f, tog_h = 20.0f;

        ImU32 track_col = *v ? ImGui::GetColorU32(COLOR_ACCENT) : ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImU32 knob_col = ImGui::GetColorU32(ImVec4(0.95f, 0.95f, 0.95f, 1.0f));

        draw->AddRectFilled(pos, ImVec2(pos.x + tog_w, pos.y + tog_h), track_col, tog_h / 2);

        float knob_x = *v ? pos.x + tog_w - tog_h - 2 : pos.x + 2;
        draw->AddCircleFilled(ImVec2(knob_x + tog_h / 2 - 1, pos.y + tog_h / 2), tog_h / 2 - 2, knob_col);

        draw->AddText(ImVec2(base.x, base.y + y + 2), ImGui::GetColorU32(COLOR_TEXT_BRIGHT), label);

        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(("##tog_" + std::string(label)).c_str(), ImVec2(tog_w, tog_h));
        if (ImGui::IsItemClicked()) *v = !*v;

        y += 24.0f;
        return true;
    }

    bool KB(const char* label, int* key) {
        ImVec2 label_pos = ImVec2(base.x, base.y + y + 4);
        ImVec2 btn_pos = ImVec2(base.x + w - 72, base.y + y);
        float btn_w = 70.0f, btn_h = 22.0f;

        draw->AddText(label_pos, ImGui::GetColorU32(COLOR_TEXT_BRIGHT), label);

        ImU32 btn_bg = ImGui::GetColorU32(COLOR_ACCENT);
        ImU32 btn_border = ImGui::GetColorU32(ImVec4(0.95f, 0.3f, 0.3f, 1.0f));

        draw->AddRectFilled(btn_pos, ImVec2(btn_pos.x + btn_w, btn_pos.y + btn_h), btn_bg, 2.0f);
        draw->AddRect(btn_pos, ImVec2(btn_pos.x + btn_w, btn_pos.y + btn_h), btn_border, 2.0f, 0, 1.0f);

        std::string key_str = KeyCodeToString(*key);
        ImVec2 text_size = ImGui::CalcTextSize(key_str.c_str());
        ImVec2 text_pos = ImVec2(btn_pos.x + (btn_w - text_size.x) / 2, btn_pos.y + (btn_h - text_size.y) / 2 - 1);
        draw->AddText(text_pos, ImGui::GetColorU32(ImVec4(0.95f, 0.95f, 0.95f, 1.0f)), key_str.c_str());

        ImGui::SetCursorScreenPos(btn_pos);
        ImGui::InvisibleButton(("##kb_" + std::string(label)).c_str(), ImVec2(btn_w, btn_h));
        if (ImGui::IsItemClicked()) {
            key_listening = true;
            temp_key = *key;
        }

        if (key_listening) {
            for (int i = 1; i < 256; i++) {
                if (GetAsyncKeyState(i) & 0x8000) {
                    *key = i;
                    key_listening = false;
                    break;
                }
            }
        }

        y += 26.0f;
        return true;
    }

    bool Sld(const char* label, int* v, int vmin, int vmax) {
        ImVec2 label_pos = ImVec2(base.x, base.y + y);
        ImVec2 slider_pos = ImVec2(base.x, base.y + y + 18);
        float slider_w = w, slider_h = 12.0f;

        std::string val_str = std::to_string(*v);
        ImVec2 val_size = ImGui::CalcTextSize(val_str.c_str());
        draw->AddText(ImVec2(label_pos.x + w - val_size.x, label_pos.y), ImGui::GetColorU32(COLOR_TEXT), val_str.c_str());
        draw->AddText(label_pos, ImGui::GetColorU32(COLOR_TEXT_BRIGHT), label);

        ImU32 track_bg = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImU32 track_fill = ImGui::GetColorU32(COLOR_ACCENT);
        ImU32 knob_col = ImGui::GetColorU32(ImVec4(0.95f, 0.95f, 0.95f, 1.0f));

        draw->AddRectFilled(slider_pos, ImVec2(slider_pos.x + slider_w, slider_pos.y + slider_h), track_bg, 2.0f);

        float ratio = (float)(*v - vmin) / (vmax - vmin);
        float fill_w = slider_w * ratio;
        draw->AddRectFilled(slider_pos, ImVec2(slider_pos.x + fill_w, slider_pos.y + slider_h), track_fill, 2.0f);

        float knob_x = slider_pos.x + fill_w - 1;
        draw->AddCircleFilled(ImVec2(knob_x, slider_pos.y + slider_h / 2), 5.0f, knob_col);

        ImGui::SetCursorScreenPos(slider_pos);
        ImGui::InvisibleButton(("##sld_" + std::string(label)).c_str(), ImVec2(slider_w, slider_h));
        if (ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
            float mouse_x = ImGui::GetIO().MousePos.x;
            float new_ratio = (mouse_x - slider_pos.x) / slider_w;
            new_ratio = ImClamp(new_ratio, 0.0f, 1.0f);
            *v = (int)(vmin + new_ratio * (vmax - vmin));
        }

        y += 40.0f;
        return true;
    }

    bool Cmb(const char* label, int* v, const std::vector<std::string>& items) {
        ImVec2 label_pos = ImVec2(base.x, base.y + y);
        ImVec2 box_pos = ImVec2(base.x, base.y + y + 18);
        float box_w = w, box_h = 20.0f;

        draw->AddText(label_pos, ImGui::GetColorU32(COLOR_TEXT_BRIGHT), label);

        ImU32 box_bg = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImU32 box_border = ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f));

        draw->AddRectFilled(box_pos, ImVec2(box_pos.x + box_w, box_pos.y + box_h), box_bg, 2.0f);
        draw->AddRect(box_pos, ImVec2(box_pos.x + box_w, box_pos.y + box_h), box_border, 2.0f, 0, 1.0f);

        if (*v >= 0 && *v < (int)items.size()) {
            draw->AddText(ImVec2(box_pos.x + 6, box_pos.y + 3), ImGui::GetColorU32(COLOR_TEXT), items[*v].c_str());
        }

        draw->AddText(ImVec2(box_pos.x + box_w - 12, box_pos.y + 2), ImGui::GetColorU32(COLOR_TEXT), "v");

        ImGui::SetCursorScreenPos(box_pos);
        ImGui::InvisibleButton(("##cmb_" + std::string(label)).c_str(), ImVec2(box_w, box_h));
        if (ImGui::IsItemClicked()) {
            *v = (*v + 1) % items.size();
        }

        y += 42.0f;
        return true;
    }
};

bool SectionHeader(const char* id, const char* label, ImDrawList* draw, ImVec2 pos, float width, float& y) {
    if (section_open.find(id) == section_open.end())
        section_open[id] = true;

    bool& open = section_open[id];

    ImVec2 header_pos = ImVec2(pos.x, pos.y + y);
    float header_h = 28.0f;

    ImU32 bg_color = ImGui::GetColorU32(ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    draw->AddRectFilled(header_pos, ImVec2(header_pos.x + width, header_pos.y + header_h), bg_color);

    draw->AddText(ImVec2(header_pos.x + 10, header_pos.y + 6), ImGui::GetColorU32(COLOR_TEXT_BRIGHT), label);

    const char* arrow = open ? "▼" : "▶";
    ImVec2 arrow_size = ImGui::CalcTextSize(arrow);
    draw->AddText(ImVec2(header_pos.x + width - arrow_size.x - 10, header_pos.y + 6), ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)), arrow);

    ImGui::SetCursorScreenPos(header_pos);
    ImGui::InvisibleButton(("##sec_" + std::string(id)).c_str(), ImVec2(width, header_h));
    if (ImGui::IsItemClicked())
        open = !open;

    y += header_h;
    return open;
}

void RenderGui() {
    ApplyDarkTheme();

    ImGui::SetNextWindowSize(ImVec2(820, 670), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("TRACELESS", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();

    float HDR_H = 40.0f;
    float TAB_H = 48.0f;
    float content_h = win_size.y - HDR_H - TAB_H - 2;

    ImU32 header_bg = ImGui::GetColorU32(ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
    draw_list->AddRectFilled(ImVec2(win_pos.x, win_pos.y), ImVec2(win_pos.x + win_size.x, win_pos.y + HDR_H), header_bg);

    draw_list->AddText(ImVec2(win_pos.x + 14, win_pos.y + 12), ImGui::GetColorU32(COLOR_ACCENT), "severance");
    draw_list->AddText(ImVec2(win_pos.x + 14 + ImGui::CalcTextSize("severance").x, win_pos.y + 12), ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)), ".today");

    ImU32 tab_bar_bg = ImGui::GetColorU32(ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
    draw_list->AddRectFilled(ImVec2(win_pos.x, win_pos.y + win_size.y - TAB_H), ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y), tab_bar_bg);

    const char* tabs[] = {"aimbot", "visuals", "overlays", "exploits", "lists", "settings"};
    float tab_w = win_size.x / 6;

    for (int i = 0; i < 6; i++) {
        ImVec2 tab_pos = ImVec2(win_pos.x + i * tab_w, win_pos.y + win_size.y - TAB_H);

        if (active_tab == i) {
            ImU32 active_col = ImGui::GetColorU32(COLOR_ACCENT);
            draw_list->AddLine(ImVec2(tab_pos.x, tab_pos.y), ImVec2(tab_pos.x + tab_w, tab_pos.y), active_col, 2.0f);
            draw_list->AddText(ImVec2(tab_pos.x + tab_w / 2 - ImGui::CalcTextSize(tabs[i]).x / 2, tab_pos.y + 16), ImGui::GetColorU32(COLOR_TEXT_BRIGHT), tabs[i]);
        } else {
            draw_list->AddText(ImVec2(tab_pos.x + tab_w / 2 - ImGui::CalcTextSize(tabs[i]).x / 2, tab_pos.y + 16), ImGui::GetColorU32(COLOR_TEXT), tabs[i]);
        }

        ImGui::SetCursorScreenPos(tab_pos);
        ImGui::InvisibleButton(("##tab_" + std::string(tabs[i])).c_str(), ImVec2(tab_w, TAB_H));
        if (ImGui::IsItemClicked())
            active_tab = i;
    }

    ImVec2 content_pos = ImVec2(win_pos.x, win_pos.y + HDR_H);

    ImGui::SetCursorScreenPos(content_pos);
    ImGui::BeginChild("content", ImVec2(win_size.x, content_h), false, ImGuiWindowFlags_NoScrollbar);

    if (active_tab == 0) {
        ImDrawList* content_draw = ImGui::GetWindowDrawList();
        ImVec2 child_pos = ImGui::GetWindowPos();
        float child_w = ImGui::GetWindowSize().x;
        float col_w = (child_w - 4) / 2;
        float y_offset = 0;

        Col left(content_draw, child_pos, col_w);
        Col right(content_draw, ImVec2(child_pos.x + col_w + 4, child_pos.y), col_w);

        if (SectionHeader("aimbot_main", "Aimbot", content_draw, child_pos, col_w, y_offset)) {
            left.y = y_offset;
            left.Chk("Enable", &g_Options.LegitBot.AimBot.Enabled);
            left.KB("Keybind", &g_Options.LegitBot.AimBot.KeyBind);
            left.Chk("Target NPCs", &g_Options.LegitBot.AimBot.TargetNPC);
            left.Chk("Visible Check", &g_Options.LegitBot.AimBot.VisibleCheck);
            left.Chk("Prediction", &g_Options.LegitBot.AimBot.Prediction);
            std::vector<std::string> hitboxes = {"Head", "Neck", "Chest"};
            left.Cmb("Hitbox", &g_Options.LegitBot.AimBot.HitBox, hitboxes);
            left.Sld("Max Distance", &g_Options.LegitBot.AimBot.MaxDistance, 0, 1000);
        }
        y_offset = left.y + 8;

        if (SectionHeader("trigger_main", "Trigger Bot", content_draw, child_pos, col_w, y_offset)) {
            left.y = y_offset;
            left.Chk("Enable", &g_Options.LegitBot.Trigger.Enabled);
            left.KB("Keybind", &g_Options.LegitBot.Trigger.KeyBind);
            left.Chk("Target NPCs", &g_Options.LegitBot.Trigger.ShotNPC);
            left.Chk("Visible Check", &g_Options.LegitBot.Trigger.VisibleCheck);
            left.Sld("Max Distance", &g_Options.LegitBot.Trigger.MaxDistance, 0, 1000);
        }
        y_offset = left.y + 8;

        float right_y = 0;
        if (SectionHeader("silent_main", "Silent Aim", content_draw, ImVec2(child_pos.x + col_w + 4, child_pos.y), col_w, right_y)) {
            right.y = right_y;
            right.Chk("Enable", &g_Options.LegitBot.SilentAim.Enabled);
            right.KB("Keybind", &g_Options.LegitBot.SilentAim.KeyBind);
            right.Chk("Target NPCs", &g_Options.LegitBot.SilentAim.ShotNPC);
            right.Chk("Visible Check", &g_Options.LegitBot.SilentAim.VisibleCheck);
            right.Chk("Magic Bullet", &g_Options.LegitBot.SilentAim.MagicBullet);
            right.Chk("Prediction", &g_Options.LegitBot.SilentAim.Prediction);
            right.Chk("Auto Shoot", &g_Options.LegitBot.SilentAim.AutoShoot);
            right.Chk("Alive Only", &g_Options.LegitBot.SilentAim.AliveOnly);
            right.Chk("Force Driver", &g_Options.LegitBot.SilentAim.ForceDriver);
            right.Sld("Max Distance", &g_Options.LegitBot.SilentAim.MaxDistance, 0, 1000);
        }
    }
    else if (active_tab == 1) {
        ImDrawList* content_draw = ImGui::GetWindowDrawList();
        ImVec2 child_pos = ImGui::GetWindowPos();
        float child_w = ImGui::GetWindowSize().x;
        float y_offset = 0;

        Col col(content_draw, child_pos, child_w);

        if (SectionHeader("esp_players", "Player ESP", content_draw, child_pos, child_w, y_offset)) {
            col.y = y_offset;
            col.Chk("Enable", &g_Options.Visuals.ESP.Players.Enabled);
            col.Chk("Box", &g_Options.Visuals.ESP.Players.Box);
            col.Chk("Corner Boxes", &g_Options.Visuals.ESP.Players.CornerBox);
            col.Chk("Skeleton", &g_Options.Visuals.ESP.Players.Skeleton);
            col.Chk("Head Circle", &g_Options.Visuals.ESP.Players.Head);
            col.Chk("Names", &g_Options.Visuals.ESP.Players.Name);
            col.Chk("Health Bar", &g_Options.Visuals.ESP.Players.HealthBar);
            col.Chk("Armor Bar", &g_Options.Visuals.ESP.Players.ArmorBar);
            col.Chk("Weapon Names", &g_Options.Visuals.ESP.Players.WeaponName);
            col.Chk("Distance", &g_Options.Visuals.ESP.Players.Distance);
            col.Chk("Snaplines", &g_Options.Visuals.ESP.Players.SnapLines);
        }
        y_offset = col.y + 8;

        if (SectionHeader("esp_vehicles", "Vehicle ESP", content_draw, child_pos, child_w, y_offset)) {
            col.y = y_offset;
            col.Chk("Enable", &g_Options.Visuals.ESP.Vehicles.Enabled);
            col.Chk("Ignore Occupied", &g_Options.Visuals.ESP.Vehicles.IgnoreOccupiedVehicles);
            col.Chk("Names", &g_Options.Visuals.ESP.Vehicles.Name);
            col.Chk("Distance", &g_Options.Visuals.ESP.Vehicles.Distance);
            col.Chk("Marker", &g_Options.Visuals.ESP.Vehicles.Marker);
        }
        y_offset = col.y + 8;

        if (SectionHeader("esp_settings", "ESP Settings", content_draw, child_pos, child_w, y_offset)) {
            col.y = y_offset;
            col.Chk("Show Local Player", &g_Options.Visuals.ESP.Players.ShowLocalPlayer);
            col.Chk("Show NPCs", &g_Options.Visuals.ESP.Players.ShowNPCs);
            col.Chk("Visible Only", &g_Options.Visuals.ESP.Players.VisibleOnly);
            col.Sld("Render Distance", &g_Options.Visuals.ESP.Players.RenderDistance, 0, 1000);
        }
    }
    else if (active_tab == 2) {
        ImDrawList* content_draw = ImGui::GetWindowDrawList();
        ImVec2 child_pos = ImGui::GetWindowPos();
        float child_w = ImGui::GetWindowSize().x;
        float y_offset = 0;

        Col col(content_draw, child_pos, child_w);

        if (SectionHeader("overlays_screen", "Screen", content_draw, child_pos, child_w, y_offset)) {
            col.y = y_offset;
            col.Chk("Watermark", &g_Options.Misc.Screen.EnableWatermark);
            col.Chk("Keybind List", &g_Options.Misc.Screen.EnableKeybindList);
            col.Chk("Aimbot FOV", &g_Options.Misc.Screen.ShowAimbotFov);
            col.Chk("Silent FOV", &g_Options.Misc.Screen.ShowSilentAimFov);
            col.Chk("Trigger FOV", &g_Options.Misc.Screen.ShowTriggerFov);
        }
    }
    else if (active_tab == 3) {
        ImDrawList* content_draw = ImGui::GetWindowDrawList();
        ImVec2 child_pos = ImGui::GetWindowPos();
        float child_w = ImGui::GetWindowSize().x;
        float y_offset = 0;

        Col col(content_draw, child_pos, child_w);

        if (SectionHeader("exploits_local", "Local Player", content_draw, child_pos, child_w, y_offset)) {
            col.y = y_offset;
            col.Chk("God Mode", &g_Options.Exploits.LocalPlayer.God);
            col.Chk("Noclip", &g_Options.Exploits.LocalPlayer.Noclip);
            col.Chk("Invisible", &g_Options.Exploits.LocalPlayer.Invisible);
            col.Chk("Shrink", &g_Options.Exploits.LocalPlayer.Shrink);
            col.Chk("Speed Hack", &g_Options.Exploits.LocalPlayer.speed);
            col.Sld("Player Speed", &g_Options.Exploits.LocalPlayer.Player_speed, 1, 10);
        }
        y_offset = col.y + 8;

        if (SectionHeader("exploits_weapon", "Weapon", content_draw, child_pos, child_w, y_offset)) {
            col.y = y_offset;
            col.Chk("No Reload", &g_Options.Exploits.Weapon.NoReload);
            col.Chk("No Recoil", &g_Options.Exploits.Weapon.NoRecoil);
            col.Chk("No Spread", &g_Options.Exploits.Weapon.NoSpread);
            col.Chk("Rapid Fire", &g_Options.Exploits.Weapon.RapidFire);
            col.Chk("One Shot Kill", &g_Options.Exploits.Weapon.OneShotKill);
            col.Chk("Infinite Ammo", &g_Options.Exploits.Weapon.InfiniteAmmo);
        }
    }
    else if (active_tab == 4) {
        ImDrawList* content_draw = ImGui::GetWindowDrawList();
        ImVec2 child_pos = ImGui::GetWindowPos();
        draw_list->AddText(ImVec2(child_pos.x + 10, child_pos.y + 10), ImGui::GetColorU32(COLOR_TEXT), "Lists coming soon...");
    }
    else if (active_tab == 5) {
        ImDrawList* content_draw = ImGui::GetWindowDrawList();
        ImVec2 child_pos = ImGui::GetWindowPos();
        float child_w = ImGui::GetWindowSize().x;
        float y_offset = 0;

        Col col(content_draw, child_pos, child_w);

        if (SectionHeader("settings_general", "General", content_draw, child_pos, child_w, y_offset)) {
            col.y = y_offset;
            col.Chk("Capture Bypass", &g_Options.General.CaptureBypass);
            col.Chk("Legit Mode", &g_Options.Misc.Other.legit_mode);
            col.Chk("Anti Screenshot", &g_Options.Misc.Other.anti_screenshot);
            col.Sld("Thread Delay", &g_Options.General.ThreadDelay, 0, 32);
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

}
}
