#!/usr/bin/env bash

set -e
set -o pipefail

ARGON_JS_EXTRA_C_FLAGS=""
if [[ "$ARGON_JS_BUILD_BUILD_WITH_SIMD" == "1" ]]; then
    ARGON_JS_EXTRA_C_FLAGS="-msimd128 -msse2"
fi

cmake \
    -DOUTPUT_NAME="argon2" \
    -DCMAKE_TOOLCHAIN_FILE="$EMCC_SDK_PATH/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" \
    -DCMAKE_VERBOSE_MAKEFILE=OFF \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_C_FLAGS="-O $ARGON_JS_EXTRA_C_FLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="-O3 \
                              -s NO_FILESYSTEM=1 \
                              -s 'EXPORTED_FUNCTIONS=[\"_argon2_hash\",\"_argon2_hash_ext\",\"_argon2_verify\",\"_argon2_verify_ext\",\"_argon2_error_message\",\"_argon2_encodedlen\",\"_malloc\",\"_free\"]' \
                              -s 'EXPORTED_RUNTIME_METHODS=[\"UTF8ToString\",\"allocate\",\"ALLOC_NORMAL\"]' \
                              -s DEMANGLE_SUPPORT=0 \
                              -s ASSERTIONS=0 \
                              -s NO_EXIT_RUNTIME=1 \
                              -s TOTAL_MEMORY=16MB \
                              -s BINARYEN_MEM_MAX=2147418112 \
                              -s ALLOW_MEMORY_GROWTH=1 \
                              -s WASM=1" \
    .
cmake --build .

shasum dist/argon2.js
shasum dist/argon2.wasm

perl -pi -e 's/"argon2.js.mem"/null/g' dist/argon2.js
perl -pi -e 's/$/if(typeof module!=="undefined")module.exports=Module;Module.unloadRuntime=function(){if(typeof self!=="undefined"){delete self.Module}Module=jsModule=wasmMemory=wasmTable=asm=buffer=HEAP8=HEAPU8=HEAP16=HEAPU16=HEAP32=HEAPU32=HEAPF32=HEAPF64=undefined;if(typeof module!=="undefined"){delete module.exports}};/' dist/argon2.js
perl -pi -e 's/typeof Module!=="undefined"\?Module:\{};/typeof self!=="undefined"&&typeof self.Module!=="undefined"?self.Module:{};var jsModule=Module;/g' dist/argon2.js
perl -pi -e 's/receiveInstantiatedSource\(output\)\{/receiveInstantiatedSource(output){Module=jsModule;if(typeof self!=="undefined")self.Module=Module;/g' dist/argon2.js
