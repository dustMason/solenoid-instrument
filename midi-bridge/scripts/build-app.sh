#!/bin/bash
set -euo pipefail
bridge_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$bridge_root"
export CLANG_MODULE_CACHE_PATH="$bridge_root/.build/clang-cache"
export SWIFTPM_MODULECACHE_OVERRIDE="$bridge_root/.build/swift-cache"
swift build --disable-sandbox --cache-path "$bridge_root/.build/package-cache" -c release
bridge_bin="$(swift build --disable-sandbox --cache-path "$bridge_root/.build/package-cache" -c release --show-bin-path)"
bridge_app="$bridge_root/dist/MIDI Bridge.app"
mkdir -p "$bridge_app/Contents/MacOS" "$bridge_app/Contents/Resources"
cp "$bridge_bin/MIDIBridge" "$bridge_app/Contents/MacOS/MIDIBridge"
cat > "$bridge_app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>MIDIBridge</string>
<key>CFBundleIdentifier</key><string>com.dustmason.MIDIBridge</string>
<key>CFBundleName</key><string>MIDI Bridge</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>1.0</string>
<key>CFBundleVersion</key><string>1</string>
<key>LSMinimumSystemVersion</key><string>12.0</string>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
PLIST
codesign --force --sign - "$bridge_app"
codesign --verify --strict "$bridge_app"
ditto -c -k --sequesterRsrc --keepParent "$bridge_app" "$bridge_root/dist/MIDI Bridge.zip"
printf '%s\n' "$bridge_app"
