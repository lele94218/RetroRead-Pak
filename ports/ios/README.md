# RetroRead iOS Port

C++17 / SDL2 reader cross-compiled for iOS (arm64). Build, sign, and ship to
TestFlight entirely from the command line — no Xcode GUI required.

## Prerequisites

- macOS with Xcode 15+ (tested on Xcode 26.3) and the iOS SDK
- CMake 3.20+ (`brew install cmake`)
- `gh` CLI (authenticated) for downloading SDL sources
- Python 3 with `pyjwt` + `cryptography` (`pip3 install --break-system-packages pyjwt cryptography`)
  — only needed for the App Store Connect signing/upload helpers
- A paid Apple Developer account + an App Store Connect API key (App Manager role)

## 1. Build the native dependencies (one-time)

SDL2, SDL2_ttf, libzip and FreeType are cross-compiled as arm64 static libs
into a **persistent** prefix (`~/.retroread-ios-deps`) so a reboot / `/tmp`
wipe never loses them:

```bash
tools/build_ios_deps.sh
```

Re-run this only if the prefix is deleted or you bump a dependency version.

## 2. Generate the Xcode project

```bash
cmake -B build-ios -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
    -DCMAKE_PREFIX_PATH="$HOME/.retroread-ios-deps" \
    -DSDL2_DIR="$HOME/.retroread-ios-deps/lib/cmake/SDL2" \
    -DSDL2_ttf_DIR="$HOME/.retroread-ios-deps/lib/cmake/SDL2_ttf" \
    -Dlibzip_DIR="$HOME/.retroread-ios-deps/lib/cmake/libzip"
```

`build-ios/` is a throwaway directory (git-ignored) — regenerate any time.

## 3. Run on a tethered device (quick dev loop)

```bash
./deploy.sh          # xcodebuild + ios-deploy onto a connected iPhone
```

⚠️ A development signature **expires after ~7 days**, after which the app
stops launching. For a build that lasts, ship to TestFlight (below).

## 4. Ship to TestFlight (no expiry)

### One-time signing setup

App Store Connect API keys with the **App Manager** role cannot perform Xcode
"cloud signing". So we create the distribution certificate + App Store
provisioning profile ourselves through the API, import the cert into the login
keychain, and export with manual signing.

```bash
tools/setup_ios_signing.sh
```

Re-run this when the distribution certificate or profile expires (~1 year).
It reads the API key from `~/.appstoreconnect/private_keys/AuthKey_<KEY_ID>.p8`.

### Release a build

```bash
./deploy-testflight.sh
```

This bumps `CFBundleVersion`, regenerates the project, archives Release,
exports a signed `.ipa` (manual signing via `ExportOptions.plist`), and uploads
to TestFlight with `xcrun altool`. Build numbers must increase per upload —
the script handles that automatically.

Processing finishes in ~5–15 min. Export-compliance is declared in
`Info.plist` (`ITSAppUsesNonExemptEncryption = false`), so no per-build prompt.

### Configuration

Signing identifiers live in two places (no secrets in the repo — the `.p8`
key stays under `~/.appstoreconnect/`):

| Setting | Value | Where |
|---|---|---|
| Bundle ID | `com.terryx.retroread` | `CMakeLists.txt` |
| Team ID | `UM249TS9JC` | `deploy-testflight.sh`, `ExportOptions.plist` |
| API Key ID / Issuer | (App Manager key) | `deploy-testflight.sh` env defaults |

Override any of them with the `ASC_KEY_ID`, `ASC_ISSUER_ID`, `TEAM_ID` env vars.

## Touch Controls

| Gesture | Action |
|---|---|
| Tap right half | Next sentence |
| Tap left half | Back / Menu |
| Swipe right | Next page |
| Swipe left | Previous page |
| Swipe up | Settings |
| Swipe down | Chapters |

Plus an on-screen A/B/X/Y virtual gamepad, and volume-key page turning.

## Adding Books

- **Files app**: RetroRead's Documents folder is visible (UIFileSharingEnabled)
- **"Open In"**: EPUB and TXT files open directly from Safari, Mail, etc.
- **In-app importer**: the document picker (UIDocumentPickerViewController)

## Troubleshooting

- **App won't launch after a few days**: development signature expired — ship to
  TestFlight, or re-run `./deploy.sh`.
- **`cmake` can't find SDL2**: the deps prefix is gone — re-run `tools/build_ios_deps.sh`.
- **`exportArchive` "Cloud signing permission error"**: run `tools/setup_ios_signing.sh`
  to (re)create the distribution cert + profile.
- **`exportArchive` method `{}` error / empty archive**: ensure the target has
  `INSTALL_PATH=$(LOCAL_APPS_DIR)` and `SKIP_INSTALL=NO` (set in `CMakeLists.txt`).
