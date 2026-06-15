#!/bin/bash
set -e

echo "=== Building ==="
xcodebuild -project build-ios/RetroRead.xcodeproj \
  -scheme RetroRead \
  -configuration Debug \
  -destination 'generic/platform=iOS' \
  -allowProvisioningUpdates \
  build 2>&1 | grep -E '^\*\*|error:'

echo "=== Deploying to iPhone ==="
ios-deploy --bundle build-ios/Debug-iphoneos/RetroRead.app
