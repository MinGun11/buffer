{
    "geode": "5.11.2",
    "gd": {
        "win": "2.2081"
    },
    "id": "ai.predictivebuffer",
    "name": "Predictive Buffer Vortex",
    "version": "v3.0.0",
    "developer": "harmfulfoid",
    "description": "All-mode predictive input buffer (Cube, Wave, Ship, Orbs, Spider, Ball, UFO, Swing) with F6/F7 Live CMD Console.",
    "settings": {
        "enabled": {
            "type": "bool",
            "name": "Enable Predictive Buffer",
            "description": "Master switch for the predictive buffer.",
            "default": true
        },
        "custom-ticks": {
            "type": "int",
            "name": "Custom Stage: Max Delay (Ticks)",
            "description": "Tick window when F6 is set to CUSTOM mode (Cube, Orbs, Spider, Ball, Robot, UFO).",
            "default": 60,
            "min": 1,
            "max": 300
        },
        "custom-wave-ticks": {
            "type": "int",
            "name": "Custom Stage: Max Wave/Ship Delay (Ticks)",
            "description": "Wave/Ship/Swing tick window when F6 is set to CUSTOM mode.",
            "default": 24,
            "min": 1,
            "max": 120
        },
        "smart-prediction": {
            "type": "bool",
            "name": "Hazard / 100 Frame-Perfect Saver",
            "description": "Automatically deflects lethal spike/corridor hitboxes within your active tick budget.",
            "default": true
        },
        "open-console": {
            "type": "bool",
            "name": "Open CMD Developer Console",
            "description": "Opens the live CMD console window on launch.",
            "default": true
        }
    }
}
