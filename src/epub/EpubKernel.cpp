#include "epub/EpubKernel.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace {
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
}

bool EpubKernel::loadHtml(const std::string& html) {
    Chapter ch;
    extractFootnoteDefs(html, ch.footnoteDefs);
    extractContent(html, ch);
    if (ch.sentences.empty()) return false;
    chapters_.push_back(std::move(ch));
    return true;
}

std::size_t EpubKernel::chapterCount() const {
    return chapters_.size();
}

const EpubKernel::Chapter& EpubKernel::chapter(std::size_t index) const {
    return chapters_.at(index);
}

void EpubKernel::setChapter(std::size_t index) {
    currentChapter_ = index;
    currentSentence_ = 0;
}

void EpubKernel::setSentenceIndex(std::size_t index) {
    currentSentence_ = index;
}

void EpubKernel::setLinesPerPage(std::size_t lines) {
    linesPerPage_ = lines;
}

void EpubKernel::setCharsPerLine(std::size_t chars) {
    charsPerLine_ = chars;
}

PageContent EpubKernel::currentPage() const {
    PageContent page;
    if (currentChapter_ >= chapters_.size()) return page;
    const Chapter& ch = chapters_[currentChapter_];
    if (currentSentence_ >= ch.sentences.size()) return page;

    // Combine sentences to fill page
    std::string combined;
    std::size_t sentCount = 0;
    for (std::size_t s = currentSentence_; s < ch.sentences.size(); ++s) {
        std::string candidate = combined;
        if (!candidate.empty()) candidate += " ";
        candidate += ch.sentences[s];

        // Simple line count estimate
        std::size_t estimatedLines = (candidate.size() + charsPerLine_ - 1) / charsPerLine_;
        if (estimatedLines > linesPerPage_ && sentCount > 0) break;

        combined = std::move(candidate);
        ++sentCount;

        // Collect footnote IDs from this sentence
        if (s < ch.sentenceFootnoteIds.size()) {
            for (const std::string& fnId : ch.sentenceFootnoteIds[s]) {
                if (std::find(page.footnoteIds.begin(), page.footnoteIds.end(), fnId) == page.footnoteIds.end()) {
                    page.footnoteIds.push_back(fnId);
                }
            }
        }
    }

    // Wrap into lines
    std::size_t pos = 0;
    while (pos < combined.size()) {
        std::size_t lineEnd = std::min(pos + charsPerLine_, combined.size());
        page.lines.push_back(combined.substr(pos, lineEnd - pos));
        pos = lineEnd;
        if (page.lines.size() >= linesPerPage_) break;
    }

    return page;
}

bool EpubKernel::nextPage() {
    if (currentChapter_ >= chapters_.size()) return false;
    const Chapter& ch = chapters_[currentChapter_];
    if (currentSentence_ >= ch.sentences.size()) return false;

    // Advance past current page's sentences
    std::string combined;
    for (std::size_t s = currentSentence_; s < ch.sentences.size(); ++s) {
        std::string candidate = combined;
        if (!candidate.empty()) candidate += " ";
        candidate += ch.sentences[s];

        std::size_t estimatedLines = (candidate.size() + charsPerLine_ - 1) / charsPerLine_;
        if (estimatedLines > linesPerPage_ && s > currentSentence_) {
            currentSentence_ = s;
            return true;
        }
        combined = std::move(candidate);
    }

    currentSentence_ = ch.sentences.size();
    return false;
}

bool EpubKernel::prevPage() {
    if (currentSentence_ == 0) return false;
    --currentSentence_;
    return true;
}

void EpubKernel::extractFootnoteDefs(
    const std::string& html,
    std::unordered_map<std::string, std::string>& defs) const {
    const std::string lower = toLower(html);

    std::string::size_type pos = 0;
    while (pos < lower.size()) {
        // Find <p class="note"> elements
        std::string::size_type pPos = lower.find("<p ", pos);
        if (pPos == std::string::npos) break;

        std::string::size_type tagEnd = html.find('>', pPos);
        if (tagEnd == std::string::npos) break;

        std::string tag = lower.substr(pPos, tagEnd - pPos + 1);
        if (tag.find("class=\"note\"") == std::string::npos) {
            pos = tagEnd + 1;
            continue;
        }

        std::string::size_type closePos = lower.find("</p>", tagEnd);
        if (closePos == std::string::npos) { pos = tagEnd + 1; continue; }

        std::string inner = html.substr(tagEnd + 1, closePos - tagEnd - 1);

        // Find <a id="mN"> inside
        std::string::size_type idPos = inner.find("id=\"");
        if (idPos != std::string::npos) {
            std::string::size_type idEnd = inner.find('"', idPos + 4);
            if (idEnd != std::string::npos) {
                std::string id = inner.substr(idPos + 4, idEnd - idPos - 4);
                std::string text = trim(stripTags(inner));
                // Remove leading [N] or 〔N〕
                if (!text.empty()) {
                    auto rb = text.find(']');
                    if (rb != std::string::npos && rb < 10 && text[0] == '[') {
                        text = trim(text.substr(rb + 1));
                    }
                    auto rb2 = text.find("\xe3\x80\x95"); // 〕
                    if (rb2 != std::string::npos && rb2 < 15) {
                        text = trim(text.substr(rb2 + 3));
                    }
                }
                if (!id.empty() && !text.empty()) {
                    defs[id] = text;
                }
            }
        }
        pos = closePos + 4;
    }
}

void EpubKernel::extractContent(const std::string& html, Chapter& chapter) const {
    const std::string lower = toLower(html);
    std::size_t cursor = 0;

    while (cursor < lower.size()) {
        // Find next <p> or <h1>-<h3>
        std::string::size_type bestPos = std::string::npos;
        std::string bestTag;
        for (const char* tag : {"p", "h1", "h2", "h3"}) {
            std::string needle = std::string("<") + tag;
            auto pos = lower.find(needle, cursor);
            if (pos != std::string::npos && (bestPos == std::string::npos || pos < bestPos)) {
                bestPos = pos;
                bestTag = tag;
            }
        }
        if (bestPos == std::string::npos) break;

        auto tagEnd = html.find('>', bestPos);
        if (tagEnd == std::string::npos) break;

        // Skip class="note" paragraphs
        std::string openTag = lower.substr(bestPos, tagEnd - bestPos + 1);
        if (openTag.find("class=\"note\"") != std::string::npos) {
            cursor = tagEnd + 1;
            continue;
        }

        std::string closeNeedle = "</" + bestTag + ">";
        auto closePos = lower.find(closeNeedle, tagEnd);
        if (closePos == std::string::npos) { cursor = tagEnd + 1; continue; }

        std::string rawInner = html.substr(tagEnd + 1, closePos - tagEnd - 1);

        // Extract footnote IDs from <a href="...#mN"> links in this paragraph
        std::vector<std::string> fnIds;
        {
            std::regex linkRe(R"RE(href="[^"]*#(m\d+)")RE");
            auto begin2 = std::sregex_iterator(rawInner.begin(), rawInner.end(), linkRe);
            auto end2 = std::sregex_iterator();
            for (auto it = begin2; it != end2; ++it) {
                std::string id = (*it)[1].str();
                if (chapter.footnoteDefs.count(id)) {
                    fnIds.push_back(id);
                }
            }
        }

        std::string text = trim(stripTags(rawInner));
        if (!text.empty()) {
            // Set chapter title from first heading
            if (chapter.title.empty() && (bestTag == "h1" || bestTag == "h2" || bestTag == "h3")) {
                chapter.title = text;
            }
            chapter.sentences.push_back(text);
            chapter.sentenceFootnoteIds.push_back(fnIds);
        }

        cursor = closePos + closeNeedle.size();
    }
}

std::string EpubKernel::stripTags(const std::string& html) const {
    std::string out;
    out.reserve(html.size());
    bool inTag = false;
    for (char ch : html) {
        if (ch == '<') { inTag = true; out += ' '; continue; }
        if (ch == '>') { inTag = false; continue; }
        if (!inTag) out += ch;
    }
    return out;
}

std::string EpubKernel::trim(const std::string& text) const {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return (begin < end) ? text.substr(begin, end - begin) : std::string{};
}
