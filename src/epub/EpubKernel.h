#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct PageContent {
    std::vector<std::string> lines;
    std::vector<std::string> footnoteIds;
};

struct FootnoteDef {
    std::string id;
    std::string content;
};

class EpubKernel {
public:
    struct Chapter {
        std::string title;
        std::vector<std::string> sentences;
        std::vector<std::vector<std::string>> sentenceFootnoteIds;
        std::unordered_map<std::string, std::string> footnoteDefs;
    };

    bool loadHtml(const std::string& html);

    std::size_t chapterCount() const;
    const Chapter& chapter(std::size_t index) const;

    void setChapter(std::size_t index);
    void setSentenceIndex(std::size_t index);
    void setLinesPerPage(std::size_t lines);
    void setCharsPerLine(std::size_t chars);

    PageContent currentPage() const;
    bool nextPage();
    bool prevPage();

private:
    void extractFootnoteDefs(const std::string& html,
                             std::unordered_map<std::string, std::string>& defs) const;
    void extractContent(const std::string& html, Chapter& chapter) const;
    std::string stripTags(const std::string& html) const;
    std::string trim(const std::string& text) const;

    std::vector<Chapter> chapters_;
    std::size_t currentChapter_ = 0;
    std::size_t currentSentence_ = 0;
    std::size_t linesPerPage_ = 4;
    std::size_t charsPerLine_ = 40;
};
