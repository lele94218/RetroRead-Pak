#pragma once

#include <string>
#include <vector>

#include "core/BookTypes.h"
#include "platform/Renderer.h"

class DialogueBox {
public:
    void setBounds(const Rect& rect);
    void setTitle(const std::string& title);
    void setBodyView(
        const std::vector<std::string>* lines,
        const std::vector<int>* lineWidths,
        const std::vector<int>* revealWidths,
        std::size_t begin,
        std::size_t count);
    void setHint(const std::string& hint);
    void setFootnoteLines(const std::vector<std::string>& lines);
    void render(Renderer& renderer, const ReaderSettings& settings);
    void advanceFrame() { ++scrambleFrame_; }
    const Rect& bounds() const { return bounds_; }

private:
    Rect bounds_{40, 420, 1200, 240};
    std::string title_;
    const std::vector<std::string>* bodyLines_ = nullptr;
    const std::vector<int>* bodyLineWidths_ = nullptr;
    const std::vector<int>* bodyRevealWidths_ = nullptr;
    std::size_t bodyBegin_ = 0;
    std::size_t bodyCount_ = 0;
    std::string hint_;
    std::vector<std::string> footnoteLines_;
    std::uint32_t scrambleFrame_ = 0;
};
