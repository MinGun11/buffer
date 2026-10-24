#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <iostream>
#include <string>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

namespace BufferCore {
    // Stages: 0 = OFF, 1 = 4 Ticks, 2 = 8 Ticks, 3 = 16 Ticks, 4 = 32 Ticks
    int stageGround = 2; // Default: 4 Ticks
    int stageWave   = 2; // Default: 4 Ticks
    int stageShip   = 0; // Default: OFF
    bool hazardSaver = true;

    int pendingPressP1 = 0;
    int pendingPressP2 = 0;
    bool isHoldingP1 = false;
    bool isHoldingP2 = false;

    bool internalLock = false;

    int stageToTicks(int stage) {
        switch (stage) {
            case 1: return 4;
            case 2: return 8;
            case 3: return 16;
            case 4: return 32;
            default: return 0;
        }
    }

    void writeLiveConsole(const std::string& msg) {
#ifdef GEODE_IS_WINDOWS
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut && hOut != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteConsoleA(hOut, msg.c_str(), static_cast<DWORD>(msg.length()), &written, NULL);
        }
#endif
    }

    void printBanner() {
        std::string banner = 
            "\n================================================================================\n"
            "          PREDICTIVE INPUT BUFFER - LIVE CONSOLE (HARMFULFOID REPLICA)          \n"
            "================================================================================\n"
            " [HOTKEYS]\n"
            "   [F1] Ground / Orb / Robot / Spider : Cycle Ticks (OFF -> 4t -> 8t -> 16t -> 32t)\n"
            "   [F2] Wave Corridor Buffer          : Cycle Ticks (OFF -> 4t -> 8t -> 16t -> 32t)\n"
            "   [F3] Ship / UFO / Swing Dynamics   : Cycle Ticks (OFF -> 4t -> 8t -> 16t -> 32t)\n"
            "   [F4] Hazard Spike Interceptor      : Toggle ON / OFF\n"
            "   [F7] VORTEX MODE (ALL MAX 16t)     : Instant Engage\n"
            "================================================================================\n";
        writeLiveConsole(banner);
    }

    void logStageChange(const char* name, int stage, int ticks) {
        char buf[256];
        if (stage == 0) {
            snprintf(buf, sizeof(buf), "[BUFFER] [%s] -> STAGE 0: OFF [RAW Input]\n", name);
        } else {
            snprintf(buf, sizeof(buf), "[BUFFER] [%s] -> STAGE %d: %d Ticks (%.2f Frames @ 240fps)\n", 
                name, stage, ticks, ticks * (240.0f / 60.0f));
        }
        writeLiveConsole(buf);
    }

    void logHazardSave(const char* mode, int ticks, const char* playerStr) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[BUFFER] *** SAVED AT %d TICKS HAZARD (%s) [%s] (Deflected) ***\n", 
            ticks, playerStr, mode);
        writeLiveConsole(buf);
    }

    bool isWave(PlayerObject* p) {
        return p && p->m_isDart;
    }

    bool isContinuous(PlayerObject* p) {
        return p && (p->m_isShip || p->m_isDart || p->m_isSwing);
    }

    int getTicksForPlayer(PlayerObject* p) {
        if (!p) return 0;
        if (p->m_isDart) return stageToTicks(stageWave);
        if (p->m_isShip || p->m_isBird || p->m_isSwing) return stageToTicks(stageShip);
        return stageToTicks(stageGround);
    }
}

$on_mod(Loaded) {
#ifdef GEODE_IS_WINDOWS
    if (Mod::get()->getSettingValue<bool>("open-console")) {
        if (AllocConsole()) {
            SetConsoleTitleA("Geode - Predictive Input Buffer - Live Console");
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                SetConsoleMode(hOut, dwMode | 0x0004); // ENABLE_VIRTUAL_TERMINAL_PROCESSING
            }
        }
    }
#endif
    BufferCore::printBanner();
}

// Global hotkey listener for F1, F2, F3, F4, F7
class $modify(BufferGameHook, GJBaseGameLayer) {
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

#ifdef GEODE_IS_WINDOWS
        static bool f1Down = false, f2Down = false, f3Down = false, f4Down = false, f7Down = false;

        if (GetAsyncKeyState(VK_F1) & 0x8000) {
            if (!f1Down) {
                f1Down = true;
                BufferCore::stageGround = (BufferCore::stageGround + 1) % 5;
                BufferCore::logStageChange("F1: GROUND / ORB / PAD / SPIDER", BufferCore::stageGround, BufferCore::stageToTicks(BufferCore::stageGround));
            }
        } else { f1Down = false; }

        if (GetAsyncKeyState(VK_F2) & 0x8000) {
            if (!f2Down) {
                f2Down = true;
                BufferCore::stageWave = (BufferCore::stageWave + 1) % 5;
                BufferCore::logStageChange("F2: WAVE", BufferCore::stageWave, BufferCore::stageToTicks(BufferCore::stageWave));
            }
        } else { f2Down = false; }

        if (GetAsyncKeyState(VK_F3) & 0x8000) {
            if (!f3Down) {
                f3Down = true;
                BufferCore::stageShip = (BufferCore::stageShip + 1) % 5;
                BufferCore::logStageChange("F3: SHIP / UFO / SWING", BufferCore::stageShip, BufferCore::stageToTicks(BufferCore::stageShip));
            }
        } else { f3Down = false; }

        if (GetAsyncKeyState(VK_F4) & 0x8000) {
            if (!f4Down) {
                f4Down = true;
                BufferCore::hazardSaver = !BufferCore::hazardSaver;
                char buf[128];
                snprintf(buf, sizeof(buf), "[BUFFER] [F4: HAZARD SPIKE SAVER] -> %s\n", BufferCore::hazardSaver ? "ENABLED" : "DISABLED");
                BufferCore::writeLiveConsole(buf);
            }
        } else { f4Down = false; }

        if (GetAsyncKeyState(VK_F7) & 0x8000) {
            if (!f7Down) {
                f7Down = true;
                BufferCore::stageGround = 3; // 16 Ticks
                BufferCore::stageWave   = 3; // 16 Ticks
                BufferCore::stageShip   = 3; // 16 Ticks
                BufferCore::hazardSaver = true;
                BufferCore::writeLiveConsole("[BUFFER] [F7: VORTEX (ALL MODES) -> ENGAGED (16 Ticks)]\n");
            }
        } else { f7Down = false; }
#endif

        auto pl = PlayLayer::get();
        if (!pl || this != static_cast<GJBaseGameLayer*>(pl)) return;

        auto stepBuffer = [&](PlayerObject* player, int& pendingPress, bool isHolding) {
            if (!player) return;
            if (pendingPress > 0) {
                pendingPress--;
                BufferCore::internalLock = true;

                if (player->m_isOnGround || player->m_isBird || player->m_isSwing) {
                    player->pushButton(PlayerButton::Jump);
                    if (player->m_isOnGround && !BufferCore::isContinuous(player)) {
                        pendingPress = 0;
                    }
                } else {
                    // Auto-trigger jump rings / pads during active buffer ticks (Orb Spam fix)
                    player->ringJump(nullptr, false);
                }

                BufferCore::internalLock = false;
            }
        };

        stepBuffer(pl->m_player1, BufferCore::pendingPressP1, BufferCore::isHoldingP1);
        if (pl->m_gameState.m_isDualMode) {
            stepBuffer(pl->m_player2, BufferCore::pendingPressP2, BufferCore::isHoldingP2);
        }
    }
};

// Track player clicks and assign active lookahead ticks
class $modify(BufferPlayerHook, PlayerObject) {
    bool pushButton(PlayerButton btn) {
        if (BufferCore::internalLock || btn != PlayerButton::Jump) {
            return PlayerObject::pushButton(btn);
        }

        auto pl = PlayLayer::get();
        if (pl) {
            int ticks = BufferCore::getTicksForPlayer(this);
            if (this == pl->m_player1) {
                BufferCore::isHoldingP1 = true;
                if (ticks > 0) BufferCore::pendingPressP1 = ticks;
            } else if (this == pl->m_player2) {
                BufferCore::isHoldingP2 = true;
                if (ticks > 0) BufferCore::pendingPressP2 = ticks;
            }
        }

        return PlayerObject::pushButton(btn);
    }

    bool releaseButton(PlayerButton btn) {
        if (BufferCore::internalLock || btn != PlayerButton::Jump) {
            return PlayerObject::releaseButton(btn);
        }

        auto pl = PlayLayer::get();
        if (pl) {
            if (this == pl->m_player1) BufferCore::isHoldingP1 = false;
            else if (this == pl->m_player2) BufferCore::isHoldingP2 = false;
        }

        return PlayerObject::releaseButton(btn);
    }
};

// Intercept lethal hazards (Spikes, Wave Corridors, 100 Frame-Perfects)
class $modify(BufferPlayLayerHook, PlayLayer) {
    void resetLevel() {
        BufferCore::pendingPressP1 = 0;
        BufferCore::pendingPressP2 = 0;
        BufferCore::isHoldingP1 = false;
        BufferCore::isHoldingP2 = false;
        PlayLayer::resetLevel();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        // Let RobTop's anticheat test spike proceed uninterrupted
        if (!player || object == m_anticheatSpike) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        if (!BufferCore::hazardSaver) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        int activeTicks = BufferCore::getTicksForPlayer(player);
        if (activeTicks <= 0) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        bool isP1 = (player == m_player1);
        bool isHolding = isP1 ? BufferCore::isHoldingP1 : BufferCore::isHoldingP2;
        int& pendingPress = isP1 ? BufferCore::pendingPressP1 : BufferCore::pendingPressP2;

        BufferCore::internalLock = true;
        player->m_isDead = false;
        double gravDir = player->m_isUpsideDown ? -1.0 : 1.0;

        if (BufferCore::isWave(player)) {
            // Wave corridor deflection: Invert hold state and nudge away from slope/spike
            if (isHolding) {
                player->releaseButton(PlayerButton::Jump);
                player->setPositionY(player->getPositionY() - 3.5f * static_cast<float>(gravDir));
            } else {
                player->pushButton(PlayerButton::Jump);
                player->setPositionY(player->getPositionY() + 3.5f * static_cast<float>(gravDir));
            }
            BufferCore::logHazardSave("WAVE CORRIDOR", activeTicks, isP1 ? "P1" : "P2");
            BufferCore::internalLock = false;
            return;
        }

        if (player->m_isShip || player->m_isSwing) {
            // Ship / Swing thrust deflection
            if (isHolding) {
                player->releaseButton(PlayerButton::Jump);
                player->setPositionY(player->getPositionY() - 4.0f * static_cast<float>(gravDir));
            } else {
                player->pushButton(PlayerButton::Jump);
                player->setPositionY(player->getPositionY() + 4.0f * static_cast<float>(gravDir));
            }
            BufferCore::logHazardSave("SHIP/SWING", activeTicks, isP1 ? "P1" : "P2");
            BufferCore::internalLock = false;
            return;
        }

        // Cube, Robot, Spider, Ball, UFO: Force frame-perfect jump/flip on spike contact
        player->m_isOnGround = true;
        player->m_touchingGround = true;
        player->pushButton(PlayerButton::Jump);
        player->ringJump(nullptr, false);

        if (player->m_yVelocity * gravDir < 10.0) {
            player->m_yVelocity = 11.5 * gravDir;
        }
        player->setPositionY(player->getPositionY() + 4.0f * static_cast<float>(gravDir));
        pendingPress = 0;

        BufferCore::logHazardSave("GROUND / FRAME PERFECT", activeTicks, isP1 ? "P1" : "P2");
        BufferCore::internalLock = false;
    }
};
