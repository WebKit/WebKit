$ErrorActionPreference = "Stop"

$output = if ($env:WEBKIT_OUTPUT_DIR) { $env:WEBKIT_OUTPUT_DIR } else { "bun-webkit" }
$WebKitBuild = if ($env:WEBKIT_BUILD_DIR) { $env:WEBKIT_BUILD_DIR } else { "WebKitBuild" }
$CMAKE_BUILD_TYPE = if ($env:CMAKE_BUILD_TYPE) { $env:CMAKE_BUILD_TYPE } else { "Release" }
$BUN_WEBKIT_VERSION = if ($env:BUN_WEBKIT_VERSION) { $env:BUN_WEBKIT_VERSION } else { $(git rev-parse HEAD) }

# WebKit/JavaScriptCore requires being linked against the dynamic ICU library,
# but we do not want Bun to ship with DLLs, so we build ICU statically and
# link against that. This means we need both the static and dynamic build of
# ICU, once to link webkit, and second to link bun.
#
# I spent a few hours trying to find a way to get it to work with just the
# static library, did not get any meaningful results.
#
# Note that Bun works fine when you use this dual library technique.
$ICU_SOURCE_URL = "https://github.com/unicode-org/icu/releases/download/release-73-2/icu4c-73_2-src.tgz"
$ICU_SHARED_URL = "https://github.com/unicode-org/icu/releases/download/release-73-2/icu4c-73_2-Win64-MSVC2019.zip"

$ICU_STATIC_ROOT = Join-Path $WebKitBuild "icu"
$ICU_STATIC_LIBRARY = Join-Path $ICU_STATIC_ROOT "lib"
$ICU_STATIC_INCLUDE_DIR = Join-Path $ICU_STATIC_ROOT "include"

$ICU_SHARED_ROOT = Join-Path $WebKitBuild "icu-shared"
$ICU_SHARED_LIBRARY = Join-Path $ICU_SHARED_ROOT "lib64"
$ICU_SHARED_INCLUDE_DIR = Join-Path $ICU_SHARED_ROOT "include"

$null = mkdir $WebKitBuild -ErrorAction SilentlyContinue

if (!(Test-Path -Path $ICU_STATIC_ROOT)) {
    $ICU_STATIC_TAR = Join-Path $WebKitBuild "icu4c-src.tgz"
    
    if (!(Test-Path $ICU_STATIC_TAR)) {
        Write-Host ":: Downloading ICU"
        Invoke-WebRequest -Uri $ICU_SOURCE_URL -OutFile $ICU_STATIC_TAR
    }
    Write-Host ":: Extracting ICU"
    tar.exe -xzf "icu4c-73_2-src.tgz" -C $WebKitBuild
    if ($LASTEXITCODE -ne 0) { throw "tar failed with exit code $LASTEXITCODE" }

    # two patches needed to build statically with clang-cl
    
    # 1. replace references to `cl` with `clang-cl` from configure
    $ConfigureFile = Get-Content "$ICU_STATIC_ROOT/source/runConfigureICU" -Raw
    Set-Content "$ICU_STATIC_ROOT/source/runConfigureICU" ($ConfigureFile -replace "=cl", "=clang-cl") -NoNewline -Encoding UTF8
    # 2. hack remove dllimport from platform.h
    $PlatformFile = Get-Content "$ICU_STATIC_ROOT/source/common/unicode/platform.h" -Raw
    Set-Content "$ICU_STATIC_ROOT/source/common/unicode/platform.h" ($PlatformFile -replace "__declspec\(dllimport\)", "")
    
    Push-Location $ICU_STATIC_ROOT/source
    try {
        Write-Host ":: Configuring ICU Build"
        bash.exe ./runConfigureICU Cygwin/MSVC `
            --enable-static `
            --disable-shared `
            --with-data-packaging=static `
            --disable-samples `
            --disable-tests `
            --disable-debug `
            --enable-release
        if ($LASTEXITCODE -ne 0) { throw "runConfigureICU failed with exit code $LASTEXITCODE" }
    
        Write-Host ":: Building ICU"
        make "-j$((Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors)"
        if ($LASTEXITCODE -ne 0) { throw "make failed with exit code $LASTEXITCODE" }
    }
    finally { Pop-Location }
    
    $null = mkdir -Force $ICU_STATIC_INCLUDE_DIR/unicode
    Copy-Item -r $ICU_STATIC_ROOT/source/common/unicode/* $ICU_STATIC_INCLUDE_DIR/unicode
    Copy-Item -r $ICU_STATIC_ROOT/source/i18n/unicode/* $ICU_STATIC_INCLUDE_DIR/unicode
    $null = mkdir -Force $ICU_STATIC_LIBRARY
    Copy-Item -r $ICU_STATIC_ROOT/source/lib/* $ICU_STATIC_LIBRARY/
}

if (!(Test-Path -Path $ICU_SHARED_ROOT)) {
    # Download and extract URL
    Write-Host "Downloading shared library ICU from ${ICU_SHARED_URL}"
    $icuZipPath = Join-Path $WebKitBuild "icu.zip"
    if (!(Test-Path -Path $icuZipPath)) {
        Invoke-WebRequest -Uri $ICU_SHARED_URL -OutFile $icuZipPath
    }
    
    $null = New-Item -ItemType Directory -Path $ICU_SHARED_ROOT -Force
    $null = Expand-Archive $icuZipPath $ICU_SHARED_ROOT
    Get-ChildItem $ICU_SHARED_ROOT | Get-ChildItem
    Expand-Archive (Get-ChildItem $ICU_SHARED_ROOT | Get-ChildItem).FullName $ICU_SHARED_ROOT

}

$env:CC = "clang-cl"
$env:CXX = "clang-cl"
$env:CFLAGS = "/Zi /Z7"
$env:CXXFLAGS = "/Zi /Z7"

Write-Host ":: Configuring WebKit"

cmake -S . -B $WebKitBuild `
    -DPORT="JSCOnly" `
    -DENABLE_STATIC_JSC=ON `
    -DENABLE_SINGLE_THREADED_VM_ENTRY_SCOPE=ON `
    -DALLOW_LINE_AND_COLUMN_NUMBER_IN_BUILTINS=ON `
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}" `
    -DUSE_THIN_ARCHIVES=OFF `
    -DENABLE_DFG_JIT=ON `
    -DENABLE_FTL_JIT=OFF `
    -DENABLE_WEBASSEMBLY=OFF `
    -DUSE_BUN_JSC_ADDITIONS=ON `
    -DENABLE_BUN_SKIP_FAILING_ASSERTIONS=ON `
    -DUSE_SYSTEM_MALLOC=ON `
    "-DICU_ROOT=${ICU_SHARED_ROOT}" `
    "-DICU_LIBRARY=${ICU_SHARED_LIBRARY}" `
    "-DICU_INCLUDE_DIR=${ICU_SHARED_INCLUDE_DIR}" `
    "-DCMAKE_C_COMPILER=clang-cl" `
    "-DCMAKE_CXX_COMPILER=clang-cl" `
    "-DCMAKE_C_FLAGS_RELEASE=/Zi /O2 /Ob2 /DNDEBUG" `
    "-DCMAKE_CXX_FLAGS_RELEASE=/Zi /O2 /Ob2 /DNDEBUG" `
    -DENABLE_REMOTE_INSPECTOR=ON `
    -G Ninja
if ($LASTEXITCODE -ne 0) { throw "cmake failed with exit code $LASTEXITCODE" }

# Workaround for what is probably a CMake bug
$batFiles = Get-ChildItem -Path $WebKitBuild -Filter "*.bat" -File -Recurse
foreach ($file in $batFiles) {
    $content = Get-Content $file.FullName -Raw
    $newContent = $content -replace "(\|\| \(set FAIL_LINE=\d+& goto :ABORT\))", ""
    if ($content -ne $newContent) {
        Set-Content -Path $file.FullName -Value $newContent
        Write-Host ":: Patch $($file.FullName)"
    }
}

Write-Host ":: Building WebKit"
cmake --build $WebKitBuild --config Release --target jsc
if ($LASTEXITCODE -ne 0) { throw "cmake --build failed with exit code $LASTEXITCODE" }

Write-Host ":: Packaging ${output}"

Remove-Item -Recurse -ErrorAction SilentlyContinue $output
$null = mkdir -ErrorAction SilentlyContinue $output
$null = mkdir -ErrorAction SilentlyContinue $output/lib
$null = mkdir -ErrorAction SilentlyContinue $output/include
$null = mkdir -ErrorAction SilentlyContinue $output/include/JavaScriptCore
$null = mkdir -ErrorAction SilentlyContinue $output/include/wtf

Copy-Item $WebKitBuild/cmakeconfig.h $output/include/cmakeconfig.h
Copy-Item $WebKitBuild/lib64/JavaScriptCore.lib $output/lib/
# Copy-Item $WebKitBuild/lib64/JavaScriptCore.pdb $output/lib/
Copy-Item $WebKitBuild/lib64/WTF.lib $output/lib/
# Copy-Item $WebKitBuild/lib64/WTF.pdb $output/lib/

Add-Content -Path $output/include/cmakeconfig.h -Value "`#define BUN_WEBKIT_VERSION `"$BUN_WEBKIT_VERSION`""

Copy-Item -r -Force $WebKitBuild/JavaScriptCore/DerivedSources/* $output/include/JavaScriptCore
Copy-Item -r -Force $WebKitBuild/JavaScriptCore/Headers/JavaScriptCore/* $output/include/JavaScriptCore/
Copy-Item -r -Force $WebKitBuild/JavaScriptCore/PrivateHeaders/JavaScriptCore/* $output/include/JavaScriptCore/

Copy-Item -r $WebKitBuild/WTF/DerivedSources/* $output/include/wtf/
Copy-Item -r $WebKitBuild/WTF/Headers/wtf/* $output/include/wtf/

(Get-Content -Path $output/include/JavaScriptCore/JSValueInternal.h) `
    -replace "#import <JavaScriptCore/JSValuePrivate.h>", "#include <JavaScriptCore/JSValuePrivate.h>" `
| Set-Content -Path $output/include/JavaScriptCore/JSValueInternal.h

Copy-Item -r $ICU_STATIC_INCLUDE_DIR/* $output/include/
Copy-Item -r $ICU_STATIC_LIBRARY/* $output/lib/

tar -cz -f "${output}.tar.gz" "${output}"
if ($LASTEXITCODE -ne 0) { throw "tar failed with exit code $LASTEXITCODE" }

Write-Host "-> ${output}.tar.gz"
