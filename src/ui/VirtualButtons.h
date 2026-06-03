#pragma once

#include "platform/Input.h"
#include "platform/Renderer.h"
#include "ui/ThemePalette.h"

struct VirtualButtonHit {
    bool handled = false;
    Action action = Action::Confirm;
};

class VirtualButtons {
public:
    void layout(int screenWidth, int contentHeight, int fullHeight, int topInset = 0);
    void render(Renderer& renderer, const ThemePalette& palette);
    VirtualButtonHit hitTest(float normalizedX, float normalizedY) const;

private:
    struct Button {
        Rect rect;
        Rect touchRect;
        const char* label;
        Action action;
    };

    int screenWidth_ = 0;
    int fullHeight_ = 0;
    int contentHeight_ = 0;
    int topInset_ = 0;
    int areaY_ = 0;
    int areaHeight_ = 0;
    static constexpr int kMaxButtons = 14;
    Button buttons_[kMaxButtons] = {};
    int buttonCount_ = 0;
};
