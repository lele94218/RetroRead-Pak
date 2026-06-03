#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include "epub/EpubKernel.h"

namespace {
std::string readFile(const std::string& path) {
    std::ifstream f(path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

const char* kTestDataDir = TESTDATA_DIR;
}

class EpubKernelTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string html10 = readFile(std::string(kTestDataDir) + "/part0010.html");
        std::string html11 = readFile(std::string(kTestDataDir) + "/part0011.html");
        std::string html12 = readFile(std::string(kTestDataDir) + "/part0012.html");
        ASSERT_FALSE(html10.empty());
        ASSERT_FALSE(html11.empty());
        ASSERT_FALSE(html12.empty());
        ASSERT_TRUE(kernel.loadHtml(html10));
        ASSERT_TRUE(kernel.loadHtml(html11));
        ASSERT_TRUE(kernel.loadHtml(html12));
    }

    EpubKernel kernel;
};

TEST_F(EpubKernelTest, LoadsThreeChapters) {
    EXPECT_EQ(kernel.chapterCount(), 3u);
}

TEST_F(EpubKernelTest, ChapterHasTitle) {
    EXPECT_FALSE(kernel.chapter(0).title.empty());
    EXPECT_FALSE(kernel.chapter(1).title.empty());
    EXPECT_FALSE(kernel.chapter(2).title.empty());
}

TEST_F(EpubKernelTest, ChapterHasSentences) {
    EXPECT_GT(kernel.chapter(0).sentences.size(), 10u);
    EXPECT_GT(kernel.chapter(1).sentences.size(), 10u);
    EXPECT_GT(kernel.chapter(2).sentences.size(), 10u);
}

TEST_F(EpubKernelTest, FootnoteDefsExist) {
    // Each chapter file has footnote definitions
    EXPECT_GT(kernel.chapter(0).footnoteDefs.size(), 5u);
    EXPECT_GT(kernel.chapter(1).footnoteDefs.size(), 5u);
    EXPECT_GT(kernel.chapter(2).footnoteDefs.size(), 5u);
}

TEST_F(EpubKernelTest, FootnoteDefContentNotEmpty) {
    const auto& defs = kernel.chapter(0).footnoteDefs;
    auto it = defs.find("m1");
    ASSERT_NE(it, defs.end());
    EXPECT_FALSE(it->second.empty());
    // m1 in part0010 is about 须眉
    EXPECT_NE(it->second.find("须眉"), std::string::npos);
}

TEST_F(EpubKernelTest, SentenceFootnoteIdsTracked) {
    const auto& ch = kernel.chapter(0);
    ASSERT_EQ(ch.sentenceFootnoteIds.size(), ch.sentences.size());

    // Find a sentence that has footnote refs
    bool foundRef = false;
    for (std::size_t i = 0; i < ch.sentenceFootnoteIds.size(); ++i) {
        if (!ch.sentenceFootnoteIds[i].empty()) {
            foundRef = true;
            // Each footnote ID should exist in the defs map
            for (const std::string& fnId : ch.sentenceFootnoteIds[i]) {
                EXPECT_TRUE(ch.footnoteDefs.count(fnId))
                    << "Footnote " << fnId << " referenced but not defined";
            }
            break;
        }
    }
    EXPECT_TRUE(foundRef) << "No sentences with footnote references found";
}

TEST_F(EpubKernelTest, NoteParasNotInSentences) {
    // Footnote definition text should NOT appear as a sentence
    const auto& ch = kernel.chapter(0);
    for (const std::string& sent : ch.sentences) {
        // Note paragraphs typically start with [N] and contain definition text
        // They should be filtered out. Check that no sentence starts with known def pattern.
        EXPECT_TRUE(sent.find("class=\"note\"") == std::string::npos)
            << "Raw HTML leaked into sentence: " << sent.substr(0, 80);
    }
}

TEST_F(EpubKernelTest, CurrentPageReturnsContent) {
    kernel.setChapter(0);
    kernel.setLinesPerPage(4);
    kernel.setCharsPerLine(40);

    PageContent page = kernel.currentPage();
    EXPECT_FALSE(page.lines.empty());
}

TEST_F(EpubKernelTest, NextPageAdvances) {
    kernel.setChapter(0);
    kernel.setLinesPerPage(4);
    kernel.setCharsPerLine(40);

    PageContent page1 = kernel.currentPage();
    ASSERT_TRUE(kernel.nextPage());
    PageContent page2 = kernel.currentPage();

    // Pages should be different
    EXPECT_NE(page1.lines, page2.lines);
}

TEST_F(EpubKernelTest, PageWithFootnotesReturnsIds) {
    // Navigate through chapter 0 until we find a page with footnotes
    kernel.setChapter(0);
    kernel.setLinesPerPage(4);
    kernel.setCharsPerLine(60);

    bool foundFootnote = false;
    for (int i = 0; i < 200; ++i) {
        PageContent page = kernel.currentPage();
        if (!page.footnoteIds.empty()) {
            foundFootnote = true;
            // Verify the footnote ID has a definition
            const auto& defs = kernel.chapter(0).footnoteDefs;
            for (const std::string& fnId : page.footnoteIds) {
                EXPECT_TRUE(defs.count(fnId))
                    << "Page has footnote " << fnId << " but no definition found";
            }
            break;
        }
        if (!kernel.nextPage()) break;
    }
    EXPECT_TRUE(foundFootnote) << "Never found a page with footnotes";
}

TEST_F(EpubKernelTest, FootnoteIdsArePerChapter) {
    // m1 in chapter 0 and m1 in chapter 2 should have different content
    const auto& defs0 = kernel.chapter(0).footnoteDefs;
    const auto& defs2 = kernel.chapter(2).footnoteDefs;

    auto it0 = defs0.find("m1");
    auto it2 = defs2.find("m1");
    ASSERT_NE(it0, defs0.end());
    ASSERT_NE(it2, defs2.end());
    EXPECT_NE(it0->second, it2->second)
        << "m1 should have different content in different chapters";
}

TEST_F(EpubKernelTest, ChineseAnnotationStyle) {
    // Chapter 1 (part0011) has 〔一〕 style annotations
    const auto& ch = kernel.chapter(1);
    // m2 in part0011 maps to a 〔一〕 style note
    auto it = ch.footnoteDefs.find("m2");
    ASSERT_NE(it, ch.footnoteDefs.end());
    // Content should be about 本回回目
    EXPECT_FALSE(it->second.empty());
}

TEST_F(EpubKernelTest, FootnoteNotDuplicated) {
    // A paragraph with multiple footnote refs should assign each to the correct sentence
    // part0012 has a paragraph with m72,m73,m74,m75,m76 - each should appear only once
    const auto& ch = kernel.chapter(2); // part0012

    // Count how many sentences have m73
    int m73Count = 0;
    for (const auto& fnIds : ch.sentenceFootnoteIds) {
        for (const std::string& id : fnIds) {
            if (id == "m73") ++m73Count;
        }
    }
    EXPECT_EQ(m73Count, 1) << "m73 should appear in exactly one sentence, not " << m73Count;
}
