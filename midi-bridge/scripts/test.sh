#!/bin/bash
set -euo pipefail
bridge_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$bridge_root"
export CLANG_MODULE_CACHE_PATH="$bridge_root/.build/clang-cache"
export SWIFTPM_MODULECACHE_OVERRIDE="$bridge_root/.build/swift-cache"
swift test --disable-sandbox --cache-path "$bridge_root/.build/package-cache"
