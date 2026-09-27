param(
    [string]$NdkPath = $env:ANDROID_NDK_HOME,
    [string]$BuildType = "Release"
)

if (-not $NdkPath) {
    $NdkPath = $env:ANDROID_NDK_ROOT
}

if (-not $NdkPath) {
    # Check common Android SDK NDK paths
    $defaultSdkNdk = "$env:LOCALAPPDATA\Android\Sdk\ndk"
    if (Test-Path $defaultSdkNdk) {
        $latestNdk = Get-ChildItem $defaultSdkNdk | Sort-Object Name -Descending | Select-Object -First 1
        if ($latestNdk) {
            $NdkPath = $latestNdk.FullName
        }
    }
}

if (-not $NdkPath -or -not (Test-Path $NdkPath)) {
    Write-Error "Android NDK path not found. Please specify -NdkPath or set `$env:ANDROID_NDK_HOME."
    exit 1
}

[Environment]::SetEnvironmentVariable("ANDROID_NDK_HOME", $NdkPath, "Process")
[Environment]::SetEnvironmentVariable("ANDROID_NDK_ROOT", $NdkPath, "Process")

$patchMap = @{
    "ZArchive-android.patch" = "dependencies\ZArchive"
    "libadrenotools-android.patch" = "dependencies\libadrenotools"
    "xbyak_aarch64-android.patch" = "dependencies\xbyak_aarch64"
}
foreach ($name in $patchMap.Keys) {
    $patch = Join-Path "patches\android" $name
    $repo = $patchMap[$name]
    git -C $repo apply --check $patch 2>$null
    if ($LASTEXITCODE -eq 0) {
        git -C $repo apply $patch
        if ($LASTEXITCODE -ne 0) { throw "Failed to apply $name" }
        Write-Host "Applied $name"
    } else {
        git -C $repo apply --reverse --check $patch 2>$null
        if ($LASTEXITCODE -ne 0) { throw "Patch $name does not match $repo" }
    }
}
$vcpkgPath = (Resolve-Path "dependencies\vcpkg").Path
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgPath, "Process")

# Auto-detect cmake in Android SDK if not in PATH
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $sdkCmake = Get-ChildItem "$env:LOCALAPPDATA\Android\Sdk\cmake" -Recurse -Filter "cmake.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($sdkCmake) {
        $cmakeBinDir = Split-Path $sdkCmake.FullName
        $env:PATH = "$cmakeBinDir;$env:PATH"
    }
}

$BuildDir = "build_libretro_android_arm64"
$ToolchainFile = "$NdkPath/build/cmake/android.toolchain.cmake"

Write-Host "=== Building Cemu Libretro Core for Android ARM64 (arm64-v8a) ===" -ForegroundColor Green
Write-Host "NDK Path: $NdkPath"
Write-Host "Build Type: $BuildType"
Write-Host "Build Dir: $BuildDir"

cmake -S . -B $BuildDir `
    -DCMAKE_TOOLCHAIN_FILE="$ToolchainFile" `
    -DANDROID_ABI="arm64-v8a" `
    -DANDROID_PLATFORM="android-30" `
    -DANDROID_STL="c++_shared" `
    -DCMAKE_BUILD_TYPE="$BuildType" `
    -DENABLE_VCPKG=ON `
    -DVCPKG_TARGET_ANDROID=ON `
    -DENABLE_SDL=OFF `
    -DENABLE_WXWIDGETS=OFF `
    -DENABLE_OPENGL=OFF `
    -DENABLE_BLUEZ=OFF `
    -DBUNDLE_SPEEX=ON `
    -DENABLE_DISCORD_RPC=OFF `
    -DENABLE_NSYSHID_LIBUSB=OFF `
    -DENABLE_WAYLAND=OFF `
    -DENABLE_HIDAPI=OFF `
    -G Ninja

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed."
    exit $LASTEXITCODE
}

cmake --build $BuildDir --target cemu_libretro

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n=== Build Successful! ===" -ForegroundColor Green
    $builtSo = "$BuildDir/src/libretro/libcemu_libretro.so"
    if (-not (Test-Path $builtSo)) {
        $builtSo = "$BuildDir/src/libretro/cemu_libretro.so"
    }
    Write-Host "Output: $builtSo"
    $targetJni = "d:\Work\Omnidroid\omnidroid-cores\omnidroid_core_cemu\src\main\jniLibs\arm64-v8a\libcemu_libretro_android.so"
    if (Test-Path "d:\Work\Omnidroid\omnidroid-cores\omnidroid_core_cemu\src\main\jniLibs\arm64-v8a") {
        Copy-Item -Path $builtSo -Destination $targetJni -Force
        Write-Host "Copied to: $targetJni" -ForegroundColor Green
    }
} else {
    Write-Error "Build failed."
}
