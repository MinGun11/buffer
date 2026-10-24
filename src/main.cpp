#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <iostream>
#include <string>
#include <cstdio>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

namespace BufferEngine {
    // F6 Master Stages:
    // 0 = OFF
    // 1 = LIGHT (4 Ticks)
    // 2 = MEDIUM (8 Ticks)
    // 3 = VORTEX (16 Ticks)
    // 4 = EXTREME (32 Ticks)
    // 5 = CUSTOM (Uses mod.json sliders)
    int masterStage = 3; // Starts in VORTEX (16 Ticks) by default

    int bonusGroundTicks = 0;
    int bonusWaveTicks   = 0;
    bool hazardSaverEnabled = true;

    // Per-player tracking
    bool holdingP1 = false;
    bool holdingP2 = false;

    int pressBufferP1 = 0;
    int releaseBufferP1 = 0;
    int pressBufferP2 = 0;
    int releaseBufferP2 = 0;

    int saveCooldownP1 = 0;
    int saveCooldownP2 = 0;

    bool internalBypass = false;

    void sendToCMD(const std::string& text) {
#ifdef GEODE_IS_WINDOWS
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut && hOut != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteConsoleA(hOut, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
        }
#endif
        std::cout << text << std::flush;
        log::info("{}", text);
    }

    const char* getStageTitle(int stage) {
        switch (stage) {
            case 0: return "STAGE 0: OFF (Raw Input)";
            case 1: return "STAGE 1: LIGHT (4 Ticks)";
            case 2: return "STAGE 2: MEDIUM (8 Ticks)";
            case 3: return "STAGE 3: VORTEX (16 Ticks)";
            case 4: return "STAGE 4: EXTREME (32 Ticks)";
            case 5: return "STAGE 5: CUSTOM (Geode Settings)";
            default: return "UNKNOWN";
        }
    }

    int getBaseTicks(bool isWaveOrShip) {
        if (!Mod::get()->getSettingValue<bool>("enabled") || masterStage == 0) {
            return 0;
        }
        int base = 0;
        switch (masterStage) {
            case 1: base = 4; break;
            case 2: base = 8; break;
            case 3: base = 16; break;
            case 4: base = 32; break;
            case 5:
            default:
                base = isWaveOrShip
                    ? static_cast<int>(Mod::get()->getSettingValue<int64_t>("custom-wave-ticks"))
                    : static_cast<int>(Mod::get()->getSettingValue<int64_t>("custom-ticks"));
                break;
        }
        int extra = isWaveOrShip ? bonusWaveTicks : bonusGroundTicks;
        int total = base + extra;
        return (total < 0) ? 0 : total;
    }

    bool isContinuousMode(PlayerObject* p) {
        return p && (p->m_isDart || p->m_isShip || p->m_isSwing);
    }

    int getTicksForPlayer(PlayerObject* p) {
        if (!p) return 0;
        return getBaseTicks(isContinuousMode(p));
    }

    void printLiveHUD() {
        int gTicks = getBaseTicks(false);
        int wTicks = getBaseTicks(true);
        char buf[1024];
        std::snprintf(
            buf, sizeof(buf),
            "\n======================================================================\n"
            "   PREDICTIVE BUFFER VORTEX - LIVE DEVELOPER CONSOLE\n"
            "======================================================================\n"
            "  ACTIVE F6 MODE       : %s\n"
            "  CUBE/ORB/SPIDER/BALL : %d Ticks\n"
            "  WAVE/SHIP/SWING      : %d Ticks\n"
            "  100-FP HAZARD SAVER  : %s\n"
            "----------------------------------------------------------------------\n"
            "  [F6] Cycle Mode (OFF -> 4t -> 8t -> 16t VORTEX -> 32t -> CUSTOM)\n"
            "  [F7] Instant Toggle VORTEX (16t) / CUSTOM\n"
            "  [F1] +4 Ground/Orb Ticks  | [F2] +4 Wave/Ship Ticks\n"
            "  [F3] Reset Bonus Ticks    | [F4] Toggle Hazard Spike Saver\n"
            "======================================================================\n",
            getStageTitle(masterStage),
            gTicks,
            wTicks,
            hazardSaverEnabled ? "ENABLED (Auto-Deflect ON)" : "DISABLED"
        );
        sendToCMD(buf);
    }

    void logAction(const char* tag, const char* details, int ticks) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "[BUFFER] [%s] %s (Window: %d ticks)\n", tag, details, ticks);
        sendToCMD(buf);
    }

    void initConsole() {
#ifdef GEODE_IS_WINDOWS
        if (Mod::get()->getSettingValue<bool>("open-console")) {
            if (AllocConsole()) {
                FILE* fDummy = nullptr;
                freopen_s(&fDummy, "CONOUT$", "w", stdout);
                freopen_s(&fDummy, "CONOUT$", "w", stderr);
            }
            SetConsoleTitleA("Geode - Predictive Buffer Vortex (F6 / F7 Live Console)");
        }
#endif
        hazardSaverEnabled = Mod::get()->getSettingValue<bool>("smart-prediction");
        printLiveHUD();
    }

    void handleHotkey(int key) {
        if (key == 6) { // F6
            masterStage = (masterStage + 1) % 6;
            printLiveHUD();
        } else if (key == 7) { // F7
            masterStage = (masterStage == 3) ? 5 : 3;
            printLiveHUD();
        } else if (key == 1) { // F1
            bonusGroundTicks = (bonusGroundTicks + 4) % 36;
            printLiveHUD();
        } else if (key == 2) { // F2
            bonusWaveTicks = (bonusWaveTicks + 4) % 36;
            printLiveHUD();
        } else if (key == 3) { // F3
            bonusGroundTicks = 0;
            bonusWaveTicks = 0;
            printLiveHUD();
        } else if (key == 4) { // F4
            hazardSaverEnabled = !hazardSaverEnabled;
            printLiveHUD();
        }
    }
}

$on_mod(Loaded) {
    BufferEngine::initConsole();
}

// 1. Global Keyboard Hook (Works in menus, pause screen, and inside levels)
class $modify(BufferKeyHook, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double timestamp) {
        if (isKeyDown && !isKeyRepeat) {
            if (key == enumKeyCodes::KEY_F6) {
                BufferEngine::handleHotkey(6);
            } else if (key == enumKeyCodes::KEY_F7) {
                BufferEngine::handleHotkey(7);
            } else if (key == enumKeyCodes::KEY_F1) {
                BufferEngine::handleHotkey(1);
            } else if (key == enumKeyCodes::KEY_F2) {
                BufferEngine::handleHotkey(2);
            } else if (key == enumKeyCodes::KEY_F3) {
                BufferEngine::handleHotkey(3);
            } else if (key == enumKeyCodes::KEY_F4) {
                BufferEngine::handleHotkey(4);
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, timestamp);
    }
};

// 2. Player Input Hook: Captures early clicks & releases into the tick buffer
class $modify(BufferPlayerHook, PlayerObject) {
    bool pushButton(PlayerButton btn) {
        if (BufferEngine::internalBypass || btn != PlayerButton::Jump) {
            return PlayerObject::pushButton(btn);
        }

        auto pl = PlayLayer::get();
        if (pl) {
            int maxTicks = BufferEngine::getTicksForPlayer(this);
            if (this == pl->m_player1) {
                BufferEngine::holdingP1 = true;
                if (maxTicks > 0) BufferEngine::pressBufferP1 = maxTicks;
            } else if (this == pl->m_player2) {
                BufferEngine::holdingP2 = true;
                if (maxTicks > 0) BufferEngine::pressBufferP2 = maxTicks;
            }
        }

        return PlayerObject::pushButton(btn);
    }

    bool releaseButton(PlayerButton btn) {
        if (BufferEngine::internalBypass || btn != PlayerButton::Jump) {
            return PlayerObject::releaseButton(btn);
        }

        auto pl = PlayLayer::get();
        if (pl) {
            int maxTicks = BufferEngine::getTicksForPlayer(this);
            if (this == pl->m_player1) {
                BufferEngine::holdingP1 = false;
                if (maxTicks > 0) BufferEngine::releaseBufferP1 = maxTicks;
            } else if (this == pl->m_player2) {
                BufferEngine::holdingP2 = false;
                if (maxTicks > 0) BufferEngine::releaseBufferP2 = maxTicks;
            }
        }

        return PlayerObject::releaseButton(btn);
    }
};

// 3. Per-Tick Physics Command Hook: Solves Orb Spam, Spider/Ball Flips, UFO, and Early Ground Jumps
class $modify(BufferCommandHook, GJBaseGameLayer) {
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        auto pl = PlayLayer::get();
        if (!pl || this != static_cast<GJBaseGameLayer*>(pl)) return;
        if (!Mod::get()->getSettingValue<bool>("enabled") || BufferEngine::masterStage == 0) return;

        auto processPlayerTick = [&](PlayerObject* p, int& pressBuf, int& releaseBuf, int& cooldown, bool isHolding) {
            if (!p || p->m_isDead) return;
            if (cooldown > 0) cooldown--;

            if (pressBuf > 0) {
                pressBuf--;

                // For non-continuous modes (Cube, Orbs, Robot, Spider, Ball, UFO):
                // Re-pulsing releaseButton -> pushButton forces GD's native ringJump and ground/spider
                // checks to fire on the exact first sub-tick you enter an orb hitbox or touch ground!
                if (!BufferEngine::isContinuousMode(p) && isHolding) {
                    BufferEngine::internalBypass = true;

                    double velBefore = p->m_yVelocity;
                    bool groundBefore = p->m_isOnGround;
                    bool upsideBefore = p->m_isUpsideDown;

                    p->releaseButton(PlayerButton::Jump);
                    p->pushButton(PlayerButton::Jump);

                    // If the re-pulse triggered a jump, orb, or gravity flip, consume the buffer
                    if (p->m_yVelocity != velBefore || (groundBefore && !p->m_isOnGround) || p->m_isUpsideDown != upsideBefore) {
                        pressBuf = 0;
                    }

                    BufferEngine::internalBypass = false;
                }
            }

            if (releaseBuf > 0) {
                releaseBuf--;
            }
        };

        processPlayerTick(
            pl->m_player1,
            BufferEngine::pressBufferP1,
            BufferEngine::releaseBufferP1,
            BufferEngine::saveCooldownP1,
            BufferEngine::holdingP1
        );

        if (pl->m_gameState.m_isDualMode) {
            processPlayerTick(
                pl->m_player2,
                BufferEngine::pressBufferP2,
                BufferEngine::releaseBufferP2,
                BufferEngine::saveCooldownP2,
                BufferEngine::holdingP2
            );
        }
    }
};

// 4. Hazard & 100 Frame-Perfect Interceptor: Saves late jumps and tight Wave/Ship/Swing corridors
class $modify(BufferPlayLayerHook, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        BufferEngine::pressBufferP1 = 0;
        BufferEngine::releaseBufferP1 = 0;
        BufferEngine::pressBufferP2 = 0;
        BufferEngine::releaseBufferP2 = 0;
        BufferEngine::saveCooldownP1 = 0;
        BufferEngine::saveCooldownP2 = 0;
        return true;
    }

    void resetLevel() {
        BufferEngine::pressBufferP1 = 0;
        BufferEngine::releaseBufferP1 = 0;
        BufferEngine::pressBufferP2 = 0;
        BufferEngine::releaseBufferP2 = 0;
        BufferEngine::saveCooldownP1 = 0;
        BufferEngine::saveCooldownP2 = 0;
        PlayLayer::resetLevel();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        // Always allow RobTop's anticheat check spike on level start
        if (!player || object == m_anticheatSpike) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        if (!Mod::get()->getSettingValue<bool>("enabled") ||
            BufferEngine::masterStage == 0 ||
            !BufferEngine::hazardSaverEnabled) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        int maxTicks = BufferEngine::getTicksForPlayer(player);
        if (maxTicks <= 0) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        bool isP1 = (player == m_player1);
        int& pressBuf   = isP1 ? BufferEngine::pressBufferP1   : BufferEngine::pressBufferP2;
        int& releaseBuf = isP1 ? BufferEngine::releaseBufferP1 : BufferEngine::releaseBufferP2;
        int& cooldown   = isP1 ? BufferEngine::saveCooldownP1  : BufferEngine::saveCooldownP2;
        bool& isHolding = isP1 ? BufferEngine::holdingP1       : BufferEngine::holdingP2;

        // If cooldown is within our active tick allowance, intercept the hazard
        if (cooldown <= maxTicks) {
            BufferEngine::internalBypass = true;
            player->m_isDead = false;
            double grav = player->m_isUpsideDown ? -1.0 : 1.0;

            if (player->m_isDart) {
                // WAVE MODE: Corridor deflection (Invert hold/release and step out of hitbox)
                if (isHolding) {
                    player->releaseButton(PlayerButton::Jump);
                    isHolding = false;
                    player->setPositionY(player->getPositionY() - static_cast<float>(3.2 * grav));
                } else {
                    player->pushButton(PlayerButton::Jump);
                    isHolding = true;
                    player->setPositionY(player->getPositionY() + static_cast<float>(3.2 * grav));
                }
                cooldown += 2;
                BufferEngine::logAction("WAVE SAVE", "Deflected corridor hitbox", maxTicks);
                BufferEngine::internalBypass = false;
                return;
            }

            if (player->m_isShip || player->m_isSwing) {
                // SHIP / SWING MODE: Invert vertical thrust and clear obstacle boundary
                if (isHolding) {
                    player->releaseButton(PlayerButton::Jump);
                    player->m_yVelocity = -4.5 * grav;
                    player->setPositionY(player->getPositionY() - static_cast<float>(3.5 * grav));
                } else {
                    player->pushButton(PlayerButton::Jump);
                    player->m_yVelocity = 4.5 * grav;
                    player->setPositionY(player->getPositionY() + static_cast<float>(3.5 * grav));
                }
                cooldown += 2;
                BufferEngine::logAction("SHIP/SWING SAVE", "Adjusted flight vector", maxTicks);
                BufferEngine::internalBypass = false;
                return;
            }

            if (player->m_isBall || player->m_isSpider) {
                // BALL / SPIDER: Force ground state and execute gravity flip / teleport
                player->m_isOnGround = true;
                player->releaseButton(PlayerButton::Jump);
                player->pushButton(PlayerButton::Jump);
                player->setPositionY(player->getPositionY() + static_cast<float>(3.5 * grav));
                pressBuf = 0;
                cooldown += 3;
                BufferEngine::logAction("BALL/SPIDER SAVE", "Executed frame-perfect flip", maxTicks);
                BufferEngine::internalBypass = false;
                return;
            }

            // CUBE / ROBOT / UFO: Execute 100-frame-perfect ground jump or orb hit on spike contact
            player->m_isOnGround = true;
            player->releaseButton(PlayerButton::Jump);
            player->pushButton(PlayerButton::Jump);

            if (player->m_yVelocity * grav < 10.5) {
                player->m_yVelocity = 11.2 * grav;
            }
            player->setPositionY(player->getPositionY() + static_cast<float>(3.5 * grav));
            pressBuf = 0;
            releaseBuf = 0;
            cooldown += 3;

            BufferEngine::logAction("FRAME-PERFECT SAVE", "Triggered jump on spike hitbox", maxTicks);
            BufferEngine::internalBypass = false;
            return;
        }

        PlayLayer::destroyPlayer(player, object);
    }
};
