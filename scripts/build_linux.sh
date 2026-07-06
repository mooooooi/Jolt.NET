#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
NATIVE_DIR="$ROOT_DIR/native"
BIN_DIR="$ROOT_DIR/bin"

echo "=========================================="
echo " Jolt.NET - Linux Build (x64 + arm64)"
echo "=========================================="
echo "Root:   $ROOT_DIR"
echo "Native: $NATIVE_DIR"
echo ""
echo "Prerequisites:"
echo "  - cmake, ninja-build"
echo "  - gcc-aarch64-linux-gnu, g++-aarch64-linux-gnu (for arm64 cross-compile)"
echo "  Install: sudo apt-get install -y cmake ninja-build gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
echo ""

BUILD_X64_DISTRIBUTION="$ROOT_DIR/build_linux_x64_Distribution"
BUILD_X64_DEBUG="$ROOT_DIR/build_linux_x64_Debug"
BUILD_ARM64_DISTRIBUTION="$ROOT_DIR/build_linux_arm64_Distribution"
BUILD_ARM64_DEBUG="$ROOT_DIR/build_linux_arm64_Debug"

# --- linux-x64 Distribution ---
echo "[1/4] Configure linux-x64 (Distribution)..."
cmake -S "$NATIVE_DIR" -B "$BUILD_X64_DISTRIBUTION" -G Ninja \
    -DCMAKE_BUILD_TYPE=Distribution \
    -DCROSS_PLATFORM_DETERMINISTIC=ON

echo "[1/4] Build linux-x64 (Distribution)..."
cmake --build "$BUILD_X64_DISTRIBUTION" --config Distribution --verbose --parallel

# --- linux-arm64 Distribution ---
echo "[2/4] Configure linux-arm64 (Distribution)..."
cmake -S "$NATIVE_DIR" -B "$BUILD_ARM64_DISTRIBUTION" -G Ninja \
    -DCMAKE_BUILD_TYPE=Distribution \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCROSS_PLATFORM_DETERMINISTIC=ON

echo "[2/4] Build linux-arm64 (Distribution)..."
cmake --build "$BUILD_ARM64_DISTRIBUTION" --config Distribution --verbose --parallel

# --- linux-x64 Debug ---
echo "[3/4] Configure linux-x64 (Debug)..."
cmake -S "$NATIVE_DIR" -B "$BUILD_X64_DEBUG" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCROSS_PLATFORM_DETERMINISTIC=ON \
    -DGENERATE_DEBUG_SYMBOLS=ON

echo "[3/4] Build linux-x64 (Debug)..."
cmake --build "$BUILD_X64_DEBUG" --config Debug --verbose --parallel

# --- linux-arm64 Debug ---
echo "[4/4] Configure linux-arm64 (Debug)..."
cmake -S "$NATIVE_DIR" -B "$BUILD_ARM64_DEBUG" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCROSS_PLATFORM_DETERMINISTIC=ON \
    -DGENERATE_DEBUG_SYMBOLS=ON

echo "[4/4] Build linux-arm64 (Debug)..."
cmake --build "$BUILD_ARM64_DEBUG" --config Debug --verbose --parallel

# --- Package ---
echo "Packaging..."
mkdir -p "$BIN_DIR/linux-x64"
mkdir -p "$BIN_DIR/linux-arm64"

cp "$BUILD_X64_DISTRIBUTION/lib/libjoltcd.so" "$BIN_DIR/linux-x64/libjoltc.so"
cp "$BUILD_X64_DEBUG/lib/libjoltcd.so" "$BIN_DIR/linux-x64/libjoltcd.so"
cp "$BUILD_ARM64_DISTRIBUTION/lib/libjoltcd.so" "$BIN_DIR/linux-arm64/libjoltc.so"
cp "$BUILD_ARM64_DEBUG/lib/libjoltcd.so" "$BIN_DIR/linux-arm64/libjoltcd.so"

echo ""
echo "Done! Output:"
ls -lh "$BIN_DIR/linux-x64"
ls -lh "$BIN_DIR/linux-arm64"
