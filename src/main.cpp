#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <iostream>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

namespace BufferState {
    // Stages: 0 = OFF, 1 = 4 Ticks, 2 = 8 Ticks, 3 = VORTEX (16 Ticks), 4 = CUSTOM (mod.json sliders)
    int currentStage = 4;

    bool holdingP1 = false;
    bool holdingP2 = false;

    int pendingPressTicksP1 = 0;
    int pendingReleaseTicksP1 = 0;
    int pendingPressTicksP2 = 0;
    int pendingReleaseTicksP2 = 0;

    int hazardSaveCooldownP1 = 0;
    int hazardSaveCooldownP2 = 0;
    bool internalInputLock = false;

    int getActiveTicks(bool isWave) {
        if (!Mod::get()->getSettingValue<bool>("enabled") || currentStage == 0) {
            return 0;
        }
        switch (currentStage) {
            case 1: return isWave ? 4 : 6;
            case 2: return isWave ? 8 : 12;
            case 3: return isWave ? 16 : 32; // Vortex stage
            case 4:
            default:
                return isWave
                    ? static_cast<int>(Mod::get()->getSettingValue<int64_t>("custom-wave-ticks"))
                    : static_cast<int>(Mod::get()->getSettingValue<int64_t>("custom-ticks"));
        }
    }

    const char* getStageName() {
        switch (currentStage) {
            case 0: return "OFF";
            case 1: return "STAGE 1 (4 Ticks - Legit)";
            case 2: return "STAGE 2 (8 Ticks - Strong)";
            case 3: return "VORTEX (16W / 32C Ticks)";
            case 4: return "CUSTOM (mod.json Sliders)";
            default: return "CUSTOM";
        }
    }

    void printConsoleBanner() {
        int cTicks = static_cast<int>(Mod::get()->getSettingValue<int64_t>("custom-ticks"));
        int wTicks = static_cast<int>(Mod::get()->getSettingValue<int64_t>("custom-wave-ticks"));
        double budget = Mod::get()->getSettingValue<double>("sim-budget-ms");
        bool smart = Mod::get()->getSettingValue<bool>("smart-prediction");

        log::info("====================================================");
        log::info("[PredictiveBuffer] Active Mode : {}", getStageName());
        log::info("[PredictiveBuffer] Non-Wave Max: {} ticks | Wave Max: {} ticks",
            getActiveTicks(false), getActiveTicks(true));
        log::info("[PredictiveBuffer] Custom Cfg  : {}t / {}w | Budget: {:.1f}ms | Smart: {}",
            cTicks, wTicks, budget, smart ? "ON" : "OFF");
        log::info("[PredictiveBuffer] Hotkeys     : [F6] Cycle Stage | [F7] Jump to VORTEX/CUSTOM");
        log::info("====================================================");
    }

    bool isWaveMode(PlayerObject* p) {
        return p && p->m_isDart;
    }

    bool isContinuousMode(PlayerObject* p) {
        return p && (p->m_isShip || p->m_isDart || p->m_isSwing);
    }
}

$on_mod(Loaded) {
#ifdef GEODE_IS_WINDOWS
    if (Mod::get()->getSettingValue<bool>("open-console")) {
        if (AllocConsole()) {
            FILE* fDummy;
            freopen_s(&fDummy, "CONOUT$", "w", stdout);
            freopen_s(&fDummy, "CONOUT$", "w", stderr);
            SetConsoleTitleA("Geode - Predictive Buffer Vortex Console");
        }
    }
#endif
    BufferState::printConsoleBanner();
}

// Hotkeys F6 and F7 to switch buffer strength live in-game and update CMD console
class $modify(BufferKeyHook, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double timestamp) {
        if (isKeyDown && !isKeyRepeat) {
            if (key == enumKeyCodes::KEY_F6) {
                BufferState::currentStage = (BufferState::currentStage + 1) % 5;
                BufferState::printConsoleBanner();
            } else if (key == enumKeyCodes::KEY_F7) {
                BufferState::currentStage = (BufferState::currentStage == 3) ? 4 : 3;
                BufferState::printConsoleBanner();
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, timestamp);
    }
};

// Track raw button presses & feed the predictive tick window
class $modify(BufferPlayerHook, PlayerObject) {
    bool pushButton(PlayerButton btn) {
        if (BufferState::internalInputLock || btn != PlayerButton::Jump) {
            return PlayerObject::pushButton(btn);
        }

        auto pl = PlayLayer::get();
        if (!pl) return PlayerObject::pushButton(btn);

        bool isP1 = (this == pl->m_player1);
        bool isP2 = (this == pl->m_player2);
        int maxTicks = BufferState::getActiveTicks(BufferState::isWaveMode(this));

        if (isP1) {
            BufferState::holdingP1 = true;
            if (maxTicks > 0) BufferState::pendingPressTicksP1 = maxTicks;
        } else if (isP2) {
            BufferState::holdingP2 = true;
            if (maxTicks > 0) BufferState::pendingPressTicksP2 = maxTicks;
        }

        return PlayerObject::pushButton(btn);
    }

    bool releaseButton(PlayerButton btn) {
        if (BufferState::internalInputLock || btn != PlayerButton::Jump) {
            return PlayerObject::releaseButton(btn);
        }

        auto pl = PlayLayer::get();
        if (!pl) return PlayerObject::releaseButton(btn);

        bool isP1 = (this == pl->m_player1);
        bool isP2 = (this == pl->m_player2);
        int maxTicks = BufferState::getActiveTicks(BufferState::isWaveMode(this));

        if (isP1) {
            BufferState::holdingP1 = false;
            if (maxTicks > 0) BufferState::pendingReleaseTicksP1 = maxTicks;
        } else if (isP2) {
            BufferState::holdingP2 = false;
            if (maxTicks > 0) BufferState::pendingReleaseTicksP2 = maxTicks;
        }

        return PlayerObject::releaseButton(btn);
    }
};

// Per-tick execution for Orb Spam, Spider/Ball flips, and Cube ground frames
class $modify(BufferGameHook, GJBaseGameLayer) {
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        auto pl = PlayLayer::get();
        if (!pl || this != static_cast<GJBaseGameLayer*>(pl)) return;
        if (!Mod::get()->getSettingValue<bool>("enabled") || BufferState::currentStage == 0) return;

        bool smart = Mod::get()->getSettingValue<bool>("smart-prediction");

        auto stepPlayerBuffer = [&](PlayerObject* p, int& pressTicks, int& releaseTicks, int& saveCooldown, bool isHolding) {
            if (!p) return;
            if (saveCooldown > 0) saveCooldown--;

            if (pressTicks > 0) {
                pressTicks--;

                // Re-pulse jump & orb check during active buffer window (passes Orb Spam & Robot/Cube/Spider/Ball)
                if (smart) {
                    BufferState::internalInputLock = true;

                    if (p->m_isOnGround || p->m_isBird || p->m_isSwing) {
                        p->pushButton(PlayerButton::Jump);
                        if (p->m_isOnGround && !BufferState::isContinuousMode(p)) {
                            // Consumed buffered jump on exact first landing tick
                            pressTicks = 0;
                        }
                    } else {
                        // Trigger buffered orb / dash ring / spider / ball activation on first overlapping tick
                        p->ringJump(nullptr, false);
                    }

                    BufferState::internalInputLock = false;
                }
            }

            if (releaseTicks > 0) {
                releaseTicks--;
            }
        };

        stepPlayerBuffer(pl->m_player1, BufferState::pendingPressTicksP1, BufferState::pendingReleaseTicksP1, BufferState::hazardSaveCooldownP1, BufferState::holdingP1);
        if (pl->m_gameState.m_isDualMode) {
            stepPlayerBuffer(pl->m_player2, BufferState::pendingPressTicksP2, BufferState::pendingReleaseTicksP2, BufferState::hazardSaveCooldownP2, BufferState::holdingP2);
        }
    }
};

// Hazard-Reactive Interceptor (Passes 100 Frame Perfects, Wave Corridors, Ship/UFO/Swing spikes)
class $modify(BufferPlayLayerHook, PlayLayer) {
    void resetLevel() {
        BufferState::pendingPressTicksP1 = 0;
        BufferState::pendingReleaseTicksP1 = 0;
        BufferState::pendingPressTicksP2 = 0;
        BufferState::pendingReleaseTicksP2 = 0;
        BufferState::hazardSaveCooldownP1 = 0;
        BufferState::hazardSaveCooldownP2 = 0;
        PlayLayer::resetLevel();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        // GD calls destroyPlayer(player, m_anticheatSpike) on level start to test anticheat.
        // Always let m_anticheatSpike pass straight through.
        if (!player || object == m_anticheatSpike) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        if (!Mod::get()->getSettingValue<bool>("enabled") || BufferState::currentStage == 0) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        bool isP1 = (player == m_player1);
        int& pressTicks = isP1 ? BufferState::pendingPressTicksP1 : BufferState::pendingPressTicksP2;
        int& releaseTicks = isP1 ? BufferState::pendingReleaseTicksP1 : BufferState::pendingReleaseTicksP2;
        int& cooldown = isP1 ? BufferState::hazardSaveCooldownP1 : BufferState::hazardSaveCooldownP2;
        bool isHolding = isP1 ? BufferState::holdingP1 : BufferState::holdingP2;

        bool isWave = BufferState::isWaveMode(player);
        int maxAllowedTicks = BufferState::getActiveTicks(isWave);

        if (maxAllowedTicks <= 0) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        // Check if within active buffer window or continuous steering envelope
        bool canIntercept = (pressTicks > 0 || releaseTicks > 0 || cooldown < maxAllowedTicks);

        if (canIntercept) {
            BufferState::internalInputLock = true;
            double gravDir = player->m_isUpsideDown ? -1.0 : 1.0;

            if (isWave || player->m_isShip || player->m_isSwing) {
                // Wave / Ship / Swing: Invert vertical trajectory away from the hazard hitbox
                if (isHolding) {
                    player->releaseButton(PlayerButton::Jump);
                } else {
                    player->pushButton(PlayerButton::Jump);
                }

                // Micro-adjust position out of the spike/wall overlap based on movement direction
                float nudge = (isHolding ? -1.8f : 1.8f) * static_cast<float>(gravDir);
                player->setPositionY(player->getPositionY() + nudge);
                cooldown += 2;
            } else {
                // Cube / Ball / Spider / UFO / Robot: Execute frame-perfect jump or orb hit on spike contact
                player->m_isOnGround = true;
                player->pushButton(PlayerButton::Jump);
                player->ringJump(nullptr, false);

                if (player->m_yVelocity * gravDir < 10.0) {
                    player->m_yVelocity = 10.5 * gravDir;
                }
                player->setPositionY(player->getPositionY() + static_cast<float>(2.0 * gravDir));
                pressTicks = 0;
                cooldown += 4;
            }

            BufferState::internalInputLock = false;
            return; // Intercepted and saved!
        }

        PlayLayer::destroyPlayer(player, object);
    }
};
