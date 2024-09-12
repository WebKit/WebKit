param(
    # not on by default because CI does not need it (for some reason)
    # and i really really really really do not want to break anything
    #
    # you almost certainly need this to build locally
    [switch][bool]$ExtraEffortPathManagement = $false
)
$ErrorActionPreference = "Stop"

# Set up MSVC environment variables. This is taken from Bun's 'scripts\env.ps1'
if ($env:VSINSTALLDIR -eq $null) {
    Write-Host "Loading Visual Studio environment, this may take a second..."
    $vsDir = Get-ChildItem -Path "C:\Program Files\Microsoft Visual Studio\2022" -Directory
    if ($vsDir -eq $null) {
        throw "Visual Studio directory not found."
    } 
    Push-Location $vsDir
    try {
        . (Join-Path -Path $vsDir.FullName -ChildPath "Common7\Tools\Launch-VsDevShell.ps1") -Arch amd64 -HostArch amd64
    }
    finally { Pop-Location }
}

if ($Env:VSCMD_ARG_TGT_ARCH -eq "x86") {
    # Please do not try to compile Bun for 32 bit. It will not work. I promise.
    throw "Visual Studio environment is targetting 32 bit. This configuration is definetly a mistake."
}

# Fix up $PATH
Write-Host $env:PATH

$MakeExe = (Get-Command make).Path

$SplitPath = $env:PATH -split ";";
$MSVCPaths = $SplitPath | Where-Object { $_ -like "Microsoft Visual Studio" }
$SplitPath = $MSVCPaths + ($SplitPath | Where-Object { $_ -notlike "Microsoft Visual Studio" } | Where-Object { $_ -notlike "*mingw*" })
$PathWithPerl = $SplitPath -join ";"
$env:PATH = ($SplitPath | Where-Object { $_ -notlike "*strawberry*" }) -join ';'

if($ExtraEffortPathManagement) {
    $SedPath = $(& cygwin.exe -c 'where sed')
    $SedDir = Split-Path $SedPath
    $LinkPath = $(gcm link).Path
    $LinkDir = Split-Path $LinkPath

    # override coreutils link with the msvc one
    $env:PATH = "$LinkDir;$SedDir;$PathWithPerl"
}

Write-Host $env:PATH

(Get-Command link).Path
clang-cl.exe --version

$env:CC = "clang-cl"
$env:CXX = "clang-cl"

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
# TODO: update to 75.1. It seems that additional CFLAGS need to be passed here.
$ICU_SOURCE_URL = "https://github.com/unicode-org/icu/releases/download/release-73-2/icu4c-73_2-src.tgz"

$ICU_STATIC_ROOT = Join-Path $WebKitBuild "icu"
$ICU_STATIC_LIBRARY = Join-Path $ICU_STATIC_ROOT "lib"
$ICU_STATIC_INCLUDE_DIR = Join-Path $ICU_STATIC_ROOT "include"

$null = mkdir $WebKitBuild -ErrorAction SilentlyContinue

if (!(Test-Path -Path $ICU_STATIC_ROOT)) {
    $ICU_STATIC_TAR = Join-Path $WebKitBuild "icu4c-src.tgz"
    
    if (!(Test-Path $ICU_STATIC_TAR)) {
        Write-Host ":: Downloading ICU"
        Invoke-WebRequest -Uri $ICU_SOURCE_URL -OutFile $ICU_STATIC_TAR
    }
    Write-Host ":: Extracting ICU"
    tar.exe -xzf $ICU_STATIC_TAR -C $WebKitBuild
    if ($LASTEXITCODE -ne 0) { throw "tar failed with exit code $LASTEXITCODE" }

    # two patches needed to build statically with clang-cl
    
    # 1. fix build script to align with bun's compiler requirements
    #    a. replace references to `cl` with `clang-cl` from configure
    #    b. use -MT instead of -MD to statically link the C runtime
    #    c. enable debug build for Debug configuration
    $ConfigureFile = Get-Content "$ICU_STATIC_ROOT/source/runConfigureICU" -Raw
    $ConfigureFile = $ConfigureFile -replace "=cl", "=clang-cl"
    if ($CMAKE_BUILD_TYPE -eq "Debug") {
        $ConfigureFile = $ConfigureFile -replace "debug=0", "debug=1"
        $ConfigureFile = $ConfigureFile -replace "release=1", "release=0"
        $ConfigureFile = $ConfigureFile -replace "-MDd", "-MTd /DU_STATIC_IMPLEMENTATION"
    } else {
        $ConfigureFile = $ConfigureFile -replace "-MD'", "-MT /DU_STATIC_IMPLEMENTATION'"
    }

    Set-Content "$ICU_STATIC_ROOT/source/runConfigureICU" $ConfigureFile -NoNewline -Encoding UTF8
    
    Push-Location $ICU_STATIC_ROOT/source
    try {
        Write-Host ":: Configuring ICU Build"

        if ($CMAKE_BUILD_TYPE -eq "Release") {
            bash.exe ./runConfigureICU Cygwin/MSVC `
                --enable-static `
                --disable-shared `
                --with-data-packaging=static `
                --disable-samples `
                --disable-tests `
                --disable-debug `
                --enable-release
        } elseif ($CMAKE_BUILD_TYPE -eq "Debug") {
            bash.exe ./runConfigureICU Cygwin/MSVC `
                --enable-static `
                --disable-shared `
                --with-data-packaging=static `
                --disable-samples `
                --disable-tests `
                --enable-debug `
                --disable-release
        }

        if ($LASTEXITCODE -ne 0) { 
            Get-Content "config.log"
            throw "runConfigureICU failed with exit code $LASTEXITCODE"
        }
    
        Write-Host ":: Building ICU"
        & $MakeExe "-j$((Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors)"
        if ($LASTEXITCODE -ne 0) { throw "make failed with exit code $LASTEXITCODE" }
    }
    finally { Pop-Location }
    
    $null = mkdir -Force $ICU_STATIC_INCLUDE_DIR/unicode
    Copy-Item -r $ICU_STATIC_ROOT/source/common/unicode/* $ICU_STATIC_INCLUDE_DIR/unicode
    Copy-Item -r $ICU_STATIC_ROOT/source/i18n/unicode/* $ICU_STATIC_INCLUDE_DIR/unicode
    $null = mkdir -Force $ICU_STATIC_LIBRARY
    Copy-Item -r $ICU_STATIC_ROOT/source/lib/* $ICU_STATIC_LIBRARY/

    # JSC expects the static library to be named icudt.lib
    if ($CMAKE_BUILD_TYPE -eq "Release") {
        Move-Item $ICU_STATIC_LIBRARY/sicudt.lib $ICU_STATIC_LIBRARY/icudt.lib
        Move-Item $ICU_STATIC_LIBRARY/sicutu.lib $ICU_STATIC_LIBRARY/icutu.lib
        Move-Item $ICU_STATIC_LIBRARY/sicuio.lib $ICU_STATIC_LIBRARY/icuio.lib
        Move-Item $ICU_STATIC_LIBRARY/sicuin.lib $ICU_STATIC_LIBRARY/icuin.lib
        Move-Item $ICU_STATIC_LIBRARY/sicuuc.lib $ICU_STATIC_LIBRARY/icuuc.lib
    } elseif ($CMAKE_BUILD_TYPE -eq "Debug") {
        Move-Item $ICU_STATIC_LIBRARY/sicudtd.lib $ICU_STATIC_LIBRARY/icudt.lib
        Move-Item $ICU_STATIC_LIBRARY/sicutud.lib $ICU_STATIC_LIBRARY/icutu.lib
        Move-Item $ICU_STATIC_LIBRARY/sicuiod.lib $ICU_STATIC_LIBRARY/icuio.lib
        Move-Item $ICU_STATIC_LIBRARY/sicuind.lib $ICU_STATIC_LIBRARY/icuin.lib
        Move-Item $ICU_STATIC_LIBRARY/sicuucd.lib $ICU_STATIC_LIBRARY/icuuc.lib
    }
}

Write-Host ":: Configuring WebKit"

$env:PATH = $PathWithPerl

$env:CFLAGS = "/Zi"
$env:CXXFLAGS = "/Zi"

$CmakeMsvcRuntimeLibrary = "MultiThreaded"
if ($CMAKE_BUILD_TYPE -eq "Debug") {
    $CmakeMsvcRuntimeLibrary = "MultiThreadedDebug"
}

$NoWebassembly = if ($env:NO_WEBASSEMBLY) { $env:NO_WEBASSEMBLY } else { $false }
$WebAssemblyState = if ($NoWebassembly) { "OFF" } else { "ON" }

cmake -S . -B $WebKitBuild `
    -DPORT="JSCOnly" `
    -DENABLE_STATIC_JSC=ON `
    -DENABLE_SINGLE_THREADED_VM_ENTRY_SCOPE=ON `
    -DALLOW_LINE_AND_COLUMN_NUMBER_IN_BUILTINS=ON `
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}" `
    -DUSE_THIN_ARCHIVES=OFF `
    -DENABLE_JIT=ON `
    -DENABLE_DFG_JIT=ON `
    -DENABLE_FTL_JIT=ON `
    -DENABLE_WEBASSEMBLY_BBQJIT=ON `
    -DENABLE_WEBASSEMBLY_OMGJIT=ON `
    -DENABLE_SAMPLING_PROFILER=ON `
    "-DENABLE_WEBASSEMBLY=${WebAssemblyState}" `
    -DUSE_BUN_JSC_ADDITIONS=ON `
    -DENABLE_BUN_SKIP_FAILING_ASSERTIONS=ON `
    -DUSE_SYSTEM_MALLOC=ON `
    "-DICU_ROOT=${ICU_STATIC_ROOT}" `
    "-DICU_LIBRARY=${ICU_STATIC_LIBRARY}" `
    "-DICU_INCLUDE_DIR=${ICU_STATIC_INCLUDE_DIR}" `
    "-DCMAKE_C_COMPILER=clang-cl" `
    "-DCMAKE_CXX_COMPILER=clang-cl" `
    "-DCMAKE_C_FLAGS_RELEASE=/Zi /O2 /Ob2 /DNDEBUG  " `
    "-DCMAKE_CXX_FLAGS_RELEASE=/Zi /O2 /Ob2 /DNDEBUG  -Xclang -fno-c++-static-destructors " `
    "-DCMAKE_C_FLAGS_DEBUG=/Zi /FS /O0 /Ob0 " `
    "-DCMAKE_CXX_FLAGS_DEBUG=/Zi /FS /O0 /Ob0 -Xclang -fno-c++-static-destructors " `
    -DENABLE_REMOTE_INSPECTOR=ON `
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CmakeMsvcRuntimeLibrary}" `
    -G Ninja
# TODO: "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded" `
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
cmake --build $WebKitBuild --config Release --target jsc --verbose
if ($LASTEXITCODE -ne 0) { throw "cmake --build failed with exit code $LASTEXITCODE" }

Write-Host ":: Packaging ${output}"

# Dump the entire tree of files in $WebKitBuild to the console.
# This is useful for debugging.
Get-ChildItem -Recurse $WebKitBuild | Format-List -Force | Out-String | Write-Host

Remove-Item -Recurse -ErrorAction SilentlyContinue $output
$null = mkdir -ErrorAction SilentlyContinue $output
$null = mkdir -ErrorAction SilentlyContinue $output/include
$null = mkdir -ErrorAction SilentlyContinue $output/include/JavaScriptCore
$null = mkdir -ErrorAction SilentlyContinue $output/include/wtf

Copy-Item -Recurse $WebKitBuild/lib $output
Copy-Item -Recurse $WebKitBuild/bin $output

# If there's a lib64, also copy it.
if (Test-Path -Path $WebKitBuild/lib64) {
    Copy-Item -Recurse $WebKitBuild/lib64/* $output/lib
}

Copy-Item $WebKitBuild/cmakeconfig.h $output/include/cmakeconfig.h

if ($CMAKE_BUILD_TYPE -eq "Release") {
    Move-Item $ICU_STATIC_LIBRARY/icudt.lib $ICU_STATIC_LIBRARY/sicudt.lib
    Move-Item $ICU_STATIC_LIBRARY/icutu.lib $ICU_STATIC_LIBRARY/sicutu.lib
    Move-Item $ICU_STATIC_LIBRARY/icuio.lib $ICU_STATIC_LIBRARY/sicuio.lib
    Move-Item $ICU_STATIC_LIBRARY/icuin.lib $ICU_STATIC_LIBRARY/sicuin.lib
    Move-Item $ICU_STATIC_LIBRARY/icuuc.lib $ICU_STATIC_LIBRARY/sicuuc.lib
} elseif ($CMAKE_BUILD_TYPE -eq "Debug") {
    Move-Item $ICU_STATIC_LIBRARY/icudt.lib $ICU_STATIC_LIBRARY/sicudtd.lib
    Move-Item $ICU_STATIC_LIBRARY/icutu.lib $ICU_STATIC_LIBRARY/sicutud.lib
    Move-Item $ICU_STATIC_LIBRARY/icuio.lib $ICU_STATIC_LIBRARY/sicuiod.lib
    Move-Item $ICU_STATIC_LIBRARY/icuin.lib $ICU_STATIC_LIBRARY/sicuind.lib
    Move-Item $ICU_STATIC_LIBRARY/icuuc.lib $ICU_STATIC_LIBRARY/sicuucd.lib
}

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
