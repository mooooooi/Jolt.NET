#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

TOOL_NAME="ClangSharpPInvokeGenerator"
TOOL_PACKAGE="clangsharppinvokegenerator"
TOOL_VERSION="20.1.2.1"
NATIVE_LIB_VERSION="20.1.2"

echo "=========================================="
echo " Jolt.NET - Generate C# Bindings"
echo "=========================================="
echo ""

# ---------------------------------------------------------------------------
# Step 1: Ensure ClangSharpPInvokeGenerator is installed
# ---------------------------------------------------------------------------
echo "[Step 1] Checking $TOOL_NAME..."

if dotnet tool list -g 2>/dev/null | grep -q "$TOOL_PACKAGE"; then
    INSTALLED_VER=$(dotnet tool list -g 2>/dev/null | grep "$TOOL_PACKAGE" | awk '{print $2}')
    echo "  Already installed (v$INSTALLED_VER)"
else
    echo "  Not found. Installing $TOOL_PACKAGE v$TOOL_VERSION..."
    dotnet tool install -g "$TOOL_PACKAGE" --version "$TOOL_VERSION"
    echo "  Installed."
fi

# ---------------------------------------------------------------------------
# Step 2: Locate the tool store directory
# ---------------------------------------------------------------------------
DOTNET_TOOLS_DIR="$HOME/.dotnet/tools"
TOOL_STORE_BASE="$DOTNET_TOOLS_DIR/.store/$TOOL_PACKAGE"

INSTALLED_VER=$(dotnet tool list -g 2>/dev/null | grep "$TOOL_PACKAGE" | awk '{print $2}')
TOOL_STORE="$TOOL_STORE_BASE/$INSTALLED_VER/$TOOL_PACKAGE/$INSTALLED_VER/tools/net8.0/any"

if [ ! -d "$TOOL_STORE" ]; then
    # net9.0 fallback
    TOOL_STORE="$TOOL_STORE_BASE/$INSTALLED_VER/$TOOL_PACKAGE/$INSTALLED_VER/tools/net9.0/any"
fi

if [ ! -d "$TOOL_STORE" ]; then
    echo "  ERROR: Cannot locate tool store directory."
    echo "  Searched: $TOOL_STORE_BASE/$INSTALLED_VER/..."
    exit 1
fi

echo "  Tool store: $TOOL_STORE"

# ---------------------------------------------------------------------------
# Step 3: Ensure native libraries are present (macOS / Linux)
# ---------------------------------------------------------------------------
echo ""
echo "[Step 2] Checking native libraries..."

OS="$(uname -s)"
ARCH="$(uname -m)"

need_native_fix=false

if [ "$OS" = "Darwin" ]; then
    if [ ! -f "$TOOL_STORE/libclang.dylib" ] || [ ! -f "$TOOL_STORE/libClangSharp.dylib" ]; then
        need_native_fix=true
    fi
elif [ "$OS" = "Linux" ]; then
    if [ ! -f "$TOOL_STORE/libclang.so" ] || [ ! -f "$TOOL_STORE/libClangSharp.so" ]; then
        need_native_fix=true
    fi
fi

if [ "$need_native_fix" = true ]; then
    echo "  Native libraries missing. Downloading runtime packages..."

    # Determine RID
    RID=""
    if [ "$OS" = "Darwin" ]; then
        if [ "$ARCH" = "arm64" ]; then
            RID="osx-arm64"
        else
            RID="osx-x64"
        fi
    elif [ "$OS" = "Linux" ]; then
        if [ "$ARCH" = "aarch64" ]; then
            RID="linux-arm64"
        else
            RID="linux-x64"
        fi
    else
        echo "  ERROR: Unsupported OS '$OS'. Use the Windows .bat script instead."
        exit 1
    fi

    echo "  Platform: $OS / $ARCH -> RID: $RID"

    TEMP_DIR="$(mktemp -d)"
    trap 'rm -rf "$TEMP_DIR"' EXIT

    cat > "$TEMP_DIR/temp.csproj" << CSPROJ
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
  </PropertyGroup>
  <ItemGroup>
    <PackageReference Include="libclang.runtime.$RID" Version="$NATIVE_LIB_VERSION" />
    <PackageReference Include="libClangSharp.runtime.$RID" Version="$NATIVE_LIB_VERSION" />
  </ItemGroup>
</Project>
CSPROJ

    dotnet restore "$TEMP_DIR/temp.csproj" --packages "$TEMP_DIR/packages"

    if [ "$OS" = "Darwin" ]; then
        EXT="dylib"
    else
        EXT="so"
    fi

    LIBCLANG="$(find "$TEMP_DIR/packages" -name "libclang.$EXT" -path "*$RID*" | head -1)"
    LIBCLANGSHARP="$(find "$TEMP_DIR/packages" -name "libClangSharp.$EXT" -path "*$RID*" | head -1)"

    if [ -z "$LIBCLANG" ] || [ -z "$LIBCLANGSHARP" ]; then
        echo "  ERROR: Failed to locate native libraries in downloaded packages."
        exit 1
    fi

    cp "$LIBCLANG" "$TOOL_STORE/"
    cp "$LIBCLANGSHARP" "$TOOL_STORE/"

    echo "  Copied libclang.$EXT      -> $TOOL_STORE/"
    echo "  Copied libClangSharp.$EXT  -> $TOOL_STORE/"
else
    echo "  Native libraries already present."
fi

# ---------------------------------------------------------------------------
# Step 3: Run code generation
# ---------------------------------------------------------------------------
echo ""
echo "[Step 3] Generating C# bindings..."
echo "  RSP file: $ROOT_DIR/clangsharp.rsp"

cd "$ROOT_DIR"
"$DOTNET_TOOLS_DIR/$TOOL_NAME" @clangsharp.rsp

echo ""
echo "=========================================="
echo " Done! Bindings generated successfully."
echo "=========================================="
