#! /bin/bash

# Expects to have .NET SDK 9.0.3xx with `wasm-tools` installed.
# Installation options:
# A) Download and manually install from https://aka.ms/dotnet/9.0.3xx/daily/dotnet-sdk-win-x64.zip or https://aka.ms/dotnet/9.0.3xx/daily/dotnet-sdk-linux-x64.tar.gz
# B) "Scripted install" as described in
# https://learn.microsoft.com/en-us/dotnet/core/install/linux-scripted-manual#scripted-install:
#   `wget https://dot.net/v1/dotnet-install.sh -O dotnet-install.sh && chmod +x ./dotnet-install.sh`, then
#   `./dotnet-install.sh --channel 9.0` (You must provide the 9.0 channel, otherwise it will install 8.0).
# Finally `sudo dotnet workload install wasm-tools` (without sudo for a user
# installation of dotnet, e.g., with option B above).

rm -r src/dotnet/bin src/dotnet/obj ./build-interp ./build-aot build.log

touch build.log
BUILD_LOG="$(realpath build.log)"

echo "Built on $(date -u '+%Y-%m-%dT%H:%M:%SZ')\n" | tee -a "$BUILD_LOG"
echo "Toolchain versions" | tee -a "$BUILD_LOG"
echo -n "dotnet " | tee -a "$BUILD_LOG"
dotnet --version | tee -a "$BUILD_LOG"
wasm-opt --version | tee -a "$BUILD_LOG"

for version in "interp" "aot"; do
    echo "Building $version..." | tee -a "$BUILD_LOG"

    DOTNET_ARGS=""
    if [ "$version" == "aot" ]; then
        DOTNET_ARGS+=" -p:RunAOTCompilation=true"
    fi
    # Use deterministic builds and don't embed build directory paths to avoid spurious binary updates.
    dotnet publish -o ./build-$version ./src/dotnet/dotnet.csproj -p:Deterministic=true -p:DeterministicSourcePaths=true $DOTNET_ARGS

    # Silence warning on ArrayBuffer instantiation, which we intentionally use
    # to keep the workload consistent between browsers and shells (the latter
    # don't always support streaming compilation.)
    perl -pi -e "s|\Q&&w('WebAssembly resource does not have the expected content type \"application/wasm\", so falling back to slower ArrayBuffer instantiation.')\E||g" ./build-$version/wwwroot/_framework/dotnet.js

    echo "Copying symbol maps..." | tee -a "$BUILD_LOG"
    cp ./src/dotnet/obj/Release/net9.0/wasm/for-publish/dotnet.native.js.symbols ./build-$version/wwwroot/_framework/

    # Net9 and Net10 use Emscripten version 3.1.56, which emits legacy EH, see https://github.com/WebKit/JetStream/pull/188
    # Only the `dotnetwasm` main file is a proper Wasm module, the rest is WebCIL, see https://github.com/dotnet/runtime/blob/main/docs/design/mono/webcil.md
    # FIXME: Update toolchain to Net11 once available, then remove this wasm-opt call.
    DOTNET_WASM_FILE="./build-$version/wwwroot/_framework/dotnet.native.wasm"
    wasm-opt "$DOTNET_WASM_FILE" -o "$DOTNET_WASM_FILE" --translate-to-exnref --enable-bulk-memory --enable-exception-handling --enable-simd --enable-reference-types --enable-multivalue
done
