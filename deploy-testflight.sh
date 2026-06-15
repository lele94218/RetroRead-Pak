#!/bin/bash
# Headless TestFlight deploy: archive -> export .ipa -> upload to App Store Connect.
# Uses an App Store Connect API key (.p8) so it needs no Xcode login / GUI.
#
# One-time setup already done:
#   - API key at ~/.appstoreconnect/private_keys/AuthKey_<KEY_ID>.p8
#   - iOS deps built into ~/.retroread-ios-deps (tools/build_ios_deps.sh)
#   - App created in App Store Connect with bundle id com.terryx.retroread
#
# Usage:  ./deploy-testflight.sh
set -e
cd "$(dirname "$0")"

# --- Config (override via env) ---
TEAM_ID="${TEAM_ID:-UM249TS9JC}"
KEY_ID="${ASC_KEY_ID:-799J6X3X8U}"
ISSUER_ID="${ASC_ISSUER_ID:-44a51cfb-d669-4bdb-97a8-688a583732b9}"
KEY_PATH="${ASC_KEY_PATH:-$HOME/.appstoreconnect/private_keys/AuthKey_$KEY_ID.p8}"
DEPS="${IOS_DEPS_PREFIX:-$HOME/.retroread-ios-deps}"
INFO_PLIST="ports/ios/Info.plist"
ARCHIVE="build-ios/RetroRead.xcarchive"
EXPORT_DIR="build-ios/export"

AUTH=(-allowProvisioningUpdates \
      -authenticationKeyPath "$KEY_PATH" \
      -authenticationKeyID "$KEY_ID" \
      -authenticationKeyIssuerID "$ISSUER_ID")

# --- 1. Bump build number (TestFlight requires it to increase each upload) ---
CUR=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "$INFO_PLIST")
NEXT=$((CUR + 1))
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $NEXT" "$INFO_PLIST"
echo "=== Build number: $CUR -> $NEXT ==="

# --- 2. (Re)generate Xcode project pointing at the prebuilt iOS deps ---
echo "=== Configuring Xcode project ==="
rm -rf build-ios
DEVELOPMENT_TEAM="$TEAM_ID" cmake -B build-ios -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
    -DCMAKE_PREFIX_PATH="$DEPS" \
    -DSDL2_DIR="$DEPS/lib/cmake/SDL2" \
    -DSDL2_ttf_DIR="$DEPS/lib/cmake/SDL2_ttf" \
    -Dlibzip_DIR="$DEPS/lib/cmake/libzip" >/dev/null

# --- 3. Archive (Release) ---
echo "=== Archiving (this is the slow part) ==="
xcodebuild -project build-ios/RetroRead.xcodeproj \
    -scheme RetroRead -configuration Release \
    -destination 'generic/platform=iOS' \
    -archivePath "$ARCHIVE" \
    "${AUTH[@]}" \
    archive

# --- 4. Export .ipa ---
# Manual signing via ExportOptions.plist using the locally-installed
# "Apple Distribution" cert + "RetroRead AppStore" profile.
# (Cloud signing via App Manager API key is NOT permitted, so we manage
#  the distribution cert + profile ourselves. See tools/setup_ios_signing.sh.)
echo "=== Exporting .ipa ==="
rm -rf "$EXPORT_DIR"
xcodebuild -exportArchive \
    -archivePath "$ARCHIVE" \
    -exportPath "$EXPORT_DIR" \
    -exportOptionsPlist ExportOptions.plist

# --- 5. Upload to TestFlight ---
IPA=$(ls "$EXPORT_DIR"/*.ipa | head -1)
echo "=== Uploading $IPA to TestFlight ==="
xcrun altool --upload-app -f "$IPA" -t ios \
    --apiKey "$KEY_ID" --apiIssuer "$ISSUER_ID"

echo ""
echo "=== Uploaded build $NEXT. Processing takes ~5-15 min in App Store Connect. ==="
echo "Check: https://appstoreconnect.apple.com -> RetroRead -> TestFlight"
