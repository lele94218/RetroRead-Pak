# RetroRead iOS Port

## Prerequisites

- macOS with Xcode 15+
- CMake 3.20+ (`brew install cmake`)
- SDL2 and SDL2_ttf iOS frameworks
- libzip for iOS

## Quick Start

### 1. Install dependencies

```bash
# CMake
brew install cmake

# SDL2 iOS frameworks — download from https://github.com/libsdl-org/SDL/releases
# Look for SDL2-2.x.x.dmg and SDL2_ttf-2.x.x.dmg
# Copy the .framework bundles:
mkdir -p ports/ios/Frameworks
cp -R /path/to/SDL2.framework ports/ios/Frameworks/
cp -R /path/to/SDL2_ttf.framework ports/ios/Frameworks/

# libzip via vcpkg
brew install vcpkg
vcpkg install libzip:arm64-ios
```

### 2. Generate Xcode project

```bash
# From the project root:
cmake -B build-ios -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=arm64-ios
```

If not using vcpkg for libzip, omit the toolchain flags:
```bash
cmake -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS
```

### 3. Build and run

```bash
# Open in Xcode
open build-ios/RetroRead.xcodeproj

# Or build from command line
cmake --build build-ios --config Release -- -sdk iphoneos
```

In Xcode:
1. Select your iOS device or simulator as the run target
2. Set your development team in Signing & Capabilities
3. Hit Run (Cmd+R)

## Touch Controls

| Gesture | Action |
|---|---|
| Tap right half | Next sentence |
| Tap left half | Back / Menu |
| Swipe right | Next page |
| Swipe left | Previous page |
| Swipe up | Settings |
| Swipe down | Chapters |

## Adding Books

Books can be added via:
- **Files app**: RetroRead's Documents/Books folder is visible in the iOS Files app (UIFileSharingEnabled is on)
- **"Open In"**: EPUB and TXT files can be opened directly from Safari, Mail, or other apps
- **iTunes/Finder file sharing**: Connect device and drag files into the RetroRead document folder

## Troubleshooting

**Font not found**: The app looks for PingFang.ttc (iOS system font). If text doesn't render, bundle a .ttf font in `pak/assets/fonts/ui.ttf`.

**No sound**: iOS may require an audio session category. If blips don't play, the SDL2 iOS backend usually handles this, but check that the device isn't in silent mode.

**Black screen on simulator**: The x86_64 simulator needs x86_64 frameworks. Use a real device or arm64 simulator (Apple Silicon Mac).
