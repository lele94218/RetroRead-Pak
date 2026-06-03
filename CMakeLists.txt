cmake_minimum_required(VERSION 3.20)
project(RetroRead LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Sources ---

set(RETROREAD_SOURCES
    src/app/main.cpp
    src/app/Application.cpp
    src/audio/TextBlipPlayer.cpp
    src/core/BookLibrary.cpp
    src/core/BookCache.cpp
    src/core/ProgressStore.cpp
    src/core/SettingsStore.cpp
    src/epub/EpubCompiler.cpp
    src/epub/EpubArchive.cpp
    src/epub/EpubKernel.cpp
    src/epub/HtmlTextExtractor.cpp
    src/platform/PlatformFactory.cpp
    src/platform/sdl/SDLRenderer.cpp
    src/platform/sdl/SDLInput.cpp
    src/platform/sdl/SDLFileSystem.cpp
    src/platform/sdl/SDLClock.cpp
    src/text/SentenceSplitter.cpp
    src/text/TextTyper.cpp
    src/text/Utf8.cpp
    src/txt/TxtCompiler.cpp
    src/ui/SceneManager.cpp
    src/ui/BookListScene.cpp
    src/ui/ChapterScene.cpp
    src/ui/LoadingScene.cpp
    src/ui/SettingsScene.cpp
    src/ui/ReaderScene.cpp
    src/ui/widgets/DialogueBox.cpp
    src/ui/VirtualButtons.cpp
)

# NextUI sources are only for TrimUI hardware — always excluded from iOS/desktop CMake builds.
# They depend on NextUI SDK headers not available outside that toolchain.

# --- Platform-specific sources ---

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(APPEND RETROREAD_SOURCES
        src/platform/ios/IOSBridge.mm
    )
endif()

# --- Target ---

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(RetroRead MACOSX_BUNDLE ${RETROREAD_SOURCES})
else()
    add_executable(RetroRead ${RETROREAD_SOURCES})
endif()

target_include_directories(RetroRead PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_compile_definitions(RetroRead PRIVATE RETROREAD_NO_NEXTUI)
endif()

# --- Find dependencies ---

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # iOS: use find_package with CMAKE_PREFIX_PATH pointing to the cross-compiled libs
    find_package(SDL2 REQUIRED CONFIG)
    target_link_libraries(RetroRead PRIVATE SDL2::SDL2-static SDL2::SDL2main)

    find_package(SDL2_ttf REQUIRED CONFIG)
    target_link_libraries(RetroRead PRIVATE SDL2_ttf::SDL2_ttf-static)

    find_package(libzip REQUIRED CONFIG)
    target_link_libraries(RetroRead PRIVATE libzip::zip)

    # FreeType is built as part of SDL2_ttf vendored build
    find_library(FREETYPE_LIB freetype PATHS "${CMAKE_PREFIX_PATH}/lib" NO_DEFAULT_PATH)
    if(FREETYPE_LIB)
        target_link_libraries(RetroRead PRIVATE ${FREETYPE_LIB})
    endif()

    # zlib (provided by iOS SDK)
    target_link_libraries(RetroRead PRIVATE z)

    # iOS system frameworks required by SDL2
    target_link_libraries(RetroRead PRIVATE
        "-framework Foundation"
        "-framework UIKit"
        "-framework AudioToolbox"
        "-framework AVFAudio"
        "-framework CoreAudio"
        "-framework CoreGraphics"
        "-framework CoreHaptics"
        "-framework CoreMotion"
        "-framework Metal"
        "-framework QuartzCore"
        "-framework OpenGLES"
        "-framework GameController"
        "-framework IOKit"
        "-framework CoreServices"
        "-framework UniformTypeIdentifiers"
        "-framework AVFoundation"
        "-framework MediaPlayer"
    )

    # iOS bundle configuration
    set_target_properties(RetroRead PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ports/ios/Info.plist"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.expkzb.retroread"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "14.0"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
    )

    # App icon via Asset Catalog
    set(XCASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ports/ios/Assets.xcassets")
    if(EXISTS "${XCASSETS_DIR}")
        target_sources(RetroRead PRIVATE "${XCASSETS_DIR}")
        set_source_files_properties("${XCASSETS_DIR}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
    endif()

    # Bundle the assets folder into the app
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pak/assets")
        file(GLOB_RECURSE ASSET_FILES "${CMAKE_CURRENT_SOURCE_DIR}/pak/assets/*")
        foreach(ASSET_FILE ${ASSET_FILES})
            file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/pak/assets" "${ASSET_FILE}")
            get_filename_component(REL_DIR "${REL_PATH}" DIRECTORY)
            set_source_files_properties("${ASSET_FILE}" PROPERTIES
                MACOSX_PACKAGE_LOCATION "assets/${REL_DIR}")
        endforeach()
        target_sources(RetroRead PRIVATE ${ASSET_FILES})
    endif()

else()
    # Desktop: use pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
    pkg_check_modules(LIBZIP REQUIRED libzip)

    target_include_directories(RetroRead PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_TTF_INCLUDE_DIRS}
        ${LIBZIP_INCLUDE_DIRS}
    )
    target_link_libraries(RetroRead PRIVATE
        ${SDL2_LIBRARIES}
        ${SDL2_TTF_LIBRARIES}
        ${LIBZIP_LIBRARIES}
    )
endif()
