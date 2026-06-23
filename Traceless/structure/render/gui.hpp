#pragma once

#include <Windows.h>
#include <string>

namespace FrameWork {

void ApplyDarkTheme();
bool SectionHeader(const char* id, const char* label);
void KeybindButton(const char* label, int* key);
void RenderGui();

struct Options {
    struct {
        struct { bool Enabled; int KeyBind; bool TargetNPC; bool VisibleCheck; int HitBox; int MaxDistance; bool Prediction; } AimBot;
        struct { bool Enabled; int KeyBind; bool ShotNPC; bool VisibleCheck; int MaxDistance; } Trigger;
        struct { bool Enabled; int KeyBind; bool ShotNPC; bool VisibleCheck; bool MagicBullet; bool Prediction; bool AutoShoot; bool AliveOnly; bool ForceDriver; int MaxDistance; } SilentAim;
    } LegitBot;

    struct {
        struct { bool Enabled; bool Box; bool CornerBox; bool Skeleton; bool Head; bool Name; bool HealthBar; bool ArmorBar; bool WeaponName; bool Distance; bool SnapLines; bool ShowLocalPlayer; bool ShowNPCs; bool VisibleOnly; int RenderDistance; float BoxColor[4]; float SkeletonColor[4]; float NameColor[4]; } Players;
        struct { bool Enabled; bool IgnoreOccupiedVehicles; bool Name; bool Distance; bool Marker; float Color[4]; } Vehicles;
        struct { Players Players; Vehicles Vehicles; } ESP;
    } Visuals;

    struct {
        struct { bool God; bool Noclip; bool Invisible; bool Shrink; bool speed; float Player_speed; int tpbind; } LocalPlayer;
        struct { bool NoReload; bool NoRecoil; bool NoSpread; bool RapidFire; bool OneShotKill; bool InfiniteAmmo; } Weapon;
    } Exploits;

    struct {
        struct { bool EnableWatermark; bool EnableKeybindList; bool ShowAimbotFov; bool ShowSilentAimFov; bool ShowTriggerFov; float AimbotFovColor[4]; float SilentFovColor[4]; float TriggerFovColor[4]; } Screen;
        struct { bool legit_mode; bool anti_screenshot; } Other;
    } Misc;

    struct { bool CaptureBypass; int MenuKey; int ThreadDelay; bool ShutDown; std::string SubscriptionDuration; } General;
    bool Destruct;
};

extern Options g_Options;

}
