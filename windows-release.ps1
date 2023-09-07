# Set default OUT_DIR to WebKitBuild
if (!$env:WEBKIT_BUILD_DIR) {
    $env:WEBKIT_BUILD_DIR = "WebKitBuild"
}

# ICU_LIBRARY is expected to exist: C:\Users\windo\Build\libicu\lib64
# ICU_INCLUDE_DIR is expected to exist: C:\Users\windo\Build\libicu\include
# ICU_ROOT is expected to exist: C:\Users\windo\Build\libicu
cmake `
    -DPORT="JSCOnly" `
    -DENABLE_STATIC_JSC=ON `
    -DENABLE_SINGLE_THREADED_VM_ENTRY_SCOPE=ON `
    -DALLOW_LINE_AND_COLUMN_NUMBER_IN_BUILTINS=ON `
    -DCMAKE_BUILD_TYPE=Release `
    -DUSE_THIN_ARCHIVES=OFF `
    -DENABLE_DFG_JIT=ON `
    -DENABLE_FTL_JIT=OFF `
    -DENABLE_WEBASSEMBLY=OFF `
    -DUSE_BUN_JSC_ADDITIONS=ON `
    -S . `
    -B ${env:WEBKIT_BUILD_DIR} `
    -DUSE_SYSTEM_MALLOC=ON `
    -DSTATICALLY_LINKED_WITH_WTF=ON

cmake --build WebKitBuild --config Release --target jsc

$output = "bun-webkit-x64"
if ($env:WEBKIT_OUTPUT_DIR) {
    $output = $env:WEBKIT_OUTPUT_DIR
}

rm -r -Force $output
mkdir -Force $output
mkdir -Force $output/lib
mkdir -Force $output/include
mkdir -Force $output/scripts
mkdir -Force $output/include/JavaScriptCore
mkdir -Force $output/include/wtf

cp WebKitBuild/cmakeconfig.h $output/include/cmakeconfig.h
cp WebKitBuild/lib64/*.lib $output/lib/
cp WebKitBuild/lib64/*.pdb $output/lib/

$BUN_WEBKIT_VERSION = $(git rev-parse HEAD)

# Append #define BUN_WEBKIT_VERSION "${BUN_WEBKIT_VERSION}" to $output/include/cmakeconfig.h
Add-Content -Path $output/include/cmakeconfig.h -Value "`#define BUN_WEBKIT_VERSION `"`"$BUN_WEBKIT_VERSION`"`""

cp -r -Force WebKitBuild/JavaScriptCore/DerivedSources/* $output/include/JavaScriptCore
cp -r -Force WebKitBuild/JavaScriptCore/Headers/JavaScriptCore/* $output/include/JavaScriptCore/
cp -r -Force WebKitBuild/JavaScriptCore/PrivateHeaders/JavaScriptCore/* $output/include/JavaScriptCore/

cp -r WebKitBuild/WTF/DerivedSources/* $output/include/wtf/
cp -r WebKitBuild/WTF/Headers/wtf/* $output/include/wtf/

cp  WebKitBuild/JavaScriptCore/Scripts/*.py $output/scripts/
