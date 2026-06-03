#pragma once

#include <array>

#include <SDL.h>

#include "platform/Input.h"

class VirtualButtons;

class SDLInput final : public Input {
public:
    SDLInput();

    void setVirtualButtons(VirtualButtons* vb) { virtualButtons_ = vb; }

    bool initialize() override;
    void shutdown() override;
    void poll() override;

    bool wasPressed(Action action) const override;
    bool isHeld(Action action) const override;
    bool quitRequested() const override;
    bool hadTouchActivity() const override { return touchHadActivity_; }

private:
    static std::size_t indexFor(Action action);
    const char* actionName(Action action) const;
    void logPressed(Action action, const char* source, int code) const;
    void setPressed(Action action);
    void setHeld(Action action, bool held);
    void applyQuitCombo(Action action);
    void handleKeyDown(SDL_Keycode key);
    void handleKeyUp(SDL_Keycode key);
    void handleControllerButtonDown(Uint8 button);
    void handleControllerButtonUp(Uint8 button);
    void handleJoystickButtonDown(Uint8 button);
    void handleJoystickButtonUp(Uint8 button);
    void handleJoystickHat(Uint8 value);
    bool tryMapJoystickButton(Uint8 button, Action& action) const;
    void handleFingerDown(const SDL_TouchFingerEvent& finger);
    void handleFingerUp(const SDL_TouchFingerEvent& finger);

    VirtualButtons* virtualButtons_ = nullptr;
    bool quitRequested_ = false;
    float touchStartX_ = 0.0f;
    float touchStartY_ = 0.0f;
    bool touchActive_ = false;
    bool touchHadActivity_ = false;
    SDL_GameController* controller_ = nullptr;
    SDL_Joystick* joystick_ = nullptr;
    bool preferJoystick_ = false;
    bool debugLogging_ = false;
    int buttonA_ = 0;
    int buttonB_ = 1;
    int buttonX_ = 2;
    int buttonY_ = 3;
    int buttonL1_ = 4;
    int buttonR1_ = 5;
    int buttonSelect_ = 6;
    int buttonStart_ = 7;
    std::array<bool, 14> pressed_{};
    std::array<bool, 14> held_{};
};
