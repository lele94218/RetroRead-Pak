#include "ui/SettingsScene.h"

#include <memory>
#include <utility>

#include "app/Application.h"
#include "platform/Input.h"
#include "platform/Renderer.h"
#include "ui/ReaderScene.h"
#include "ui/ThemePalette.h"

namespace {
const char* themePresetName(ThemePreset preset) {
    switch (preset) {
    case ThemePreset::BoldAmber:
        return "Bold Amber";
    case ThemePreset::CalmTeal:
        return "Calm Teal";
    case ThemePreset::FrameBlue:
        return "Frame Blue";
    case ThemePreset::BattleRed:
        return "Battle Red";
    case ThemePreset::MintLcd:
        return "Mint LCD";
    case ThemePreset::GbInvert:
        return "GB Invert";
    case ThemePreset::ClassicDark:
    default:
        return "Classic Dark";
    }
}

const char* fontPresetName(FontPreset preset) {
    switch (preset) {
    case FontPreset::Pixel:
        return "Pixel";
    case FontPreset::Normal:
    default:
        return "Normal";
    }
}

const char* textVoiceModeName(TextVoiceMode mode) {
    switch (mode) {
    case TextVoiceMode::Fixed:
        return "Fixed";
    case TextVoiceMode::FollowText:
        return "Follow Text";
    case TextVoiceMode::Off:
    default:
        return "Off";
    }
}

const char* performanceModeName(PerformanceMode mode) {
    switch (mode) {
    case PerformanceMode::Hud:
        return "HUD";
    case PerformanceMode::Log:
        return "Log";
    case PerformanceMode::Off:
    default:
        return "Off";
    }
}

int uiFont(int normalSize, int handheldSize) {
#ifdef NEXTREADING_TG5040
    (void)normalSize;
    return handheldSize;
#else
    (void)handheldSize;
    return normalSize;
#endif
}

int uiSpacing(int normalValue, int handheldValue) {
#ifdef NEXTREADING_TG5040
    (void)normalValue;
    return handheldValue;
#else
    (void)handheldValue;
    return normalValue;
#endif
}
}

SettingsScene::SettingsScene(Application& app, BookScript book, ReadingProgress progress)
    : AppScene(app), book_(std::move(book)), progress_(std::move(progress)) {
}

void SettingsScene::update(float dt) {
    (void)dt;
    Input& input = app_.input();

    if (input.wasPressed(Action::Back) || input.wasPressed(Action::Start) || input.wasPressed(Action::OpenMenu)) {
        returnToReader();
        return;
    }

    if (input.wasPressed(Action::Up) && selectedIndex_ > 0) {
        --selectedIndex_;
        clampScroll();
    }

    if (input.wasPressed(Action::Down) && selectedIndex_ < 6) {
        ++selectedIndex_;
        clampScroll();
    }

    if (input.wasPressed(Action::Left)) {
        applyDelta(-1);
    }

    if (input.wasPressed(Action::Right) || input.wasPressed(Action::Confirm)) {
        applyDelta(1);
    }
}

void SettingsScene::render(Renderer& renderer) {
    const int screenWidth = renderer.screenWidth();
    const int margin = std::max(18, screenWidth / 24);
    const int contentWidth = screenWidth - margin * 2;
    const ReaderSettings& settings = app_.settings();
    const ThemePalette palette = themePalette(settings.themePreset);

    renderer.clear(palette.screenBackground);

    renderer.drawText(
        "Reader Settings",
        Rect{margin, 18, contentWidth, uiSpacing(40, 70)},
        palette.headerText,
        uiFont(28, 42),
        TextAlign::Left,
        settings.fontPreset);

    renderer.drawText(
        "Up/Down: select  Left/Right: change  Start: back",
        Rect{margin, uiSpacing(60, 92), contentWidth, uiSpacing(24, 32)},
        palette.secondaryText,
        uiFont(14, 22),
        TextAlign::Left,
        settings.fontPreset);

    if (settings.performanceMode == PerformanceMode::Hud) {
        renderer.drawText(
            app_.performanceHudText(),
            Rect{screenWidth - margin - 200, 18, 200, 24},
            palette.secondaryText,
            uiFont(14, 18),
            TextAlign::Right,
            settings.fontPreset);
    }

    const std::string rows[7] = {
        "Font Size: " + std::to_string(settings.fontSize),
        "Text Speed: " + std::to_string(settings.textSpeed) + " ms",
        "Sentences: " + std::to_string(settings.sentencesPerPage),
        std::string("Voice: ") + textVoiceModeName(settings.textVoiceMode),
        std::string("Theme: ") + themePresetName(settings.themePreset),
        std::string("Font: ") + fontPresetName(settings.fontPreset),
        std::string("Perf: ") + performanceModeName(settings.performanceMode),
    };

    const int rowH = uiSpacing(46, 76);
    const int rowGap = uiSpacing(4, 8);
    int y = uiSpacing(100, 140);
    const int startIndex = scrollOffset_;
    const int endIndex = std::min(7, startIndex + 5);
    for (int i = startIndex; i < endIndex; ++i) {
        const bool selected = i == selectedIndex_;
        const Rect rowRect{margin, y, contentWidth, rowH};
        if (selected) {
            renderer.fillRect(rowRect, palette.selectionFill);
            renderer.drawRect(rowRect, palette.selectionOutline);
        }
        const int textTop = rowRect.y + (rowRect.h - uiSpacing(24, 40)) / 2;
        renderer.drawText(
            rows[i],
            Rect{margin + 12, textTop, contentWidth - 24, uiSpacing(24, 40)},
            selected ? palette.selectionText : palette.primaryText,
            uiFont(20, 36),
            TextAlign::Left,
            settings.fontPreset);
        y += rowH + rowGap;
    }
}

void SettingsScene::applyDelta(int delta) {
    ReaderSettings& settings = app_.settings();
    switch (selectedIndex_) {
    case 0: {
        int value = static_cast<int>(settings.fontSize) + delta * 2;
        settings.fontSize = static_cast<std::uint32_t>(std::max(16, std::min(40, value)));
        break;
    }
    case 1: {
        int value = static_cast<int>(settings.textSpeed) + delta * 5;
        settings.textSpeed = static_cast<std::uint32_t>(std::max(5, std::min(120, value)));
        progress_.textSpeed = settings.textSpeed;
        break;
    }
    case 2: {
        int value = static_cast<int>(settings.sentencesPerPage) + delta;
        settings.sentencesPerPage = static_cast<std::uint32_t>(std::max(1, std::min(5, value)));
        break;
    }
    case 3: {
        int mode = static_cast<int>(settings.textVoiceMode);
        constexpr int kModeCount = 3;
        mode = (mode + delta + kModeCount) % kModeCount;
        settings.textVoiceMode = static_cast<TextVoiceMode>(mode);
        break;
    }
    case 4: {
        int preset = static_cast<int>(settings.themePreset);
        constexpr int kPresetCount = 7;
        preset = (preset + delta + kPresetCount) % kPresetCount;
        settings.themePreset = static_cast<ThemePreset>(preset);
        break;
    }
    case 5: {
        int preset = static_cast<int>(settings.fontPreset);
        preset = (preset + delta + 2) % 2;
        settings.fontPreset = static_cast<FontPreset>(preset);
        break;
    }
    case 6:
        if (delta != 0) {
            int mode = static_cast<int>(settings.performanceMode);
            constexpr int kModeCount = 3;
            mode = (mode + delta + kModeCount) % kModeCount;
            settings.performanceMode = static_cast<PerformanceMode>(mode);
        }
        break;
    default:
        break;
    }

    app_.settingsStore().save(app_.fileSystem(), settings);
}

void SettingsScene::returnToReader() {
    app_.sceneManager().replace(std::make_unique<ReaderScene>(app_, std::move(book_), progress_));
}

void SettingsScene::clampScroll() {
    const int visibleRows = 5;
    const int maxOffset = std::max(0, 7 - visibleRows);
    if (selectedIndex_ < scrollOffset_) {
        scrollOffset_ = selectedIndex_;
    }
    if (selectedIndex_ >= scrollOffset_ + visibleRows) {
        scrollOffset_ = selectedIndex_ - visibleRows + 1;
    }
    scrollOffset_ = std::max(0, std::min(scrollOffset_, maxOffset));
}
