#pragma once
#include <Windows.h>
#include <string>

namespace FrameWork {

void ApplyDarkTheme();
void RenderGui();

struct Options {
    struct {
        struct {
            bool Enabled; int KeyBind; bool TargetNPC; bool VisibleCheck;
            int HitBox; int MaxDistance; bool Prediction;
            int Smoothness;  // 0-100%
            int FovRadius;   // 0-500px
        } AimBot;
        struct { bool Enabled; int KeyBind; bool ShotNPC; bool VisibleCheck; int MaxDistance; } Trigger;
        struct { bool Enabled; int KeyBind; bool ShotNPC; bool VisibleCheck; bool MagicBullet;
                 bool Prediction; bool AutoShoot; bool AliveOnly; bool ForceDriver; int MaxDistance; } SilentAim;
    } LegitBot;

    struct {
        struct {
            bool Enabled; bool Box; bool CornerBox; bool Skeleton; bool Head;
            bool Name; bool HealthBar; bool ArmorBar; bool WeaponName; bool Distance; bool SnapLines;
            bool ShowLocalPlayer; bool ShowNPCs; bool VisibleOnly; int RenderDistance;
            float BoxColor[4]; float SkeletonColor[4]; float NameColor[4];
            int BoxStyle;       // 0=outline,1=filled,2=corners
            bool DistanceColor; // fade color with distance
        } Players;
        struct { bool Enabled; bool IgnoreOccupiedVehicles; bool Name; bool Distance; bool Marker; float Color[4]; } Vehicles;
        struct { Players Players; Vehicles Vehicles; } ESP;
    } Visuals;

    struct {
        struct { bool God; bool Noclip; bool Invisible; bool Shrink; bool speed;
                 float Player_speed; int tpbind; bool TeleportToMarker; } LocalPlayer;
        struct { bool NoReload; bool NoRecoil; bool NoSpread; bool RapidFire; bool OneShotKill; bool InfiniteAmmo; } Weapon;
        struct { int VehicleModel; int WeaponModel; int MoneyDropAmount; bool DropMoney; } Spawner;
    } Exploits;

    struct {
        struct { bool EnableWatermark; bool EnableKeybindList; bool ShowAimbotFov; bool ShowSilentAimFov;
                 bool ShowTriggerFov; float AimbotFovColor[4]; float SilentFovColor[4]; float TriggerFovColor[4]; } Screen;
        struct { bool legit_mode; bool anti_screenshot; } Other;
        struct { bool AdminNotify; bool LowHealthWarning; int LowHealthThreshold; bool WantedLevelAlert; } Alerts;
    } Misc;

    struct {
        bool CaptureBypass; int MenuKey; int ThreadDelay; bool ShutDown; std::string SubscriptionDuration;
        float AccentColor[4]; // r,g,b,a (0.0-1.0)
        int   MenuOpacity;    // 50-100 %
    } General;

    bool Destruct;
};

extern Options g_Options;

} // namespace FrameWork
