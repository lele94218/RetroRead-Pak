#include "ui/VirtualButtons.h"

#include <algorithm>

void VirtualButtons::layout(int screenWidth, int contentHeight, int fullHeight) {
    screenWidth_ = screenWidth;
    contentHeight_ = contentHeight;
    fullHeight_ = fullHeight;
    areaY_ = contentHeight;
    areaHeight_ = fullHeight - contentHeight;
    if (areaHeight_ <= 0) return;

    const int cy = areaY_ + areaHeight_ / 2;
    const int bw = 50;
    const int bh = 38;
    const int sw = 44;
    const int sh = 30;

    // Left: D-pad
    const int lx = 52;
    buttons_[0] = {{lx - bw/2, cy - bh - 6, bw, bh}, {}, "Up", Action::Up};
    buttons_[1] = {{lx - bw/2, cy + 6, bw, bh}, {}, "Dn", Action::Down};
    buttons_[2] = {{lx - bw - 6, cy - bh/2, bw, bh}, {}, "<", Action::Left};
    buttons_[3] = {{lx + 6, cy - bh/2, bw, bh}, {}, ">", Action::Right};

    // Right: A / B
    const int rx = screenWidth_ - 52;
    buttons_[4] = {{rx + 2, cy - bh/2, bw, bh}, {}, "A", Action::Confirm};
    buttons_[5] = {{rx - bw - 2, cy - bh/2, bw, bh}, {}, "B", Action::Left};

    // Center: L1, Sel, Start, Menu, R1
    const int mx = screenWidth_ / 2;
    const int topY = areaY_ + 12;
    const int gap = 6;
    const int totalW = sw * 5 + gap * 4;
    const int startX = mx - totalW / 2;
    buttons_[6]  = {{startX, topY, sw, sh}, {}, "L1", Action::PrevChapter};
    buttons_[7]  = {{startX + sw + gap, topY, sw, sh}, {}, "Sel", Action::Secondary};
    buttons_[8]  = {{startX + (sw + gap) * 2, topY, sw, sh}, {}, "Str", Action::Start};
    buttons_[9]  = {{startX + (sw + gap) * 3, topY, sw, sh}, {}, "Menu", Action::Back};
    buttons_[10] = {{startX + (sw + gap) * 4, topY, sw, sh}, {}, "R1", Action::NextChapter};

    // Copy rect to touchRect with expanded hit areas
    for (int i = 0; i < 11; ++i) {
        const Rect& r = buttons_[i].rect;
        buttons_[i].touchRect = {r.x - 4, r.y - 8, r.w + 8, r.h + 16};
    }

    buttonCount_ = 11;
}

void VirtualButtons::render(Renderer& renderer) {
    if (areaHeight_ <= 0) return;

    renderer.fillRect({0, areaY_, screenWidth_, areaHeight_}, {10, 10, 14, 255});

    const Color fill{32, 32, 42, 255};
    const Color border{65, 65, 85, 255};
    const Color aFill{75, 35, 35, 255};
    const Color aBorder{150, 70, 70, 255};
    const Color bFill{35, 35, 75, 255};
    const Color bBorder{70, 70, 150, 255};
    const Color funcFill{28, 28, 35, 255};
    const Color funcBorder{55, 55, 70, 255};
    const Color txt{185, 185, 195, 255};

    for (int i = 0; i < buttonCount_; ++i) {
        Color f = fill, b = border;
        if (i == 4) { f = aFill; b = aBorder; }
        else if (i == 5) { f = bFill; b = bBorder; }
        else if (i >= 6) { f = funcFill; b = funcBorder; }

        renderer.fillRect(buttons_[i].rect, f);
        renderer.drawRect(buttons_[i].rect, b);

        const Rect& r = buttons_[i].rect;
        int fs = (i >= 6) ? 11 : 13;
        Rect tr = {r.x, r.y + (r.h - fs - 2) / 2, r.w, fs + 4};
        renderer.drawText(buttons_[i].label, tr, txt, fs, TextAlign::Center);
    }
}

VirtualButtonHit VirtualButtons::hitTest(float normalizedX, float normalizedY) const {
    if (areaHeight_ <= 0) return {false, Action::Confirm};

    const int px = static_cast<int>(normalizedX * static_cast<float>(screenWidth_));
    const int py = static_cast<int>(normalizedY * static_cast<float>(fullHeight_));

    if (py < areaY_) return {false, Action::Confirm};

    for (int i = 0; i < buttonCount_; ++i) {
        const Rect& r = buttons_[i].touchRect;
        if (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h) {
            return {true, buttons_[i].action};
        }
    }
    return {false, Action::Confirm};
}
