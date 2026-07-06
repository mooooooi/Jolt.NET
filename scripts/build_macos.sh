#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
NATIVE_DIR="$ROOT_DIR/native"
BIN_DIR="$ROOT_DIR/bin/osx"
BUILD_DISTRIBUTION="$ROOT_DIR/build_osx_Distribution"
BUILD_DEBUG="$ROOT_DIR/build_osx_Debug"
BUILD_PARALLELISM="${JOLT_BUILD_PARALLELISM:-}"

if [[ -z "$BUILD_PARALLELISM" ]]; then
    if command -v sysctl >/dev/null 2>&1; then
        BUILD_PARALLELISM="$(sysctl -n hw.ncpu)"
    else
        BUILD_PARALLELISM=2
    fi
fi

echo "=========================================="
echo " Jolt.NET - macOS Build (Universal)"
echo "=========================================="
echo "Root:   $ROOT_DIR"
echo "Native: $NATIVE_DIR"
echo "Jobs:   $BUILD_PARALLELISM"
echo ""

# --- Distribution Build ---
echo "[1/2] Configure osx-universal (Distribution)..."
cmake -S "$NATIVE_DIR" -B "$BUILD_DISTRIBUTION" -G Ninja \
    -DCMAKE_BUILD_TYPE=Distribution \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCROSS_PLATFORM_DETERMINISTIC=ON

echo "[1/2] Build osx-universal (Distribution)..."
cmake --build "$BUILD_DISTRIBUTION" --config Distribution --verbose --parallel "$BUILD_PARALLELISM"

# --- Debug Build ---
echo "[2/2] Configure osx-universal (Debug)..."
cmake -S "$NATIVE_DIR" -B "$BUILD_DEBUG" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCROSS_PLATFORM_DETERMINISTIC=ON \
    -DGENERATE_DEBUG_SYMBOLS=ON

echo "[2/2] Build osx-universal (Debug)..."
cmake --build "$BUILD_DEBUG" --config Debug --verbose --parallel "$BUILD_PARALLELISM"

echo "Generate dSYM for Debug..."
dsymutil "$BUILD_DEBUG/lib/libjoltcd.dylib" -o "$BUILD_DEBUG/lib/libjoltcd.dylib.dSYM"

# --- Package ---
echo "Packaging..."
mkdir -p "$BIN_DIR"
cp "$BUILD_DISTRIBUTION/lib/libjoltcd.dylib" "$BIN_DIR/libjoltc.dylib"
cp "$BUILD_DEBUG/lib/libjoltcd.dylib" "$BIN_DIR/libjoltcd.dylib"
rm -rf "$BIN_DIR/libjoltcd.dylib.dSYM"
cp -R "$BUILD_DEBUG/lib/libjoltcd.dylib.dSYM" "$BIN_DIR/libjoltcd.dylib.dSYM"

echo ""
echo "Done! Output:"
ls -lh "$BIN_DIR"
