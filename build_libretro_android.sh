#!/usr/bin/env bash
set -e

# Configure Android NDK path
if [ -z "$ANDROID_NDK_HOME" ] && [ -n "$ANDROID_NDK_ROOT" ]; then
    export ANDROID_NDK_HOME="$ANDROID_NDK_ROOT"
fi

if [ -z "$ANDROID_NDK_HOME" ]; then
    echo "ERROR: ANDROID_NDK_HOME or ANDROID_NDK_ROOT is not set."
    echo "Please set ANDROID_NDK_HOME to your Android NDK directory (e.g. export ANDROID_NDK_HOME=/path/to/ndk/25.2.9519653)"
    exit 1
fi

apply_patch() {
    local repo="$1"
    local patch="$2"
    if git -C "$repo" apply --check "$patch" 2>/dev/null; then
        git -C "$repo" apply "$patch"
        echo "Applied $(basename "$patch")"
    elif git -C "$repo" apply --reverse --check "$patch" 2>/dev/null; then
        :
    else
        echo "ERROR: $patch does not match $repo"
        exit 1
    fi
}

apply_patch dependencies/ZArchive "$PWD/patches/android/ZArchive-android.patch"
apply_patch dependencies/libadrenotools "$PWD/patches/android/libadrenotools-android.patch"
apply_patch dependencies/xbyak_aarch64 "$PWD/patches/android/xbyak_aarch64-android.patch"

BUILD_DIR="build_libretro_android_arm64"
BUILD_TYPE="${1:-Release}"

echo "=== Building Cemu Libretro Core for Android ARM64 (arm64-v8a) ==="
echo "NDK: $ANDROID_NDK_HOME"
echo "Build Type: $BUILD_TYPE"
echo "Build Dir: $BUILD_DIR"

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-30 \
    -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DENABLE_VCPKG=ON \
    -DVCPKG_TARGET_ANDROID=ON \
    -DENABLE_SDL=OFF \
    -DENABLE_WXWIDGETS=OFF \
    -DENABLE_OPENGL=OFF \
    -DENABLE_BLUEZ=OFF \
    -DBUNDLE_SPEEX=ON \
    -DENABLE_DISCORD_RPC=OFF \
    -DENABLE_NSYSHID_LIBUSB=OFF \
    -DENABLE_WAYLAND=OFF \
    -DENABLE_HIDAPI=OFF \
    -G Ninja

cmake --build "$BUILD_DIR" --target cemu_libretro -j$(nproc 2>/dev/null || echo 4)

echo ""
echo "=== Build Complete! ==="
echo "Core library: $BUILD_DIR/src/libretro/cemu_libretro.so"
