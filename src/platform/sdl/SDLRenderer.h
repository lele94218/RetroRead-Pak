#pragma once

#include <string>
#include <unordered_map>

#include <SDL.h>
#ifndef NEXTREADING_NO_SDL_TTF
#include <SDL_ttf.h>
#else
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#include "platform/Renderer.h"

class SDLRenderer final : public Renderer {
public:
    SDLRenderer();
    ~SDLRenderer() override;

    bool initialize() override;
    void shutdown() override;

    void beginFrame() override;
    void endFrame() override;

    void clear(const Color& color) override;
    void fillRect(const Rect& rect, const Color& color) override;
    void drawRect(const Rect& rect, const Color& color) override;
    void setClipRect(const Rect& rect) override;
    void clearClipRect() override;
    void drawText(
        const std::string& text,
        const Rect& bounds,
        const Color& color,
        int fontSize,
        TextAlign align,
        FontPreset fontPreset) override;
    void drawTextReveal(
        const std::string& text,
        const Rect& bounds,
        const Color& color,
        int fontSize,
        int revealWidth,
        int softenWidth,
        TextAlign align,
        FontPreset fontPreset) override;

    int measureTextWidth(const std::string& text, int fontSize, FontPreset fontPreset) const override;
    int lineHeight(int fontSize, FontPreset fontPreset) const override;
    bool saveScreenshot(const std::string& path) override;

    int screenWidth() const override;
    int screenHeight() const override;
    int fullScreenHeight() const override;

private:
    struct CachedTextTexture {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
    };

#ifndef NEXTREADING_NO_SDL_TTF
    TTF_Font* fontForSize(int fontSize, FontPreset fontPreset);
#else
    FT_Face faceForPreset(FontPreset fontPreset);
#endif
    CachedTextTexture* cachedTextTexture(
        const std::string& text,
        const Color& color,
        int fontSize,
        FontPreset fontPreset);
    void clearTextTextureCache();
    std::string findFontPath(FontPreset fontPreset) const;
    SDL_Color toSdlColor(const Color& color) const;
    SDL_Rect toSdlRect(const Rect& rect) const;

    int width_ = 1280;
    int height_ = 720;
    int fullHeight_ = 720;
    int displayScale_ = 1;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unordered_map<std::string, CachedTextTexture> textTextureCache_;
#ifndef NEXTREADING_NO_SDL_TTF
    std::unordered_map<int, TTF_Font*> fontCache_;
#else
    FT_Library ftLibrary_ = nullptr;
    FT_Face normalFace_ = nullptr;
    FT_Face pixelFace_ = nullptr;
    std::string normalFacePath_;
    std::string pixelFacePath_;
#endif
};
