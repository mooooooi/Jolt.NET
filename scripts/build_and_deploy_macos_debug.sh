#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
UNITY_MACOS_DIR="$ROOT_DIR/../project-Bubble/Project Bubble/Packages/mooooooi.jolt-physics/Jolt.Native/Release/macos"

echo "=========================================="
echo " Jolt.NET - macOS Debug Build & Deploy"
echo "=========================================="

# --- Step 1: Build ---
echo "[Step 1] Running macOS build..."
bash "$SCRIPT_DIR/build_macos.sh"

# --- Step 2: Deploy to Unity ---
BUILD_DIR="$ROOT_DIR/lib/joltc/build_osx/lib"

echo ""
echo "[Step 2] Deploying Debug files to Unity..."
echo "  From: $BUILD_DIR"
echo "  To:   $UNITY_MACOS_DIR"

mkdir -p "$UNITY_MACOS_DIR"

cp -f  "$BUILD_DIR/libjoltcd.dylib"  "$UNITY_MACOS_DIR/libjoltc.dylib"
rm -rf "$UNITY_MACOS_DIR/libjoltc.dylib.dSYM"
cp -R  "$BUILD_DIR/libjoltcd.dylib.dSYM" "$UNITY_MACOS_DIR/libjoltc.dylib.dSYM"

echo "  Copied libjoltcd.dylib      -> libjoltc.dylib"
echo "  Copied libjoltcd.dylib.dSYM -> libjoltc.dylib.dSYM"

# --- Step 3: Clear macOS quarantine & ad-hoc sign ---
echo ""
echo "[Step 3] Clearing macOS security restrictions..."

xattr -cr "$UNITY_MACOS_DIR/libjoltc.dylib"
xattr -cr "$UNITY_MACOS_DIR/libjoltc.dylib.dSYM"
echo "  Cleared quarantine attributes"

codesign --force --deep --sign - "$UNITY_MACOS_DIR/libjoltc.dylib"
codesign --force --deep --sign - "$UNITY_MACOS_DIR/libjoltc.dylib.dSYM"
echo "  Ad-hoc signed"

echo ""
echo "Done! Deployed files:"
ls -lh "$UNITY_MACOS_DIR/libjoltc.dylib"
ls -lhd "$UNITY_MACOS_DIR/libjoltc.dylib.dSYM"
