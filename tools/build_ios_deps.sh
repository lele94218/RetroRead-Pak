#!/bin/bash
# Cross-compile SDL2, SDL2_ttf, libzip as static libs for iOS (arm64).
# Installs into a PERSISTENT prefix so a /tmp wipe / reboot never loses them.
#
# Usage:  tools/build_ios_deps.sh
# Output: $PREFIX/lib/*.a  +  $PREFIX/lib/cmake/{SDL2,SDL2_ttf,libzip}
#
# After this, configure the app with:
#   cmake -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
#       -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
#       -DCMAKE_PREFIX_PATH="$PREFIX" \
#       -DSDL2_DIR="$PREFIX/lib/cmake/SDL2" \
#       -DSDL2_ttf_DIR="$PREFIX/lib/cmake/SDL2_ttf" \
#       -Dlibzip_DIR="$PREFIX/lib/cmake/libzip"
set -e

PREFIX="${IOS_DEPS_PREFIX:-$HOME/.retroread-ios-deps}"
WORK="${IOS_DEPS_WORK:-$HOME/.cache/retroread-ios-deps-src}"
SDL2_VER=2.32.10
SDL2_TTF_VER=2.24.0
LIBZIP_VER=1.11.3
NPROC=$(sysctl -n hw.ncpu)

mkdir -p "$PREFIX" "$WORK"
cd "$WORK"

echo "=== [1/4] Downloading sources ==="
[ -f "SDL2-$SDL2_VER.tar.gz" ] || gh release download "release-$SDL2_VER" -R libsdl-org/SDL -p "SDL2-$SDL2_VER.tar.gz" -D "$WORK"
[ -f "SDL2_ttf-$SDL2_TTF_VER.tar.gz" ] || gh release download "release-$SDL2_TTF_VER" -R libsdl-org/SDL_ttf -p "SDL2_ttf-$SDL2_TTF_VER.tar.gz" -D "$WORK"
[ -f "libzip-$LIBZIP_VER.tar.gz" ] || curl -sL "https://github.com/nih-at/libzip/releases/download/v$LIBZIP_VER/libzip-$LIBZIP_VER.tar.gz" -o "libzip-$LIBZIP_VER.tar.gz"
[ -d "SDL2-$SDL2_VER" ] || tar xzf "SDL2-$SDL2_VER.tar.gz"
[ -d "SDL2_ttf-$SDL2_TTF_VER" ] || tar xzf "SDL2_ttf-$SDL2_TTF_VER.tar.gz"
[ -d "libzip-$LIBZIP_VER" ] || tar xzf "libzip-$LIBZIP_VER.tar.gz"

echo "=== [2/4] Building SDL2 ==="
cmake -S "$WORK/SDL2-$SDL2_VER" -B "$WORK/build-sdl2-ios" \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$WORK/build-sdl2-ios" --config Release -j"$NPROC" >/dev/null
cmake --install "$WORK/build-sdl2-ios" --config Release >/dev/null

echo "=== [3/4] Building SDL2_ttf (vendored freetype) ==="
cmake -S "$WORK/SDL2_ttf-$SDL2_TTF_VER" -B "$WORK/build-sdl2ttf-ios" \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" -DSDL2_DIR="$PREFIX/lib/cmake/SDL2" \
    -DSDL2TTF_SAMPLES=OFF -DSDL2TTF_VENDORED=ON -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 >/dev/null
cmake --build "$WORK/build-sdl2ttf-ios" --config Release -j"$NPROC" >/dev/null
cmake --install "$WORK/build-sdl2ttf-ios" --config Release >/dev/null

echo "=== [4/4] Building libzip ==="
cmake -S "$WORK/libzip-$LIBZIP_VER" -B "$WORK/build-libzip-ios" \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_SHARED_LIBS=OFF -DBUILD_TOOLS=OFF -DBUILD_REGRESS=OFF \
    -DBUILD_EXAMPLES=OFF -DBUILD_DOC=OFF -DENABLE_BZIP2=OFF \
    -DENABLE_LZMA=OFF -DENABLE_ZSTD=OFF \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 >/dev/null
cmake --build "$WORK/build-libzip-ios" --config Release -j"$NPROC" >/dev/null
cmake --install "$WORK/build-libzip-ios" --config Release >/dev/null

echo "=== Done. Installed libs: ==="
ls "$PREFIX/lib/"*.a
echo "PREFIX=$PREFIX"
