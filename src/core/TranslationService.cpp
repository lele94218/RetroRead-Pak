#include "core/TranslationService.h"
#include "core/BookTypes.h"

#include <sstream>

#include "platform/FileSystem.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IOS
extern void iosClaudeTranslateStart(const char* text, const char* apiKey);
extern bool iosClaudeTranslateReady();
extern bool iosClaudeTranslateError();
extern std::string iosClaudeTranslateResult();
extern void iosClaudeTranslateConsume();
extern void iosGeminiTranslateStart(const char* text, const char* apiKey);
extern bool iosGeminiTranslateReady();
extern bool iosGeminiTranslateError();
extern std::string iosGeminiTranslateResult();
extern void iosGeminiTranslateConsume();
#endif
#endif

namespace {
std::string escapeField(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
        if (ch == '\\') out += "\\\\";
        else if (ch == '\t') out += "\\t";
        else if (ch == '\n') out += "\\n";
        else out += ch;
    }
    return out;
}

std::string unescapeField(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            char next = value[i + 1];
            if (next == '\\') { out += '\\'; ++i; }
            else if (next == 't') { out += '\t'; ++i; }
            else if (next == 'n') { out += '\n'; ++i; }
            else out += value[i];
        } else {
            out += value[i];
        }
    }
    return out;
}
}

void TranslationService::requestTranslation(const std::string& sourceText, const std::string& apiKey, TranslationProvider provider) {
    if (sourceText.empty() || apiKey.empty()) {
        state_ = TranslationState::Idle;
        return;
    }

    auto it = cache_.find(sourceText);
    if (it != cache_.end()) {
        translatedText_ = it->second;
        pendingSource_ = sourceText;
        state_ = TranslationState::Ready;
        return;
    }

    pendingSource_ = sourceText;
    translatedText_.clear();
    activeProvider_ = provider;
    state_ = TranslationState::Loading;

#if defined(__APPLE__) && TARGET_OS_IOS
    if (provider == TranslationProvider::Gemini) {
        iosGeminiTranslateStart(sourceText.c_str(), apiKey.c_str());
    } else {
        iosClaudeTranslateStart(sourceText.c_str(), apiKey.c_str());
    }
#else
    state_ = TranslationState::Error;
#endif
}

void TranslationService::update() {
    if (state_ != TranslationState::Loading) return;

#if defined(__APPLE__) && TARGET_OS_IOS
    bool ready = false;
    bool error = false;
    if (activeProvider_ == TranslationProvider::Gemini) {
        ready = iosGeminiTranslateReady();
        error = ready && iosGeminiTranslateError();
    } else {
        ready = iosClaudeTranslateReady();
        error = ready && iosClaudeTranslateError();
    }

    if (!ready) return;

    if (error) {
        state_ = TranslationState::Error;
        if (activeProvider_ == TranslationProvider::Gemini) iosGeminiTranslateConsume();
        else iosClaudeTranslateConsume();
        return;
    }

    if (activeProvider_ == TranslationProvider::Gemini) {
        translatedText_ = iosGeminiTranslateResult();
        iosGeminiTranslateConsume();
    } else {
        translatedText_ = iosClaudeTranslateResult();
        iosClaudeTranslateConsume();
    }

    // Clean markdown, emoji, and unsupported Unicode characters
    {
        std::string clean;
        clean.reserve(translatedText_.size());
        for (std::size_t i = 0; i < translatedText_.size(); ++i) {
            unsigned char ch = static_cast<unsigned char>(translatedText_[i]);
            // Strip markdown
            if (ch == '*' || ch == '#') continue;
            // 4-byte UTF-8 (U+10000+): emoji and supplementary planes — skip
            if (ch >= 0xF0 && i + 3 < translatedText_.size()) {
                i += 3;
                continue;
            }
            // 3-byte UTF-8: check for problematic ranges
            if (ch >= 0xE0 && ch < 0xF0 && i + 2 < translatedText_.size()) {
                unsigned char b1 = static_cast<unsigned char>(translatedText_[i+1]);
                unsigned char b2 = static_cast<unsigned char>(translatedText_[i+2]);
                // Decode codepoint
                uint32_t cp = ((ch & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
                // U+2022 bullet → dash
                if (cp == 0x2022) { clean += "-"; i += 2; continue; }
                // U+2700-U+27BF dingbats, U+2600-U+26FF misc symbols — skip
                if (cp >= 0x2600 && cp <= 0x27BF) { i += 2; continue; }
                // U+FE00-U+FE0F variation selectors — skip
                if (cp >= 0xFE00 && cp <= 0xFE0F) { i += 2; continue; }
                // Keep everything else (CJK, punctuation, etc.)
                clean += translatedText_[i];
                clean += translatedText_[i+1];
                clean += translatedText_[i+2];
                i += 2;
                continue;
            }
            clean += translatedText_[i];
        }
        translatedText_ = std::move(clean);
    }

    if (!translatedText_.empty() && !pendingSource_.empty()) {
        cache_[pendingSource_] = translatedText_;
    }

    state_ = translatedText_.empty() ? TranslationState::Error : TranslationState::Ready;
#endif
}


void TranslationService::loadCache(FileSystem& fs) {
    std::string content;
    if (!fs.readTextFile(fs.savesPath() + "/translation_cache.db", content)) return;

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string source = unescapeField(line.substr(0, tab));
        std::string translated = unescapeField(line.substr(tab + 1));
        if (!source.empty() && !translated.empty()) {
            cache_[source] = translated;
        }
    }
}

void TranslationService::saveCache(FileSystem& fs) {
    std::ostringstream out;
    for (const auto& [source, translated] : cache_) {
        out << escapeField(source) << '\t' << escapeField(translated) << '\n';
    }
    fs.writeTextFile(fs.savesPath() + "/translation_cache.db", out.str());
}
