#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

class FileSystem;

enum class TranslationState {
    Idle,
    Loading,
    Ready,
    Error,
};

enum class TranslationProvider : std::uint8_t;

class TranslationService {
public:
    void requestTranslation(const std::string& sourceText, const std::string& apiKey, TranslationProvider provider);
    void update();

    TranslationState state() const { return state_; }
    const std::string& translatedText() const { return translatedText_; }
    const std::string& sourceText() const { return pendingSource_; }

    void loadCache(FileSystem& fs);
    void saveCache(FileSystem& fs);

private:
    TranslationState state_ = TranslationState::Idle;
    TranslationProvider activeProvider_{};
    std::string pendingSource_;
    std::string translatedText_;
    std::unordered_map<std::string, std::string> cache_;
};
