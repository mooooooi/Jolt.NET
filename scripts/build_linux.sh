#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
JOLTC_DIR="$ROOT_DIR/lib/joltc"
BIN_DIR="$ROOT_DIR/bin"

echo "=========================================="
echo " Jolt.NET - Linux Build (x64 + arm64)"
echo "=========================================="
echo "Root:  $ROOT_DIR"
echo "Joltc: $JOLTC_DIR"
echo ""
echo "Prerequisites:"
echo "  - cmake, ninja-build"
echo "  - gcc-aarch64-linux-gnu, g++-aarch64-linux-gnu (for arm64 cross-compile)"
echo "  Install: sudo apt-get install -y cmake ninja-build gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
echo ""

cd "$JOLTC_DIR"

# --- linux-x64 Distribution ---
echo "[1/4] Configure linux-x64 (Distribution)..."
cmake -S "." -B "build_linux_x64" -G Ninja \
    -DCMAKE_BUILD_TYPE=Distribution \
    -DCMAKE_INSTALL_PREFIX:String="SDK" \
    -DCROSS_PLATFORM_DETERMINISTIC=ON

echo "[1/4] Build linux-x64 (Distribution)..."
cmake --build build_linux_x64 --config Distribution --verbose --parallel

# --- linux-arm64 Distribution ---
echo "[2/4] Configure linux-arm64 (Distribution)..."
cmake -S "." -B "build_linux_arm64" -G Ninja \
    -DCMAKE_BUILD_TYPE=Distribution \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_INSTALL_PREFIX:String="SDK" \
    -DCROSS_PLATFORM_DETERMINISTIC=ON

echo "[2/4] Build linux-arm64 (Distribution)..."
cmake --build build_linux_arm64 --config Distribution --verbose --parallel

# --- linux-x64 Debug ---
echo "[3/4] Configure linux-x64 (Debug)..."
cmake -S "." -B "build_linux_x64" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX:String="SDK" \
    -DCROSS_PLATFORM_DETERMINISTIC=ON

echo "[3/4] Build linux-x64 (Debug)..."
cmake --build build_linux_x64 --config Debug --verbose --parallel

# --- linux-arm64 Debug ---
echo "[4/4] Configure linux-arm64 (Debug)..."
cmake -S "." -B "build_linux_arm64" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_INSTALL_PREFIX:String="SDK" \
    -DCROSS_PLATFORM_DETERMINISTIC=ON

echo "[4/4] Build linux-arm64 (Debug)..."
cmake --build build_linux_arm64 --config Debug --verbose --parallel

# --- Package ---
echo "Packaging..."
mkdir -p "$BIN_DIR/linux-x64"
mkdir -p "$BIN_DIR/linux-arm64"

cp build_linux_x64/lib/libjoltc.so   "$BIN_DIR/linux-x64/libjoltc.so"
cp build_linux_x64/lib/libjoltcd.so  "$BIN_DIR/linux-x64/libjoltcd.so"
cp build_linux_arm64/lib/libjoltc.so  "$BIN_DIR/linux-arm64/libjoltc.so"
cp build_linux_arm64/lib/libjoltcd.so "$BIN_DIR/linux-arm64/libjoltcd.so"

echo ""
echo "Done! Output:"
ls -lh "$BIN_DIR/linux-x64"
ls -lh "$BIN_DIR/linux-arm64"
