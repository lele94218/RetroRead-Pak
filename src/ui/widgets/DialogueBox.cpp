#include <algorithm>
#include <cstdlib>

#include "text/Utf8.h"
#include "ui/widgets/DialogueBox.h"
#include "ui/ThemePalette.h"

namespace {
int uiSpacing(int normalValue, int handheldValue) {
#ifdef NEXTREADING_TG5040
    (void)normalValue;
    return handheldValue;
#else
    (void)handheldValue;
    return normalValue;
#endif
}

int dialogueBodyFont(const ReaderSettings& settings) {
#ifdef NEXTREADING_TG5040
    return std::max(24, std::min(48, static_cast<int>(settings.fontSize) + 8));
#else
    return std::max(20, std::min(36, static_cast<int>(settings.fontSize)));
#endif
}

int dialogueTitleFont() {
#ifdef NEXTREADING_TG5040
    return 30;
#else
    return 20;
#endif
}

int dialogueHintFont() {
#ifdef NEXTREADING_TG5040
    return 24;
#else
    return 16;
#endif
}

int revealSoftBand() {
#ifdef NEXTREADING_TG5040
    return 3;
#else
    return 2;
#endif
}

void drawInsetFrame(Renderer& renderer, const Rect& bounds, const Color& outer, const Color& inner) {
    renderer.drawRect(bounds, outer);
    renderer.drawRect(Rect{bounds.x + 2, bounds.y + 2, bounds.w - 4, bounds.h - 4}, inner);
}

void fillRectSafe(Renderer& renderer, const Rect& rect, const Color& color) {
    if (rect.w <= 0 || rect.h <= 0) {
        return;
    }
    renderer.fillRect(rect, color);
}

void drawCutCornerFrame(Renderer& renderer, const Rect& bounds, int thickness, int cutSize, const Color& color) {
    if (bounds.w <= cutSize * 2 || bounds.h <= cutSize * 2 || thickness <= 0) {
        return;
    }

    fillRectSafe(renderer, Rect{bounds.x + cutSize, bounds.y, bounds.w - cutSize * 2, thickness}, color);
    fillRectSafe(renderer, Rect{bounds.x + cutSize, bounds.y + bounds.h - thickness, bounds.w - cutSize * 2, thickness}, color);
    fillRectSafe(renderer, Rect{bounds.x, bounds.y + cutSize, thickness, bounds.h - cutSize * 2}, color);
    fillRectSafe(renderer, Rect{bounds.x + bounds.w - thickness, bounds.y + cutSize, thickness, bounds.h - cutSize * 2}, color);

    fillRectSafe(renderer, Rect{bounds.x, bounds.y + cutSize, cutSize, thickness}, color);
    fillRectSafe(renderer, Rect{bounds.x + cutSize, bounds.y, thickness, cutSize}, color);

    fillRectSafe(renderer, Rect{bounds.x + bounds.w - cutSize, bounds.y, thickness, cutSize}, color);
    fillRectSafe(renderer, Rect{bounds.x + bounds.w - cutSize, bounds.y + cutSize, cutSize, thickness}, color);

    fillRectSafe(renderer, Rect{bounds.x, bounds.y + bounds.h - cutSize - thickness, cutSize, thickness}, color);
    fillRectSafe(renderer, Rect{bounds.x + cutSize, bounds.y + bounds.h - cutSize, thickness, cutSize}, color);

    fillRectSafe(renderer, Rect{bounds.x + bounds.w - cutSize, bounds.y + bounds.h - cutSize, thickness, cutSize}, color);
    fillRectSafe(
        renderer,
        Rect{bounds.x + bounds.w - cutSize, bounds.y + bounds.h - cutSize - thickness, cutSize, thickness},
        color);
}
}

void DialogueBox::setBounds(const Rect& rect) {
    bounds_ = rect;
}

void DialogueBox::setTitle(const std::string& title) {
    title_ = title;
}

void DialogueBox::setBodyView(
    const std::vector<std::string>* lines,
    const std::vector<int>* lineWidths,
    const std::vector<int>* revealWidths,
    std::size_t begin,
    std::size_t count) {
    bodyLines_ = lines;
    bodyLineWidths_ = lineWidths;
    bodyRevealWidths_ = revealWidths;
    bodyBegin_ = begin;
    bodyCount_ = count;
}

void DialogueBox::setHint(const std::string& hint) {
    hint_ = hint;
}

void DialogueBox::setFootnoteLines(const std::vector<std::string>& lines) {
    footnoteLines_ = lines;
}

void DialogueBox::render(Renderer& renderer, const ReaderSettings& settings) {
    const ThemePalette palette = themePalette(settings.themePreset);
    const Color panel = palette.dialoguePanel;
    const Color border = palette.dialogueBorder;
    const Color titleColor = palette.dialogueTitle;
    const Color bodyColor = palette.dialogueBody;
    const Color hintColor = palette.dialogueHint;
    const int borderThickness = palette.dialogueBorderThickness;
    const bool drawInnerFrame = palette.dialogueInnerFrame;
    const bool useBattleFrame = palette.dialogueBattleFrame;
    const Color innerBorder = palette.dialogueInnerBorder;

    renderer.fillRect(bounds_, panel);
    if (useBattleFrame) {
        const int cut = uiSpacing(8, 12);
        drawCutCornerFrame(renderer, bounds_, borderThickness, cut, border);
    } else {
        for (int i = 0; i < borderThickness; ++i) {
            renderer.drawRect(Rect{bounds_.x - i, bounds_.y - i, bounds_.w + i * 2, bounds_.h + i * 2}, border);
        }
    }
    if (drawInnerFrame && bounds_.w > 12 && bounds_.h > 12) {
        const Rect innerRect{bounds_.x + 4, bounds_.y + 4, bounds_.w - 8, bounds_.h - 8};
        if (useBattleFrame) {
            drawCutCornerFrame(renderer, innerRect, 2, uiSpacing(6, 10), innerBorder);
        } else {
            drawInsetFrame(renderer, innerRect, innerBorder, border);
        }
    }

    const int titleFont = dialogueTitleFont();
    const int bodyFont = dialogueBodyFont(settings);
    const int hintFont = dialogueHintFont();
    const FontPreset uiFont = FontPreset::Pixel;
    const int titleHeight = renderer.lineHeight(titleFont, uiFont);
    const int bodyHeight = renderer.lineHeight(bodyFont, settings.fontPreset);
    const int hintHeight = renderer.lineHeight(hintFont, uiFont);
    const int innerX = bounds_.x + 24;
    const int innerWidth = bounds_.w - 48;
    const int bodyRenderWidth = innerWidth;
    const int titleY = bounds_.y + 16;
    const int bodyY = titleY + titleHeight + uiSpacing(14, 22);
    const int lineStep = bodyHeight + uiSpacing(6, 12);
    const int hintY = bounds_.y + bounds_.h - hintHeight - uiSpacing(10, 14);
    renderer.drawText(title_, Rect{innerX, titleY, innerWidth, titleHeight + 6}, titleColor, titleFont, TextAlign::Left, uiFont);

    if (bodyLines_ == nullptr || bodyLineWidths_ == nullptr || bodyRevealWidths_ == nullptr) {
        renderer.drawText(
            hint_,
            Rect{innerX, hintY, innerWidth, hintHeight + 4},
            hintColor,
            hintFont,
            TextAlign::Right,
            uiFont);
        return;
    }

    const bool scrambleMode = settings.textRevealMode == TextRevealMode::Scramble;
    const std::size_t bodyEnd = std::min(bodyBegin_ + bodyCount_, bodyLines_->size());
    int y = bodyY;
    for (std::size_t i = bodyBegin_; i < bodyEnd; ++i) {
        const std::size_t localIndex = i - bodyBegin_;
        const Rect textRect{innerX, y, bodyRenderWidth, bodyHeight + 8};
        const int revealWidth =
            localIndex < bodyRevealWidths_->size() ? std::max(0, (*bodyRevealWidths_)[localIndex]) : 0;
        const int fullWidth = i < bodyLineWidths_->size() ? (*bodyLineWidths_)[i] : 0;
        if (revealWidth <= 0 && !scrambleMode) {
            y += lineStep;
            continue;
        }

        if (scrambleMode) {
            const auto& line = (*bodyLines_)[i];
            const auto cps = utf8::splitCodepoints(line);
            const std::size_t total = cps.size();
            const float ratio = (fullWidth > 0) ? std::clamp(static_cast<float>(revealWidth) / fullWidth, 0.0f, 1.0f) : 0.0f;
            const std::size_t revealed = static_cast<std::size_t>(ratio * total);
            constexpr std::size_t decodeZone = 3;

            std::string display;
            for (std::size_t c = 0; c < total; ++c) {
                if (c < revealed) {
                    display += cps[c];
                } else if (c < revealed + decodeZone && revealed > 0) {
                    // Scramble zone: pick a random char from the same line
                    std::uint32_t seed = static_cast<std::uint32_t>(c * 7 + scrambleFrame_ * 3);
                    std::size_t pick = seed % total;
                    while (utf8::isWhitespace(cps[pick]) && pick + 1 < total) ++pick;
                    display += cps[pick];
                } else {
                    if (utf8::isWhitespace(cps[c])) {
                        display += cps[c];
                    } else {
                        // Random printable character seeded by position
                        std::uint32_t seed = static_cast<std::uint32_t>(c * 2654435761u + i * 97);
                        bool isCjk = cps[c].size() >= 3;
                        if (isCjk) {
                            // Random CJK character (U+4E00..U+9FFF)
                            std::uint32_t code = 0x4E00 + (seed % 0x51FF);
                            char buf[4];
                            buf[0] = static_cast<char>(0xE0 | (code >> 12));
                            buf[1] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            buf[2] = static_cast<char>(0x80 | (code & 0x3F));
                            display.append(buf, 3);
                        } else {
                            // Random ASCII letter
                            display += static_cast<char>('A' + seed % 26);
                        }
                    }
                }
            }
            if (revealWidth >= fullWidth) {
                renderer.drawText(display, textRect, bodyColor, bodyFont, TextAlign::Left, settings.fontPreset);
            } else {
                const Color halfAlpha{bodyColor.r, bodyColor.g, bodyColor.b, static_cast<uint8_t>(bodyColor.a / 2)};
                renderer.drawText(display, textRect, halfAlpha, bodyFont, TextAlign::Left, settings.fontPreset);
                if (revealWidth > 0) {
                    renderer.drawTextReveal((*bodyLines_)[i], textRect, bodyColor, bodyFont, revealWidth, 0, TextAlign::Left, settings.fontPreset);
                }
            }
        } else if (revealWidth >= fullWidth) {
            renderer.drawText(
                (*bodyLines_)[i],
                textRect,
                bodyColor,
                bodyFont,
                TextAlign::Left,
                settings.fontPreset);
        } else {
            renderer.drawTextReveal(
                (*bodyLines_)[i],
                textRect,
                bodyColor,
                bodyFont,
                revealWidth,
                revealSoftBand(),
                TextAlign::Left,
                settings.fontPreset);
        }
        y += lineStep;
    }

    if (!footnoteLines_.empty()) {
        const int fnFont = hintFont;
        const int fnLineH = hintHeight + 2;
        int fnY = hintY - static_cast<int>(footnoteLines_.size() - 1) * fnLineH;
        fnY = std::max(fnY, bodyY);
        for (const std::string& fl : footnoteLines_) {
            if (fnY + fnLineH > bounds_.y + bounds_.h) break;
            renderer.drawText(fl, Rect{innerX, fnY, innerWidth, fnLineH},
                hintColor, fnFont, TextAlign::Left, uiFont);
            fnY += fnLineH;
        }
    } else {
        renderer.drawText(
            hint_,
            Rect{innerX, hintY, innerWidth, hintHeight + 4},
            hintColor,
            hintFont,
            TextAlign::Right,
            uiFont);
    }
}
