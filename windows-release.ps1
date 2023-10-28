$ErrorActionPreference = "Stop"

$output = if ($env:WEBKIT_OUTPUT_DIR) { $env:WEBKIT_OUTPUT_DIR } else { "bun-webkit" }
$WebKitBuild = if ($env:WEBKIT_BUILD_DIR) { $env:WEBKIT_BUILD_DIR } else { "WebKitBuild" }
$CMAKE_BUILD_TYPE = if ($env:CMAKE_BUILD_TYPE) { $env:CMAKE_BUILD_TYPE } else { "Release" }
$BUN_WEBKIT_VERSION = if ($env:BUN_WEBKIT_VERSION) { $env:BUN_WEBKIT_VERSION } else { $(git rev-parse HEAD) }

$ICU_URL = "https://github.com/unicode-org/icu/releases/download/release-73-1/icu4c-73_1-Win64-MSVC2019.zip"
$ICU_ROOT = Join-Path $WebKitBuild "libicu"
$ICU_LIBRARY = Join-Path $ICU_ROOT "lib64"
$ICU_INCLUDE_DIR = Join-Path $ICU_ROOT "include"

$null = mkdir $WebKitBuild -ErrorAction SilentlyContinue

if (!(Test-Path -Path $ICU_ROOT)) {
    # Download and extract URL
    Write-Host "Downloading ICU from ${ICU_URL}"
    $icuZipPath = Join-Path $WebKitBuild "icu.zip"
    if (!(Test-Path -Path $icuZipPath)) {
        Invoke-WebRequest -Uri $ICU_URL -OutFile $icuZipPath
    }
    $null = New-Item -ItemType Directory -Path $ICU_ROOT
    $null = Expand-Archive $icuZipPath $ICU_ROOT
    Remove-Item -Path $icuZipPath
}

$env:CC = "clang-cl"
$env:CXX = "clang-cl"
# for some reason we don't get pdb files by default
# if we used MSVC we would, so it's something with clang-cl??
# these flags don't actually work, but docs say they should...
$env:CFLAGS = "/Zi /Z7"
$env:CXXFLAGS = "/Zi /Z7"

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
    "-DICU_ROOT=${ICU_ROOT}" `
    "-DICU_LIBRARY=${ICU_LIBRARY}" `
    "-DICU_INCLUDE_DIR=${ICU_INCLUDE_DIR}" `
    "-DCMAKE_C_COMPILER=clang-cl" `
    "-DCMAKE_CXX_COMPILER=clang-cl" `
    -DENABLE_REMOTE_INSPECTOR=ON `
    -G Ninja

# Workaround for what is probably a CMake bug
$batFiles = Get-ChildItem -Path $WebKitBuild -Filter "*.bat" -File -Recurse
foreach ($file in $batFiles) {
    $content = Get-Content $file.FullName -Raw
    $newContent = $content -replace "(\|\| \(set FAIL_LINE=\d+& goto :ABORT\))", ""
    if ($content -ne $newContent) {
        Set-Content -Path $file.FullName -Value $newContent
        Write-Host "Patched: $($file.FullName)"
    }
}

cmake --build $WebKitBuild --config Release --target jsc

Remove-Item -Recurse -ErrorAction SilentlyContinue $output
$null = mkdir -ErrorAction SilentlyContinue $output
$null = mkdir -ErrorAction SilentlyContinue $output/lib
$null = mkdir -ErrorAction SilentlyContinue $output/include
$null = mkdir -ErrorAction SilentlyContinue $output/include/JavaScriptCore
$null = mkdir -ErrorAction SilentlyContinue $output/include/wtf

Copy-Item $WebKitBuild/cmakeconfig.h $output/include/cmakeconfig.h

# the pdb is commented because of above
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

Copy-Item -r $WebKitBuild/libicu/include/* $output/include/
Copy-Item $WebKitBuild/libicu/lib64/* $output/lib/

Remove-Item $output/lib/testplug.pdb

tar -cz -f "${output}.tar.gz" "${output}"

Write-Host "-> ${output}.tar.gz"
