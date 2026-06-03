#include "ui/VirtualButtons.h"

#include <algorithm>
#include <cmath>

namespace {
void fillCircle(Renderer& renderer, int cx, int cy, int r, const Color& fill, const Color& border, int borderW = 2) {
    const float outerR = static_cast<float>(r);
    const float innerR = static_cast<float>(r - borderW);
    for (int dy = -r; dy <= r; ++dy) {
        const float fy = static_cast<float>(dy);
        const float outerX = std::sqrt(std::max(0.0f, outerR * outerR - fy * fy));
        const int dx = static_cast<int>(outerX);
        if (dx <= 0) continue;
        // Border band (left + right)
        const float innerX = (innerR > 0.0f) ? std::sqrt(std::max(0.0f, innerR * innerR - fy * fy)) : 0.0f;
        const int idx = static_cast<int>(innerX);
        if (idx > 0) {
            renderer.fillRect({cx - dx, cy + dy, dx - idx, 1}, border);
            renderer.fillRect({cx + idx, cy + dy, dx - idx, 1}, border);
            renderer.fillRect({cx - idx, cy + dy, idx * 2, 1}, fill);
        } else {
            renderer.fillRect({cx - dx, cy + dy, dx * 2, 1}, border);
        }
    }
}
}

void VirtualButtons::layout(int screenWidth, int contentHeight, int fullHeight, int topInset) {
    screenWidth_ = screenWidth;
    contentHeight_ = contentHeight;
    fullHeight_ = fullHeight;
    topInset_ = topInset;
    areaY_ = contentHeight;
    areaHeight_ = fullHeight - contentHeight - topInset;
    if (areaHeight_ <= 0) return;

    const int cy = areaY_ + areaHeight_ / 2;
    const int btnR = 18;
    const int smallR = 14;

    // Left: D-pad
    const int lx = 56;
    const int spread = 26;
    buttons_[0] = {{lx - btnR, cy - spread - btnR, btnR*2, btnR*2}, {}, "U", Action::Up};
    buttons_[1] = {{lx - btnR, cy + spread - btnR, btnR*2, btnR*2}, {}, "D", Action::Down};
    buttons_[2] = {{lx - spread - btnR, cy - btnR, btnR*2, btnR*2}, {}, "<", Action::Left};
    buttons_[3] = {{lx + spread - btnR, cy - btnR, btnR*2, btnR*2}, {}, ">", Action::Right};

    // Right: A/B/X/Y diamond layout
    const int rx = screenWidth_ - 56;
    const int faceSpread = 26;
    buttons_[4] = {{rx + faceSpread - btnR, cy - btnR, btnR*2, btnR*2}, {}, "A", Action::Confirm};
    buttons_[5] = {{rx - faceSpread - btnR, cy - btnR, btnR*2, btnR*2}, {}, "Y", Action::Secondary};
    buttons_[6] = {{rx - btnR, cy - faceSpread - btnR, btnR*2, btnR*2}, {}, "X", Action::ToggleAuto};
    buttons_[7] = {{rx - btnR, cy + faceSpread - btnR, btnR*2, btnR*2}, {}, "B", Action::Left};

    // Center: L1 Sel Str Menu R1
    const int mx = screenWidth_ / 2;
    const int gap = smallR * 2 + 6;
    const int half = gap * 2;
    const int funcY = cy;
    buttons_[8]  = {{mx - half - smallR, funcY - smallR, smallR*2, smallR*2}, {}, "L", Action::PrevChapter};
    buttons_[9]  = {{mx - gap - smallR, funcY - smallR, smallR*2, smallR*2}, {}, "Se", Action::Secondary};
    buttons_[10] = {{mx - smallR, funcY - smallR, smallR*2, smallR*2}, {}, "St", Action::Start};
    buttons_[11] = {{mx + gap - smallR, funcY - smallR, smallR*2, smallR*2}, {}, "Mn", Action::Back};
    buttons_[12] = {{mx + half - smallR, funcY - smallR, smallR*2, smallR*2}, {}, "R", Action::NextChapter};

    for (int i = 0; i < 13; ++i) {
        const Rect& r = buttons_[i].rect;
        const int expand = 6;
        buttons_[i].touchRect = {r.x - expand, r.y - expand, r.w + expand*2, r.h + expand*2};
    }

    buttonCount_ = 13;
}

void VirtualButtons::render(Renderer& renderer, const ThemePalette& palette) {
    if (areaHeight_ <= 0) return;

    renderer.fillRect({0, areaY_, screenWidth_, areaHeight_}, palette.screenBackground);

    auto blend = [](const Color& c, int pct) -> Color {
        return {static_cast<uint8_t>(std::min(255, c.r * pct / 100)),
                static_cast<uint8_t>(std::min(255, c.g * pct / 100)),
                static_cast<uint8_t>(std::min(255, c.b * pct / 100)), c.a};
    };

    const Color btnFill = blend(palette.dialoguePanel, 70);
    const Color btnBorder = blend(palette.dialogueBorder, 80);
    const Color funcFill = blend(palette.screenBackground, 130);
    const Color funcBorder = blend(palette.dialogueBorder, 50);
    const Color txt = palette.secondaryText;

    for (int i = 0; i < buttonCount_; ++i) {
        Color f = btnFill, b = btnBorder;
        if (i == 4) { f = palette.selectionFill; b = palette.selectionOutline; }
        else if (i >= 8) { f = funcFill; b = funcBorder; }

        const Rect& r = buttons_[i].rect;
        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        const int rad = r.w / 2;

        fillCircle(renderer, cx, cy, rad, f, b, 2);

        const int fs = (i >= 8) ? 9 : 11;
        const Color& labelColor = (i == 4) ? palette.selectionText : txt;
        Rect tr = {r.x, r.y + (r.h - fs - 2) / 2, r.w, fs + 4};
        renderer.drawText(buttons_[i].label, tr, labelColor, fs, TextAlign::Center);
    }
}

VirtualButtonHit VirtualButtons::hitTest(float normalizedX, float normalizedY) const {
    if (areaHeight_ <= 0) return {false, Action::Confirm};

    const int px = static_cast<int>(normalizedX * static_cast<float>(screenWidth_));
    const int py = static_cast<int>(normalizedY * static_cast<float>(fullHeight_)) - topInset_;

    if (py < areaY_) return {false, Action::Confirm};

    for (int i = 0; i < buttonCount_; ++i) {
        const Rect& r = buttons_[i].touchRect;
        if (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h) {
            return {true, buttons_[i].action};
        }
    }
    return {false, Action::Confirm};
}
