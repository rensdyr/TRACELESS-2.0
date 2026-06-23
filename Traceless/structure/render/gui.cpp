#include "gui.hpp"
#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <imgui.h>
#include <map>
#include <string>

namespace FrameWork {
namespace render_ui {

static std::map<const char*, bool> section_open;
static int active_tab = 0;
static bool key_listening = false;
static int temp_key = 0;

static const ImVec4 COLOR_ACCENT = ImVec4(0.86f, 0.18f, 0.18f, 1.0f);
static const ImVec4 COLOR_BG_DARK = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
static const ImVec4 COLOR_BG_MID = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
static const ImVec4 COLOR_TEXT = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);

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
        default:
            if (key >= 'A' && key <= 'Z') {
                static char buf[2] = {0, 0};
                buf[0] = (char)key;
                return std::string(buf);
            }
            if (key >= VK_F1 && key <= VK_F12) {
                static char buf[3];
                sprintf_s(buf, "F%d", key - VK_F1 + 1);
                return std::string(buf);
            }
            return "Key:" + std::to_string(key);
    }
}

void ApplyDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;

    colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.09f, 0.95f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(0.2f, 0.2f, 0.2f, 0.5f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.86f, 0.18f, 0.18f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.86f, 0.18f, 0.18f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.86f, 0.18f, 0.18f, 0.3f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.86f, 0.18f, 0.18f, 0.6f);
    colors[ImGuiCol_Header] = ImVec4(0.86f, 0.18f, 0.18f, 0.2f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.86f, 0.18f, 0.18f, 0.4f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.86f, 0.18f, 0.18f, 0.6f);
    colors[ImGuiCol_Separator] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.86f, 0.18f, 0.18f, 0.4f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.86f, 0.18f, 0.18f, 0.6f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.86f, 0.18f, 0.18f, 0.2f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.86f, 0.18f, 0.18f, 0.4f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.86f, 0.18f, 0.18f, 0.6f);
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.86f, 0.18f, 0.18f, 0.3f);
    colors[ImGuiCol_TabActive] = ImVec4(0.86f, 0.18f, 0.18f, 0.6f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.0f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 0.43f, 0.35f, 1.0f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.9f, 0.7f, 0.0f, 1.0f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.6f, 0.0f, 1.0f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.86f, 0.18f, 0.18f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.0f, 1.0f, 0.0f, 0.9f);
    colors[ImGuiCol_NavHighlight] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
}

bool SectionHeader(const char* id, const char* label) {
    if (section_open.find(id) == section_open.end())
        section_open[id] = true;

    bool& open = section_open[id];
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float sw = ImGui::GetContentRegionAvail().x;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImU32 bg_color = ImGui::IsMouseHoveringRect(cp, ImVec2(cp.x + sw, cp.y + 28)) ?
        ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f)) :
        ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

    draw_list->AddRectFilled(cp, ImVec2(cp.x + sw, cp.y + 28), bg_color, 4.0f);
    draw_list->AddText(ImVec2(cp.x + 10, cp.y + 6), ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.0f)), label);

    const char* arrow = open ? "▼" : "▶";
    float arrow_width = ImGui::CalcTextSize(arrow).x;
    draw_list->AddText(ImVec2(cp.x + sw - arrow_width - 10, cp.y + 6), ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)), arrow);

    ImGui::SetCursorScreenPos(cp);
    ImGui::InvisibleButton("##section_btn", ImVec2(sw, 28));
    if (ImGui::IsItemClicked())
        open = !open;

    ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + 28));
    return open;
}

void KeybindButton(const char* label, int* key) {
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;

    ImGui::Text("%s", label);
    ImGui::SameLine();

    std::string key_str = KeyCodeToString(*key);
    float btn_width = 100.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width - btn_width - 20);

    if (ImGui::Button(key_str.c_str(), ImVec2(btn_width, 0))) {
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
}

void render_function() {
    ImGui::SetNextWindowSize(ImVec2(700, 550), ImGuiCond_FirstUseEver);
    ImGui::Begin("TRACELESS 2.0", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    float tab_height = 40.0f;
    float content_height = ImGui::GetContentRegionAvail().y - tab_height - 10;

    if (ImGui::BeginTabBar("##main_tabs", ImGuiTabBarFlags_None)) {

        if (ImGui::BeginTabItem("Aimbot")) {
            ImGui::BeginChild("aimbot_content", ImVec2(0, content_height), true);

            if (SectionHeader("aimbot_main", "Aimbot")) {
                ImGui::Checkbox("##aimbot_enabled", &g_Options.LegitBot.AimBot.Enabled);
                ImGui::SameLine();
                ImGui::Text("Enable Aimbot");
                KeybindButton("Keybind##aimbot", &g_Options.LegitBot.AimBot.KeyBind);
                ImGui::Checkbox("Target NPCs##aimbot", &g_Options.LegitBot.AimBot.TargetNPC);
                ImGui::Checkbox("Visible Check##aimbot", &g_Options.LegitBot.AimBot.VisibleCheck);
                ImGui::Checkbox("Show FOV##aimbot", &g_Options.Misc.Screen.ShowAimbotFov);
                ImGui::Checkbox("Prediction##aimbot", &g_Options.LegitBot.AimBot.Prediction);
                ImGui::Combo("Hitbox##aimbot", &g_Options.LegitBot.AimBot.HitBox,
                    "Head\0Neck\0Chest\0\0");
                ImGui::SliderInt("Max Distance##aimbot", &g_Options.LegitBot.AimBot.MaxDistance, 0, 1000);
            }
            ImGui::Spacing();

            if (SectionHeader("silent_main", "Silent Aim")) {
                ImGui::Checkbox("##silent_enabled", &g_Options.LegitBot.SilentAim.Enabled);
                ImGui::SameLine();
                ImGui::Text("Enable Silent Aim");
                KeybindButton("Keybind##silent", &g_Options.LegitBot.SilentAim.KeyBind);
                ImGui::Checkbox("Target NPCs##silent", &g_Options.LegitBot.SilentAim.ShotNPC);
                ImGui::Checkbox("Visible Check##silent", &g_Options.LegitBot.SilentAim.VisibleCheck);
                ImGui::Checkbox("Show FOV##silent", &g_Options.Misc.Screen.ShowSilentAimFov);
                ImGui::Checkbox("Magic Bullet##silent", &g_Options.LegitBot.SilentAim.MagicBullet);
                ImGui::Checkbox("Prediction##silent", &g_Options.LegitBot.SilentAim.Prediction);
                ImGui::Checkbox("Auto Shoot##silent", &g_Options.LegitBot.SilentAim.AutoShoot);
                ImGui::Checkbox("Alive Only##silent", &g_Options.LegitBot.SilentAim.AliveOnly);
                ImGui::Checkbox("Force Driver##silent", &g_Options.LegitBot.SilentAim.ForceDriver);
                ImGui::SliderInt("Max Distance##silent", &g_Options.LegitBot.SilentAim.MaxDistance, 0, 1000);
            }
            ImGui::Spacing();

            if (SectionHeader("trigger_main", "Trigger Bot")) {
                ImGui::Checkbox("##trigger_enabled", &g_Options.LegitBot.Trigger.Enabled);
                ImGui::SameLine();
                ImGui::Text("Enable Trigger Bot");
                KeybindButton("Keybind##trigger", &g_Options.LegitBot.Trigger.KeyBind);
                ImGui::Checkbox("Target NPCs##trigger", &g_Options.LegitBot.Trigger.ShotNPC);
                ImGui::Checkbox("Visible Check##trigger", &g_Options.LegitBot.Trigger.VisibleCheck);
                ImGui::SliderInt("Max Distance##trigger", &g_Options.LegitBot.Trigger.MaxDistance, 0, 1000);
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Visuals")) {
            ImGui::BeginChild("visuals_content", ImVec2(0, content_height), true);

            if (SectionHeader("esp_players", "Player ESP")) {
                ImGui::Checkbox("##players_enabled", &g_Options.Visuals.ESP.Players.Enabled);
                ImGui::SameLine();
                ImGui::Text("Enable Player ESP");
                ImGui::Checkbox("Box##players", &g_Options.Visuals.ESP.Players.Box);
                ImGui::Checkbox("Corner Boxes##players", &g_Options.Visuals.ESP.Players.CornerBox);
                ImGui::Checkbox("Skeleton##players", &g_Options.Visuals.ESP.Players.Skeleton);
                ImGui::Checkbox("Head Circle##players", &g_Options.Visuals.ESP.Players.Head);
                ImGui::Checkbox("Names##players", &g_Options.Visuals.ESP.Players.Name);
                ImGui::Checkbox("Health Bar##players", &g_Options.Visuals.ESP.Players.HealthBar);
                ImGui::Checkbox("Armor Bar##players", &g_Options.Visuals.ESP.Players.ArmorBar);
                ImGui::Checkbox("Weapon Names##players", &g_Options.Visuals.ESP.Players.WeaponName);
                ImGui::Checkbox("Distance##players", &g_Options.Visuals.ESP.Players.Distance);
                ImGui::Checkbox("Snaplines##players", &g_Options.Visuals.ESP.Players.SnapLines);
            }
            ImGui::Spacing();

            if (SectionHeader("esp_vehicles", "Vehicle ESP")) {
                ImGui::Checkbox("##vehicles_enabled", &g_Options.Visuals.ESP.Vehicles.Enabled);
                ImGui::SameLine();
                ImGui::Text("Enable Vehicle ESP");
                ImGui::Checkbox("Ignore Occupied##vehicles", &g_Options.Visuals.ESP.Vehicles.IgnoreOccupiedVehicles);
                ImGui::Checkbox("Show Names##vehicles", &g_Options.Visuals.ESP.Vehicles.Name);
                ImGui::Checkbox("Show Distance##vehicles", &g_Options.Visuals.ESP.Vehicles.Distance);
                ImGui::Checkbox("Show Marker##vehicles", &g_Options.Visuals.ESP.Vehicles.Marker);
            }
            ImGui::Spacing();

            if (SectionHeader("esp_settings", "ESP Settings")) {
                ImGui::Checkbox("Show Local Player##esp", &g_Options.Visuals.ESP.Players.ShowLocalPlayer);
                ImGui::Checkbox("Show NPCs##esp", &g_Options.Visuals.ESP.Players.ShowNPCs);
                ImGui::Checkbox("Visible Only##esp", &g_Options.Visuals.ESP.Players.VisibleOnly);
                ImGui::SliderInt("Render Distance##esp", &g_Options.Visuals.ESP.Players.RenderDistance, 0, 1000);
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Exploits")) {
            ImGui::BeginChild("exploits_content", ImVec2(0, content_height), true);

            if (SectionHeader("exploits_local", "Local Player")) {
                ImGui::Checkbox("God Mode##local", &g_Options.Misc.Exploits.LocalPlayer.God);
                ImGui::Checkbox("Noclip##local", &g_Options.Misc.Exploits.LocalPlayer.Noclip);
                ImGui::Checkbox("Invisible##local", &g_Options.Misc.Exploits.LocalPlayer.Invisible);
                ImGui::Checkbox("Shrink##local", &g_Options.Misc.Exploits.LocalPlayer.Shrink);
                ImGui::Checkbox("Speed Hack##local", &g_Options.Misc.Exploits.LocalPlayer.speed);
                ImGui::SliderFloat("Player Speed##local", &g_Options.Misc.Exploits.LocalPlayer.Player_speed, 1.0f, 10.0f);
                if (ImGui::Button("Teleport to Waypoint", ImVec2(-1, 0))) {
                    g_Options.Misc.Exploits.LocalPlayer.tpbind = 1;
                }
            }
            ImGui::Spacing();

            if (SectionHeader("exploits_weapon", "Weapon")) {
                ImGui::Checkbox("No Reload##weapon", &g_Options.Misc.Exploits.Weapon.NoReload);
                ImGui::Checkbox("No Recoil##weapon", &g_Options.Misc.Exploits.Weapon.NoRecoil);
                ImGui::Checkbox("No Spread##weapon", &g_Options.Misc.Exploits.Weapon.NoSpread);
                ImGui::Checkbox("Rapid Fire##weapon", &g_Options.Misc.Exploits.Weapon.RapidFire);
                ImGui::Checkbox("One Shot Kill##weapon", &g_Options.Misc.Exploits.Weapon.OneShotKill);
                ImGui::Checkbox("Infinite Ammo##weapon", &g_Options.Misc.Exploits.Weapon.InfiniteAmmo);
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Overlays")) {
            ImGui::BeginChild("overlays_content", ImVec2(0, content_height), true);

            if (SectionHeader("overlays_screen", "Screen")) {
                ImGui::Checkbox("Watermark##overlays", &g_Options.Misc.Screen.EnableWatermark);
                ImGui::Checkbox("Keybind List##overlays", &g_Options.Misc.Screen.EnableKeybindList);
                ImGui::Checkbox("Aimbot FOV Circle##overlays", &g_Options.Misc.Screen.ShowAimbotFov);
                ImGui::Checkbox("Silent FOV Circle##overlays", &g_Options.Misc.Screen.ShowSilentAimFov);
                ImGui::Checkbox("Trigger FOV Circle##overlays", &g_Options.Misc.Screen.ShowTriggerFov);
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Settings")) {
            ImGui::BeginChild("settings_content", ImVec2(0, content_height), true);

            if (SectionHeader("settings_general", "General")) {
                ImGui::Checkbox("Capture Bypass##settings", &g_Options.General.CaptureBypass);
                ImGui::Checkbox("Legit Mode##settings", &g_Options.Misc.Other.legit_mode);
                ImGui::Checkbox("Anti Screenshot##settings", &g_Options.Misc.Other.anti_screenshot);
                ImGui::SliderInt("Thread Delay##settings", &g_Options.General.ThreadDelay, 0, 32);
            }
            ImGui::Spacing();

            if (SectionHeader("settings_danger", "Danger Zone")) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.86f, 0.18f, 0.18f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.1f, 0.1f, 1.0f));

                if (ImGui::Button("DESTRUCT (Wipes Disk)", ImVec2(-1, 30))) {
                    g_Options.Misc.Destruct = true;
                }

                if (ImGui::Button("UNLOAD CHEAT", ImVec2(-1, 30))) {
                    g_Options.General.ShutDown = true;
                }

                ImGui::PopStyleColor(3);
            }

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void RenderGui() {
    ApplyDarkTheme();
    render_function();
}

}
}
