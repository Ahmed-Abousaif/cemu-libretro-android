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

$vcvars = $null
$programFilesX86 = [Environment]::GetFolderPath("ProgramFilesX86")
$vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $vcvarsCandidate = Join-Path $vsInstall "VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $vcvarsCandidate) {
        $vcvars = $vcvarsCandidate
    }
}

$env:ANDROID_NDK_HOME = $NdkPath
$env:ANDROID_NDK_ROOT = $NdkPath
[Environment]::SetEnvironmentVariable("ANDROID_NDK_HOME", $NdkPath, "Process")
[Environment]::SetEnvironmentVariable("ANDROID_NDK_ROOT", $NdkPath, "Process")

$patchMap = @{
    "ZArchive-android.patch" = "dependencies\ZArchive"
    "libadrenotools-android.patch" = "dependencies\libadrenotools"
    "xbyak_aarch64-android.patch" = "dependencies\xbyak_aarch64"
}
foreach ($name in $patchMap.Keys) {
    $patch = Join-Path $PSScriptRoot "patches\android\$name"
    $repo = Join-Path $PSScriptRoot $patchMap[$name]
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
$env:VCPKG_ROOT = $vcpkgPath
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgPath, "Process")

# Auto-detect cmake and ninja in the Android SDK if they are not already on PATH
$sdkCmake = Get-ChildItem "$env:LOCALAPPDATA\Android\Sdk\cmake" -Recurse -Filter "cmake.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($sdkCmake) {
    $cmakeBinDir = Split-Path $sdkCmake.FullName
    $env:PATH = "$cmakeBinDir;$env:PATH"
}

$BuildDir = "build_libretro_android_arm64"
$ToolchainFile = "$NdkPath/build/cmake/android.toolchain.cmake"

Write-Host "=== Building Cemu Libretro Core for Android ARM64 (arm64-v8a) ===" -ForegroundColor Green
Write-Host "NDK Path: $NdkPath"
Write-Host "Build Type: $BuildType"
Write-Host "Build Dir: $BuildDir"

$ninja = Join-Path $cmakeBinDir "ninja.exe"
$vcvarsSetup = ""
if ($vcvars) {
    $vcvarsSetup = "call `"$vcvars`" >nul`r`n"
}
$buildBat = Join-Path $PSScriptRoot "build_libretro_android_arm64.bat"
@(
    "@echo off"
    $vcvarsSetup.TrimEnd()
    "set `"ANDROID_NDK_HOME=$NdkPath`""
    "set `"ANDROID_NDK_ROOT=$NdkPath`""
    "set `"VCPKG_ROOT=$vcpkgPath`""
    "set `"PATH=$cmakeBinDir;%PATH%`""
    "cd /d `"$PSScriptRoot`""
    "cmake -S . -B $BuildDir -DCMAKE_TOOLCHAIN_FILE=`"$ToolchainFile`" -DANDROID_NDK=`"$NdkPath`" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-30 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=$BuildType -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DENABLE_VCPKG=ON -DVCPKG_TARGET_ANDROID=ON -DENABLE_SDL=OFF -DENABLE_WXWIDGETS=OFF -DENABLE_OPENGL=OFF -DENABLE_BLUEZ=OFF -DBUNDLE_SPEEX=ON -DENABLE_DISCORD_RPC=OFF -DENABLE_NSYSHID_LIBUSB=OFF -DENABLE_WAYLAND=OFF -DENABLE_HIDAPI=OFF -G Ninja"
    "if errorlevel 1 exit /b 1"
    "cmake --build $BuildDir --target cemu_libretro"
    "exit /b %ERRORLEVEL%"
) | Set-Content -Path $buildBat -Encoding ascii
cmd /c $buildBat
Remove-Item $buildBat -ErrorAction SilentlyContinue

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
