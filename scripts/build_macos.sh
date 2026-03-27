#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
JOLTC_DIR="$ROOT_DIR/lib/joltc"
BIN_DIR="$ROOT_DIR/bin/osx"

echo "=========================================="
echo " Jolt.NET - macOS Build (Universal)"
echo "=========================================="
echo "Root:  $ROOT_DIR"
echo "Joltc: $JOLTC_DIR"
echo ""

cd "$JOLTC_DIR"

# --- Distribution Build ---
echo "[1/2] Configure osx-universal (Distribution)..."
cmake -S "." -B "build_osx" -G Ninja \
    -DCMAKE_BUILD_TYPE=Distribution \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_INSTALL_PREFIX:String="SDK" \
    -DCROSS_PLATFORM_DETERMINISTIC=ON

echo "[1/2] Build osx-universal (Distribution)..."
cmake --build build_osx --config Distribution --verbose --parallel

# --- Debug Build ---
echo "[2/2] Configure osx-universal (Debug)..."
cmake -S "." -B "build_osx" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_INSTALL_PREFIX:String="SDK" \
    -DCROSS_PLATFORM_DETERMINISTIC=ON \
    -DGENERATE_DEBUG_SYMBOLS=ON

echo "[2/2] Build osx-universal (Debug)..."
cmake --build build_osx --config Debug --verbose --parallel

echo "Generate dSYM for Debug..."
dsymutil build_osx/lib/libjoltcd.dylib -o build_osx/lib/libjoltcd.dylib.dSYM

# --- Package ---
echo "Packaging..."
mkdir -p "$BIN_DIR"
cp build_osx/lib/libjoltc.dylib  "$BIN_DIR/libjoltc.dylib"
cp build_osx/lib/libjoltcd.dylib "$BIN_DIR/libjoltcd.dylib"
cp -R build_osx/lib/libjoltcd.dylib.dSYM "$BIN_DIR/libjoltcd.dylib.dSYM"

echo ""
echo "Done! Output:"
ls -lh "$BIN_DIR"
