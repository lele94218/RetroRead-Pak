#include "ui/LoadingScene.h"

#include <memory>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <regex>
#include <unordered_map>
#include <utility>
#include <SDL.h>

#include "app/Application.h"
#include "core/ProgressStore.h"
#include "epub/EpubArchive.h"
#include "epub/EpubCompiler.h"
#include "epub/EpubKernel.h"
#include "platform/Input.h"
#include "platform/Renderer.h"
#include "txt/TxtCompiler.h"
#include "ui/BookListScene.h"
#include "ui/ChapterScene.h"
#include "ui/ReaderScene.h"
#include "ui/ThemePalette.h"

namespace {
bool hasExtension(const std::string& path, const char* extension) {
    const std::filesystem::path fsPath(path);
    std::string ext = fsPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext == extension;
}

void enrichWithFootnotes(const std::string& epubPath, BookScript& book) {
    if (book.chapters.empty()) return;

    EpubArchive archive;
    if (!archive.open(epubPath)) return;

    // Read all HTML files from the EPUB and run EpubKernel for footnote extraction
    // We need to match kernel chapters back to BookScript chapters by title
    EpubKernel kernel;

    // Parse the OPF to get spine order (simplified: read all xhtml from manifest)
    std::string containerXml;
    if (!archive.readTextFile("META-INF/container.xml", containerXml)) return;

    // Find rootfile path
    auto rfPos = containerXml.find("full-path=\"");
    if (rfPos == std::string::npos) return;
    rfPos += 11;
    auto rfEnd = containerXml.find('"', rfPos);
    if (rfEnd == std::string::npos) return;
    const std::string opfPath = containerXml.substr(rfPos, rfEnd - rfPos);

    std::string opfXml;
    if (!archive.readTextFile(opfPath, opfXml)) return;

    // Find all href="...xhtml" or href="...html" in manifest
    std::string opfDir;
    auto lastSlash = opfPath.rfind('/');
    if (lastSlash != std::string::npos) opfDir = opfPath.substr(0, lastSlash + 1);

    std::regex hrefRe(R"RE(href="([^"]+\.x?html?)")RE", std::regex::icase);
    auto begin = std::sregex_iterator(opfXml.begin(), opfXml.end(), hrefRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string htmlPath = opfDir + (*it)[1].str();
        std::string html;
        if (archive.readTextFile(htmlPath, html)) {
            kernel.loadHtml(html);
        }
    }

    // Match kernel chapters to BookScript chapters by sentence text overlap
    for (std::size_t bi = 0; bi < book.chapters.size(); ++bi) {
        Chapter& bookCh = book.chapters[bi];
        if (bookCh.sentences.empty()) continue;

        const std::string& firstSent = bookCh.sentences[0].text;
        for (std::size_t ki = 0; ki < kernel.chapterCount(); ++ki) {
            const EpubKernel::Chapter& kCh = kernel.chapter(ki);
            if (kCh.sentences.empty()) continue;

            // Check if first sentence of BookScript chapter appears in kernel chapter
            bool matched = false;
            for (std::size_t ks = 0; ks < kCh.sentences.size(); ++ks) {
                if (kCh.sentences[ks].find(firstSent.substr(0, std::min<std::size_t>(30, firstSent.size()))) != std::string::npos) {
                    matched = true;
                    break;
                }
            }
            if (!matched) continue;

            bookCh.footnoteDefs = kCh.footnoteDefs;

            // For each BookScript sentence, find which kernel paragraph contains it,
            // then check which specific footnote markers ([N] or 〔X〕) are in that sentence
            bookCh.sentenceFootnoteIds.resize(bookCh.sentences.size());
            for (std::size_t si = 0; si < bookCh.sentences.size(); ++si) {
                const std::string& sentText = bookCh.sentences[si].text;
                // Check each footnote def ID: does this sentence contain a marker for it?
                for (const auto& [fnId, fnContent] : kCh.footnoteDefs) {
                    // Extract number from "m73" -> "73"
                    if (fnId.size() < 2 || fnId[0] != 'm') continue;
                    std::string num = fnId.substr(1);
                    // Check for [N] marker
                    if (sentText.find("[" + num + "]") != std::string::npos) {
                        bookCh.sentenceFootnoteIds[si].push_back(fnId);
                    }
                }
                // Also check for 〔X〕 style markers by matching kernel IDs
                for (std::size_t ks = 0; ks < kCh.sentenceFootnoteIds.size(); ++ks) {
                    for (const std::string& fnId : kCh.sentenceFootnoteIds[ks]) {
                        // Already assigned via [N]?
                        auto& assigned = bookCh.sentenceFootnoteIds[si];
                        if (std::find(assigned.begin(), assigned.end(), fnId) != assigned.end()) continue;
                        // Check if kernel sentence text around this ref overlaps with BookScript sentence
                        // Use a unique snippet from the kernel paragraph near the ref
                        const std::string& kText = kCh.sentences[ks];
                        // Find 〔 markers in both
                        if (sentText.find("\xe3\x80\x94") != std::string::npos) { // 〔
                            // Check if this kernel paragraph contains the BookScript sentence text
                            if (kText.find(sentText.substr(0, std::min<std::size_t>(15, sentText.size()))) != std::string::npos) {
                                assigned.push_back(fnId);
                            }
                        }
                    }
                }
            }
            break;
        }
    }
}

bool loadOrCompileBook(Application& app, const std::string& path, BookScript& outBook) {
    if (app.bookCache().load(app.fileSystem(), path, outBook)) {
        return true;
    }

    if (hasExtension(path, ".txt")) {
        TxtCompiler compiler;
        if (!compiler.compile(path, outBook)) {
            return false;
        }
    } else {
        EpubCompiler compiler;
        if (!compiler.compile(path, outBook)) {
            return false;
        }
        enrichWithFootnotes(path, outBook);
    }

    app.bookCache().save(app.fileSystem(), outBook);
    return true;
}

bool loadChapterList(Application& app, const std::string& path, BookScript& outBook) {
    if (app.bookCache().load(app.fileSystem(), path, outBook)) {
        return true;
    }

    if (hasExtension(path, ".txt")) {
        TxtCompiler compiler;
        return compiler.compile(path, outBook);
    }

    EpubCompiler compiler;
    if (compiler.compileChapterList(path, outBook)) {
        return true;
    }

    return loadOrCompileBook(app, path, outBook);
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
}  // namespace

LoadingScene::LoadingScene(
    Application& app,
    std::string bookPath,
    std::string bookTitle,
    LoadingTarget target,
    int requestedChapterIndex)
    : AppScene(app),
      bookPath_(std::move(bookPath)),
      bookTitle_(std::move(bookTitle)),
      target_(target),
      requestedChapterIndex_(requestedChapterIndex) {
}

void LoadingScene::onEnter() {
    started_ = false;
    failed_ = false;
    errorMessage_.clear();
}

void LoadingScene::update(float dt) {
    (void)dt;

    if (failed_) {
        Input& input = app_.input();
        if (input.wasPressed(Action::Back) || input.wasPressed(Action::OpenMenu) || input.wasPressed(Action::Confirm)) {
            app_.sceneManager().replace(std::make_unique<BookListScene>(app_));
        }
        return;
    }

    if (started_) {
        return;
    }

    started_ = true;
    finishLoading();
}

void LoadingScene::finishLoading() {
    SDL_Log("RetroRead: loading book: %s", bookPath_.c_str());
    BookScript book;
    const bool loaded = target_ == LoadingTarget::Chapters ? loadChapterList(app_, bookPath_, book)
                                                           : loadOrCompileBook(app_, bookPath_, book);
    if (!loaded || book.chapters.empty()) {
        SDL_Log("RetroRead: load FAILED (loaded=%d chapters=%d)", loaded, static_cast<int>(book.chapters.size()));
        failed_ = true;
        errorMessage_ = "Unable to open this book.";
        return;
    }
    SDL_Log("RetroRead: loaded OK, %d chapters", static_cast<int>(book.chapters.size()));

    if (target_ == LoadingTarget::Chapters) {
        app_.sceneManager().replace(std::make_unique<ChapterScene>(app_, std::move(book)));
        return;
    }

    ReadingProgress progress{book.bookId, 0, 0, false, app_.settings().textSpeed};
    if (!app_.progressStore().get(book.bookId, progress)) {
        progress.bookId = book.bookId;
        progress.chapterIndex = 0;
        progress.sentenceIndex = 0;
    }
    if (requestedChapterIndex_ >= 0) {
        progress.chapterIndex = static_cast<std::uint32_t>(
            std::min<int>(requestedChapterIndex_, static_cast<int>(book.chapters.size()) - 1));
        progress.sentenceIndex = 0;
    }
    progress.autoPlay = false;
    progress.textSpeed = app_.settings().textSpeed;
    progress.lastOpenedAt = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    app_.progressStore().put(progress);
    app_.progressStore().save(app_.fileSystem());

    app_.sceneManager().replace(std::make_unique<ReaderScene>(app_, std::move(book), progress));
}

void LoadingScene::render(Renderer& renderer) {
    const int screenWidth = renderer.screenWidth();
    const int centerX = screenWidth / 2;
    const ThemePalette palette = themePalette(app_.settings().themePreset);

    renderer.clear(palette.screenBackground);

    renderer.drawText(
        failed_ ? "Open Failed" : "Preparing Book",
        Rect{40, uiSpacing(90, 160), screenWidth - 80, uiSpacing(42, 70)},
        palette.headerText,
        uiFont(28, 52),
        TextAlign::Center,
        app_.settings().fontPreset);

    renderer.drawText(
        bookTitle_,
        Rect{60, uiSpacing(150, 250), screenWidth - 120, uiSpacing(30, 48)},
        palette.primaryText,
        uiFont(18, 32),
        TextAlign::Center,
        app_.settings().fontPreset);

    if (failed_) {
        renderer.drawText(
            errorMessage_,
            Rect{60, uiSpacing(220, 340), screenWidth - 120, uiSpacing(28, 44)},
            palette.accentText,
            uiFont(18, 32),
            TextAlign::Center,
            app_.settings().fontPreset);
        renderer.drawText(
            "A or Menu: back",
            Rect{60, uiSpacing(270, 410), screenWidth - 120, uiSpacing(24, 36)},
            palette.secondaryText,
            uiFont(16, 28),
            TextAlign::Center,
            app_.settings().fontPreset);
        return;
    }

    renderer.fillRect(Rect{centerX - uiSpacing(110, 180), uiSpacing(230, 350), uiSpacing(220, 360), uiSpacing(14, 20)},
                      palette.dialogueInnerBorder);
    renderer.fillRect(Rect{centerX - uiSpacing(110, 180), uiSpacing(230, 350), uiSpacing(150, 240), uiSpacing(14, 20)},
                      palette.dialogueBorder);

    renderer.drawText(
        target_ == LoadingTarget::Reader ? "Building cache and loading your last position..."
                                         : "Building cache and opening the chapter list...",
        Rect{60, uiSpacing(270, 400), screenWidth - 120, uiSpacing(28, 44)},
        palette.secondaryText,
        uiFont(17, 28),
        TextAlign::Center,
        app_.settings().fontPreset);
}
