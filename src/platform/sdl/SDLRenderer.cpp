#include "platform/sdl/SDLRenderer.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <utility>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "text/Utf8.h"

namespace fs = std::filesystem;

namespace {
#ifdef NEXTREADING_NO_SDL_TTF
std::uint32_t decodeUtf8CodepointGeneric(const std::string& codepoint) {
    if (codepoint.empty()) {
        return 0;
    }

    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(codepoint.data());
    const unsigned char lead = bytes[0];
    if (lead < 0x80) {
        return lead;
    }
    if ((lead >> 5) == 0x6 && codepoint.size() >= 2) {
        return ((lead & 0x1F) << 6) | (bytes[1] & 0x3F);
    }
    if ((lead >> 4) == 0xE && codepoint.size() >= 3) {
        return ((lead & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F);
    }
    if ((lead >> 3) == 0x1E && codepoint.size() >= 4) {
        return ((lead & 0x07) << 18) | ((bytes[1] & 0x3F) << 12) | ((bytes[2] & 0x3F) << 6) |
               (bytes[3] & 0x3F);
    }
    return '?';
}
#endif

#ifndef NEXTREADING_NO_SDL_TTF
int fontCacheKey(int size, FontPreset preset) {
    return size * 10 + static_cast<int>(preset);
}
#endif

int renderFontSize(int size, FontPreset preset) {
    if (preset == FontPreset::Pixel && size >= 20) {
        return std::max(10, size / 2);
    }
    return size;
}

int renderScale(int size, FontPreset preset) {
    if (preset == FontPreset::Pixel && size >= 20) {
        return 2;
    }
    return 1;
}

std::string textTextureCacheKey(const std::string& text, const Color& color, int fontSize, FontPreset fontPreset) {
    return std::to_string(fontSize) + "|" + std::to_string(static_cast<int>(fontPreset)) + "|" +
           std::to_string(color.r) + "," + std::to_string(color.g) + "," + std::to_string(color.b) + "," +
           std::to_string(color.a) + "|" + text;
}

#ifdef NEXTREADING_NO_SDL_TTF
std::uint32_t decodeUtf8Codepoint(const std::string& codepoint) {
    return decodeUtf8CodepointGeneric(codepoint);
}

int freeTypeLoadFlags(FontPreset preset, bool renderGlyph) {
    int flags = renderGlyph ? FT_LOAD_RENDER : FT_LOAD_DEFAULT;
    if (preset == FontPreset::Pixel) {
        flags |= FT_LOAD_TARGET_MONO;
        if (renderGlyph) {
            flags |= FT_LOAD_MONOCHROME;
        }
    }
    return flags;
}

std::uint8_t monoBitmapCoverage(const FT_Bitmap& bitmap, int row, int col) {
    const int byteIndex = row * bitmap.pitch + (col / 8);
    const int bitIndex = 7 - (col % 8);
    return (bitmap.buffer[byteIndex] & (1 << bitIndex)) != 0 ? 255 : 0;
}
#endif
}

SDLRenderer::SDLRenderer() = default;

SDLRenderer::~SDLRenderer() {
    shutdown();
}

bool SDLRenderer::initialize() {
    if (const char* w = std::getenv("NEXTREADING_SCREEN_WIDTH")) {
        width_ = std::max(320, std::atoi(w));
    }
    if (const char* h = std::getenv("NEXTREADING_SCREEN_HEIGHT")) {
        height_ = std::max(240, std::atoi(h));
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return false;
    }

#ifndef NEXTREADING_NO_SDL_TTF
    if (TTF_Init() != 0) {
        SDL_Quit();
        return false;
    }
#else
    if (FT_Init_FreeType(&ftLibrary_) != 0) {
        SDL_Quit();
        return false;
    }
#endif

    Uint32 windowFlags = SDL_WINDOW_SHOWN;
#if defined(__APPLE__) && TARGET_OS_IOS
    windowFlags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALLOW_HIGHDPI;
#elif defined(NEXTREADING_TG5040)
    windowFlags |= SDL_WINDOW_FULLSCREEN;
#endif

    window_ = SDL_CreateWindow(
        "RetroRead",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width_,
        height_,
        windowFlags);
    if (window_ == nullptr) {
        shutdown();
        return false;
    }

#if defined(__APPLE__) && TARGET_OS_IOS
    SDL_GetWindowSize(window_, &width_, &height_);
    fullHeight_ = height_;
    extern int iosTopSafeInset();
    topInset_ = iosTopSafeInset();
    if (height_ > width_) {
        height_ = (fullHeight_ - topInset_) * 3 / 4;
    }
#endif

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer_ == nullptr) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer_ == nullptr) {
        shutdown();
        return false;
    }

#if defined(__APPLE__) && TARGET_OS_IOS
    {
        int pixelW = 0, pixelH = 0;
        SDL_GetRendererOutputSize(renderer_, &pixelW, &pixelH);
        displayScale_ = (width_ > 0) ? std::max(1, pixelW / width_) : 1;
        SDL_RenderSetLogicalSize(renderer_, width_, fullHeight_);
        SDL_Log("RetroRead: logical %dx%d pixel %dx%d scale %d content %d",
                width_, fullHeight_, pixelW, pixelH, displayScale_, height_);
    }
#elif defined(NEXTREADING_TG5040)
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "0");
    SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN);
    SDL_RenderSetLogicalSize(renderer_, width_, height_);
#endif

#if !defined(__APPLE__) || !TARGET_OS_IOS
    SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
    SDL_SetWindowGrab(window_, SDL_TRUE);
#endif
    SDL_RaiseWindow(window_);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
#ifndef NEXTREADING_NO_SDL_TTF
    return !findFontPath(FontPreset::Normal).empty();
#else
    return !findFontPath(FontPreset::Normal).empty();
#endif
}

void SDLRenderer::shutdown() {
    clearTextTextureCache();
#ifndef NEXTREADING_NO_SDL_TTF
    for (auto& [size, font] : fontCache_) {
        (void)size;
        if (font != nullptr) {
            TTF_CloseFont(font);
        }
    }
    fontCache_.clear();

    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }

    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    if (TTF_WasInit() != 0) {
        TTF_Quit();
    }
#endif
#ifdef NEXTREADING_NO_SDL_TTF
    if (normalFace_ != nullptr) {
        FT_Done_Face(normalFace_);
        normalFace_ = nullptr;
    }
    if (pixelFace_ != nullptr) {
        FT_Done_Face(pixelFace_);
        pixelFace_ = nullptr;
    }
    if (ftLibrary_ != nullptr) {
        FT_Done_FreeType(ftLibrary_);
        ftLibrary_ = nullptr;
    }
#endif

    SDL_Quit();
}

void SDLRenderer::beginFrame() {
}

void SDLRenderer::endFrame() {
    SDL_RenderPresent(renderer_);
}

void SDLRenderer::clear(const Color& color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer_);
}

void SDLRenderer::fillRect(const Rect& rect, const Color& color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    const SDL_Rect sdlRect = toSdlRect(rect);
    SDL_RenderFillRect(renderer_, &sdlRect);
}

void SDLRenderer::drawRect(const Rect& rect, const Color& color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    const SDL_Rect sdlRect = toSdlRect(rect);
    SDL_RenderDrawRect(renderer_, &sdlRect);
}

void SDLRenderer::setClipRect(const Rect& rect) {
    const SDL_Rect sdlRect = toSdlRect(rect);
    SDL_RenderSetClipRect(renderer_, &sdlRect);
}

void SDLRenderer::clearClipRect() {
    SDL_RenderSetClipRect(renderer_, nullptr);
}

void SDLRenderer::drawText(
    const std::string& text,
    const Rect& bounds,
    const Color& color,
    int fontSize,
    TextAlign align,
    FontPreset fontPreset) {
    if (text.empty()) {
        return;
    }

    CachedTextTexture* cached = cachedTextTexture(text, color, fontSize, fontPreset);
    if (cached == nullptr || cached->texture == nullptr) {
        return;
    }

    SDL_Rect target = toSdlRect(bounds);
    if (align == TextAlign::Center) {
        target.x = bounds.x + (bounds.w - cached->width) / 2;
    } else if (align == TextAlign::Right) {
        target.x = bounds.x + bounds.w - cached->width;
    }
    target.w = cached->width;
    target.h = cached->height;

    SDL_SetTextureAlphaMod(cached->texture, color.a);
    SDL_RenderCopy(renderer_, cached->texture, nullptr, &target);
}

void SDLRenderer::drawTextReveal(
    const std::string& text,
    const Rect& bounds,
    const Color& color,
    int fontSize,
    int revealWidth,
    int softenWidth,
    TextAlign align,
    FontPreset fontPreset) {
    if (text.empty() || revealWidth <= 0) {
        return;
    }

    CachedTextTexture* cached = cachedTextTexture(text, color, fontSize, fontPreset);
    if (cached == nullptr || cached->texture == nullptr) {
        return;
    }

    SDL_Rect target = toSdlRect(bounds);
    if (align == TextAlign::Center) {
        target.x = bounds.x + (bounds.w - cached->width) / 2;
    } else if (align == TextAlign::Right) {
        target.x = bounds.x + bounds.w - cached->width;
    }
    target.w = cached->width;
    target.h = cached->height;

    const int clampedReveal = std::max(0, std::min(revealWidth, target.w));
    const int clampedSoft = std::max(0, std::min(softenWidth, clampedReveal));
    const int solidWidth = std::max(0, clampedReveal - clampedSoft);
    const int texScale = std::max(1, displayScale_);

    SDL_SetTextureBlendMode(cached->texture, SDL_BLENDMODE_BLEND);

    if (solidWidth > 0) {
        SDL_Rect src{0, 0, std::max(1, solidWidth * texScale), target.h * texScale};
        SDL_Rect dst{target.x, target.y, solidWidth, target.h};
        SDL_SetTextureAlphaMod(cached->texture, color.a);
        SDL_RenderCopy(renderer_, cached->texture, &src, &dst);
    }

    if (clampedSoft > 0) {
        const int band1 = std::max(1, clampedSoft / 4);
        const int band2 = std::max(1, clampedSoft / 4);
        const int band3 = std::max(1, clampedSoft / 4);
        const int band4 = std::max(1, clampedSoft - band1 - band2 - band3);
        const std::array<int, 4> bandWidths{band1, band2, band3, band4};
        const std::array<std::uint8_t, 4> bandAlpha{
            static_cast<std::uint8_t>(color.a / 8),
            static_cast<std::uint8_t>(color.a * 3 / 8),
            static_cast<std::uint8_t>(color.a * 5 / 8),
            static_cast<std::uint8_t>(color.a * 7 / 8),
        };

        int consumed = 0;
        for (std::size_t i = 0; i < bandWidths.size(); ++i) {
            const int bandWidth = bandWidths[i];
            if (bandWidth <= 0) {
                continue;
            }
            SDL_Rect src{
                std::max(0, (solidWidth + consumed) * texScale),
                0,
                std::max(1, bandWidth * texScale),
                target.h * texScale};
            SDL_Rect dst{target.x + solidWidth + consumed, target.y, bandWidth, target.h};
            SDL_SetTextureAlphaMod(cached->texture, bandAlpha[i]);
            SDL_RenderCopy(renderer_, cached->texture, &src, &dst);
            consumed += bandWidth;
        }
        SDL_SetTextureAlphaMod(cached->texture, color.a);
    }
}

SDLRenderer::CachedTextTexture* SDLRenderer::cachedTextTexture(
    const std::string& text,
    const Color& color,
    int fontSize,
    FontPreset fontPreset) {
    if (text.empty()) {
        return nullptr;
    }

    const std::string key = textTextureCacheKey(text, color, fontSize, fontPreset);
    auto it = textTextureCache_.find(key);
    if (it != textTextureCache_.end()) {
        return &it->second;
    }

    if (textTextureCache_.size() >= 128) {
        clearTextTextureCache();
    }

    CachedTextTexture cached;

#ifndef NEXTREADING_NO_SDL_TTF
    const int hiDpiFontSize = fontSize * displayScale_;
    TTF_Font* font = fontForSize(hiDpiFontSize, fontPreset);
    if (font == nullptr) {
        return nullptr;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), toSdlColor(color));
    if (surface == nullptr) {
        return nullptr;
    }

    cached.texture = SDL_CreateTextureFromSurface(renderer_, surface);
    if (cached.texture == nullptr) {
        SDL_FreeSurface(surface);
        return nullptr;
    }

    if (fontPreset == FontPreset::Pixel && displayScale_ <= 1) {
        SDL_SetTextureScaleMode(cached.texture, SDL_ScaleModeNearest);
    } else {
        SDL_SetTextureScaleMode(cached.texture, SDL_ScaleModeLinear);
    }

    cached.width = surface->w / displayScale_;
    cached.height = surface->h / displayScale_;
    SDL_FreeSurface(surface);
#else
    FT_Face face = faceForPreset(fontPreset);
    if (face == nullptr) {
        return nullptr;
    }

    const int renderSize = renderFontSize(fontSize, fontPreset);
    const int scale = renderScale(fontSize, fontPreset);
    FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(renderSize));

    const int unscaledWidth = measureTextWidth(text, fontSize, fontPreset) / std::max(1, scale);
    const int surfaceW = std::max(1, unscaledWidth);
    const int surfaceH = std::max(1, lineHeight(fontSize, fontPreset) / std::max(1, scale));
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, surfaceW, surfaceH, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        return nullptr;
    }
    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 0, 0, 0, 0));

    const auto codepoints = utf8::splitCodepoints(text);
    int baseline = std::max(renderSize, static_cast<int>(face->size->metrics.ascender >> 6));
    baseline = std::min(surfaceH - 1, baseline);
    int penX = 0;
    std::uint8_t* pixels = static_cast<std::uint8_t*>(surface->pixels);
    const int pitch = surface->pitch;

    for (const std::string& cp : codepoints) {
        if (utf8::isWhitespace(cp)) {
            penX += std::max(2, renderSize / 3);
            continue;
        }

        const std::uint32_t code = decodeUtf8Codepoint(cp);
        if (FT_Load_Char(face, code, freeTypeLoadFlags(fontPreset, true)) != 0) {
            continue;
        }

        FT_GlyphSlot glyph = face->glyph;
        const FT_Bitmap& bmp = glyph->bitmap;
        const int glyphX = penX + glyph->bitmap_left;
        const int glyphY = baseline - glyph->bitmap_top;

        for (int row = 0; row < static_cast<int>(bmp.rows); ++row) {
            const int dstY = glyphY + row;
            if (dstY < 0 || dstY >= surfaceH) {
                continue;
            }
            for (int col = 0; col < static_cast<int>(bmp.width); ++col) {
                const int dstX = glyphX + col;
                if (dstX < 0 || dstX >= surfaceW) {
                    continue;
                }

                std::uint8_t coverage = 0;
                if (bmp.pixel_mode == FT_PIXEL_MODE_MONO) {
                    coverage = monoBitmapCoverage(bmp, row, col);
                } else {
                    coverage = bmp.buffer[row * bmp.pitch + col];
                }
                if (coverage == 0) {
                    continue;
                }

                std::uint8_t* px = pixels + dstY * pitch + dstX * 4;
                px[0] = color.r;
                px[1] = color.g;
                px[2] = color.b;
                px[3] = static_cast<std::uint8_t>((coverage * color.a) / 255);
            }
        }

        penX += static_cast<int>(glyph->advance.x >> 6);
    }

    cached.texture = SDL_CreateTextureFromSurface(renderer_, surface);
    if (cached.texture == nullptr) {
        SDL_FreeSurface(surface);
        return nullptr;
    }
    if (fontPreset == FontPreset::Pixel) {
        SDL_SetTextureScaleMode(cached.texture, SDL_ScaleModeNearest);
    }
    cached.width = surfaceW * scale;
    cached.height = surfaceH * scale;
    SDL_FreeSurface(surface);
#endif

    auto [insertedIt, inserted] = textTextureCache_.emplace(key, cached);
    if (!inserted && cached.texture != nullptr) {
        SDL_DestroyTexture(cached.texture);
    }
    return inserted ? &insertedIt->second : &insertedIt->second;
}

void SDLRenderer::clearTextTextureCache() {
    for (auto& [key, cached] : textTextureCache_) {
        (void)key;
        if (cached.texture != nullptr) {
            SDL_DestroyTexture(cached.texture);
        }
    }
    textTextureCache_.clear();
}

int SDLRenderer::measureTextWidth(const std::string& text, int fontSize, FontPreset fontPreset) const {
#ifndef NEXTREADING_NO_SDL_TTF
    const int hiDpiFontSize = fontSize * displayScale_;
    auto* self = const_cast<SDLRenderer*>(this);
    TTF_Font* font = self->fontForSize(hiDpiFontSize, fontPreset);
    if (font == nullptr || text.empty()) {
        return 0;
    }

    int width = 0;
    if (TTF_SizeUTF8(font, text.c_str(), &width, nullptr) != 0) {
        return 0;
    }
    return width / displayScale_;
#else
    if (text.empty()) {
        return 0;
    }

    FT_Face face = const_cast<SDLRenderer*>(this)->faceForPreset(fontPreset);
    if (face == nullptr) {
        return 0;
    }

    const int renderSize = renderFontSize(fontSize, fontPreset);
    FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(renderSize));
    const auto codepoints = utf8::splitCodepoints(text);
    int width = 0;
    for (const std::string& codepoint : codepoints) {
        if (utf8::isWhitespace(codepoint)) {
            width += std::max(2, renderSize / 3);
            continue;
        }
        const std::uint32_t code = decodeUtf8Codepoint(codepoint);
        if (FT_Load_Char(face, code, freeTypeLoadFlags(fontPreset, false)) == 0) {
            width += static_cast<int>(face->glyph->advance.x >> 6);
        } else {
            width += std::max(6, renderSize);
        }
    }
    return width * renderScale(fontSize, fontPreset);
#endif
}

int SDLRenderer::lineHeight(int fontSize, FontPreset fontPreset) const {
#ifndef NEXTREADING_NO_SDL_TTF
    const int hiDpiFontSize = fontSize * displayScale_;
    auto* self = const_cast<SDLRenderer*>(this);
    TTF_Font* font = self->fontForSize(hiDpiFontSize, fontPreset);
    return font != nullptr ? TTF_FontLineSkip(font) / displayScale_ : fontSize + 6;
#else
    FT_Face face = const_cast<SDLRenderer*>(this)->faceForPreset(fontPreset);
    if (face == nullptr) {
        return renderFontSize(fontSize, fontPreset) + 6;
    }
    const int renderSize = renderFontSize(fontSize, fontPreset);
    FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(renderSize));
    int height = static_cast<int>(face->size->metrics.height >> 6);
    return height * renderScale(fontSize, fontPreset);
#endif
}

bool SDLRenderer::saveScreenshot(const std::string& path) {
    if (renderer_ == nullptr || path.empty()) {
        return false;
    }

    int outputWidth = 0;
    int outputHeight = 0;
    if (SDL_GetRendererOutputSize(renderer_, &outputWidth, &outputHeight) != 0 ||
        outputWidth <= 0 || outputHeight <= 0) {
        return false;
    }

    SDL_Surface* surface =
        SDL_CreateRGBSurfaceWithFormat(0, outputWidth, outputHeight, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr) {
        return false;
    }

    const bool ok =
        SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) == 0 &&
        SDL_SaveBMP(surface, path.c_str()) == 0;
    SDL_FreeSurface(surface);
    return ok;
}

int SDLRenderer::screenWidth() const {
    return width_;
}

int SDLRenderer::screenHeight() const {
    return height_;
}

int SDLRenderer::fullScreenHeight() const {
    return fullHeight_;
}

int SDLRenderer::topInset() const {
    return topInset_;
}

#ifndef NEXTREADING_NO_SDL_TTF
TTF_Font* SDLRenderer::fontForSize(int fontSize, FontPreset fontPreset) {
    const int key = fontCacheKey(fontSize, fontPreset);
    auto it = fontCache_.find(key);
    if (it != fontCache_.end()) {
        return it->second;
    }

    const std::string fontPath = findFontPath(fontPreset);
    if (fontPath.empty()) {
        return nullptr;
    }

    TTF_Font* font = TTF_OpenFont(fontPath.c_str(), renderFontSize(fontSize, fontPreset));
    if (font == nullptr) {
        return nullptr;
    }

    if (fontPreset == FontPreset::Pixel) {
        TTF_SetFontHinting(font, TTF_HINTING_MONO);
        TTF_SetFontKerning(font, 0);
    }

    fontCache_[key] = font;
    return font;
}
#endif

std::string SDLRenderer::findFontPath(FontPreset fontPreset) const {
    const char* assetsRoot = std::getenv("NEXTREADING_ASSETS_PATH");

    const std::array<std::string, 14> normalCandidates{
        "",
        assetsRoot != nullptr ? std::string(assetsRoot) + "/fonts/ui.ttf" : "",
        assetsRoot != nullptr ? std::string(assetsRoot) + "/fonts/ui.ttc" : "",
        assetsRoot != nullptr ? std::string(assetsRoot) + "/fonts/NotoSansCJK-Regular.ttc" : "",
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/LanguageSupport/PingFang.ttc",
        "/System/Library/Fonts/Helvetica.ttc",
        "/mnt/c/Windows/Fonts/msyh.ttc",
        "/mnt/c/Windows/Fonts/simhei.ttf",
        "/mnt/c/Windows/Fonts/msyhbd.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };

    const std::array<std::string, 14> pixelCandidates = normalCandidates;

    const std::array<std::string, 4> sansCandidates{
        "",
        assetsRoot != nullptr ? std::string(assetsRoot) + "/fonts/sans.ttf" : "",
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };

    if (fontPreset == FontPreset::Sans) {
        for (const std::string& candidate : sansCandidates) {
            if (!candidate.empty() && fs::exists(candidate)) {
                return candidate;
            }
        }
        // Fallback to normal if sans not found
    }

    const auto& candidates = fontPreset == FontPreset::Pixel ? pixelCandidates : normalCandidates;

    for (const std::string& candidate : candidates) {
        if (!candidate.empty() && fs::exists(candidate)) {
            return candidate;
        }
    }

    return {};
}

SDL_Color SDLRenderer::toSdlColor(const Color& color) const {
    return SDL_Color{color.r, color.g, color.b, color.a};
}

SDL_Rect SDLRenderer::toSdlRect(const Rect& rect) const {
    return SDL_Rect{rect.x, rect.y + topInset_, rect.w, rect.h};
}

#ifdef NEXTREADING_NO_SDL_TTF
FT_Face SDLRenderer::faceForPreset(FontPreset fontPreset) {
    if (ftLibrary_ == nullptr) {
        return nullptr;
    }

    FT_Face* targetFace = fontPreset == FontPreset::Pixel ? &pixelFace_ : &normalFace_;
    std::string* targetPath = fontPreset == FontPreset::Pixel ? &pixelFacePath_ : &normalFacePath_;
    const std::string path = findFontPath(fontPreset);
    if (path.empty()) {
        return nullptr;
    }

    if (*targetFace != nullptr && *targetPath == path) {
        return *targetFace;
    }

    if (*targetFace != nullptr) {
        FT_Done_Face(*targetFace);
        *targetFace = nullptr;
    }

    if (FT_New_Face(ftLibrary_, path.c_str(), 0, targetFace) != 0) {
        return nullptr;
    }

    *targetPath = path;
    return *targetFace;
}
#endif
