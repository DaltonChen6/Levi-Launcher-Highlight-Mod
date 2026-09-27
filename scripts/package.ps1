param(
    [ValidateSet("arm64-v8a", "armeabi-v7a", "all")]
    [string]$Abi = "arm64-v8a",
    [string]$BuildRoot = "build",
    [string]$AndroidPlatform = "android-28",
    [string]$NdkHome = "",
    [string]$Generator = "Ninja"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..")

function Resolve-Cmake {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) { return $cmake.Source }

    if (-not [string]::IsNullOrWhiteSpace($env:ANDROID_HOME)) {
        $cmakeRoot = Join-Path $env:ANDROID_HOME "cmake"
        if (Test-Path $cmakeRoot) {
            $candidate = Get-ChildItem -Path $cmakeRoot -Directory |
                Sort-Object Name -Descending |
                Select-Object -First 1
            if ($candidate) {
                $exe = Join-Path $candidate.FullName "bin\cmake.exe"
                if (Test-Path $exe) { return $exe }
            }
        }
    }

    throw "cmake not found. Install CMake or set ANDROID_HOME to the Android SDK directory."
}

function Resolve-NdkHome {
    if (-not [string]::IsNullOrWhiteSpace($NdkHome) -and (Test-Path $NdkHome)) {
        return (Resolve-Path $NdkHome).Path
    }

    if (-not [string]::IsNullOrWhiteSpace($env:ANDROID_HOME)) {
        $ndkRoot = Join-Path $env:ANDROID_HOME "ndk"
        if (Test-Path $ndkRoot) {
            $candidate = Get-ChildItem -Path $ndkRoot -Directory |
                Sort-Object Name -Descending |
                Select-Object -First 1
            if ($candidate) {
                return $candidate.FullName
            }
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($env:ANDROID_NDK_HOME) -and (Test-Path $env:ANDROID_NDK_HOME)) {
        return (Resolve-Path $env:ANDROID_NDK_HOME).Path
    }

    throw "Android NDK not found. Set ANDROID_NDK_HOME or ANDROID_HOME."
}

$cmakeExe = Resolve-Cmake

# Put the SDK cmake bin directory on PATH so the Ninja generator can find ninja.exe.
$cmakeBinDir = Split-Path -Parent $cmakeExe
if ($cmakeBinDir -and ($env:Path -split ';') -notcontains $cmakeBinDir) {
    $env:Path = "$cmakeBinDir;$env:Path"
}

function Invoke-ModBuild {
    param([string]$TargetAbi)

    $ndkHome = Resolve-NdkHome
    $toolchain = Join-Path $ndkHome "build/cmake/android.toolchain.cmake"
    $buildDir = Join-Path $repoRoot "$BuildRoot-$TargetAbi"

    & $cmakeExe -S $repoRoot -B $buildDir -G $Generator `
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
        -DANDROID_ABI="$TargetAbi" `
        -DANDROID_PLATFORM="$AndroidPlatform" `
        -DANDROID_STL="c++_shared" `
        2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed for $TargetAbi" }

    & $cmakeExe --build $buildDir --target levi_package 2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed for $TargetAbi" }
}

if ($Abi -eq "all") {
    Invoke-ModBuild "arm64-v8a"
    Invoke-ModBuild "armeabi-v7a"
} else {
    Invoke-ModBuild $Abi
}

Write-Host ""
Write-Host "Build finished. Import the generated .levipack into LeviLauncher."