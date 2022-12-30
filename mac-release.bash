#!/usr/bin/env bash

set -euxo pipefail

# This script is intended to be run on a Mac machine to build a release
# of the CLI. It will build the CLI for all supported platforms and
# architectures, and then create a tarball of the binaries.

THIS_DIR=$(pwd)

# Set default values for environment variables that are not set.
CMAKE_C_COMPILER=${CMAKE_C_COMPILER:-clang-15}
CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER:-clang++}
CMAKE_C_FLAGS=${CMAKE_C_FLAGS:-}
CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS:-}

rm -rf $RUNNER_TEMP/webkit-release $RUNNER_TEMP/bun-webkit $RUNNER_TEMP/bun-webkit $RUNNER_TEMP/bun-webkit.tar.gz
mkdir -p $RUNNER_TEMP/webkit-release
cd $RUNNER_TEMP/webkit-release
cmake \
    -DPORT="JSCOnly" \
    -DENABLE_STATIC_JSC=ON \
    -DENABLE_SINGLE_THREADED_VM_ENTRY_SCOPE=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_THIN_ARCHIVES=OFF \
    -DENABLE_FTL_JIT=ON \
    -DCMAKE_C_COMPILER="$CMAKE_C_COMPILER" \
    -DCMAKE_CXX_COMPILER="$CMAKE_CXX_COMPILER" \
    -DCMAKE_C_FLAGS="$CMAKE_C_FLAGS" \
    -DCMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" \
    -DCMAKE_AR=$(which llvm-ar) \
    -DCMAKE_RANLIB=$(which llvm-ranlib) \
    -G Ninja \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DPTHREAD_JIT_PERMISSIONS_API=1 \
    -DUSE_PTHREAD_JIT_PERMISSIONS_API=ON \
    -DENABLE_REMOTE_INSPECTOR=ON \
    $THIS_DIR \
    $RUNNER_TEMP/webkit-release &&
    cmake --build $RUNNER_TEMP/webkit-release --config Release --target jsc
mkdir -p $RUNNER_TEMP/bun-webkit/lib $RUNNER_TEMP/bun-webkit/include $RUNNER_TEMP/bun-webkit/include/JavaScriptCore $RUNNER_TEMP/bun-webkit/include/wtf $RUNNER_TEMP/bun-webkit/include/bmalloc
cp $RUNNER_TEMP/webkit-release/lib/* $RUNNER_TEMP/bun-webkit/lib
cp -r $RUNNER_TEMP/webkit-release/cmakeconfig.h $RUNNER_TEMP/bun-webkit/include/cmakeconfig.h
echo "#define BUN_WEBKIT_VERSION \"$GITHUB_SHA\"" >>$RUNNER_TEMP/bun-webkit/include/cmakeconfig.h
cp -r $RUNNER_TEMP/webkit-release/WTF/Headers/wtf $RUNNER_TEMP/bun-webkit/include
cp -r $RUNNER_TEMP/webkit-release/ICU/Headers/* $RUNNER_TEMP/bun-webkit/include
cp -r $RUNNER_TEMP/webkit-release/bmalloc/Headers/bmalloc $RUNNER_TEMP/bun-webkit/include
cp $RUNNER_TEMP/webkit-release/JavaScriptCore/Headers/JavaScriptCore/* $RUNNER_TEMP/bun-webkit/include/JavaScriptCore
cp $RUNNER_TEMP/webkit-release/JavaScriptCore/PrivateHeaders/JavaScriptCore/* $RUNNER_TEMP/bun-webkit/include/JavaScriptCore
mkdir -p $RUNNER_TEMP/bun-webkit/Source/JavaScriptCore
cp -r $THIS_DIR/Source/JavaScriptCore/Scripts $RUNNER_TEMP/bun-webkit/Source/JavaScriptCore
cp $THIS_DIR/Source/JavaScriptCore/create_hash_table $RUNNER_TEMP/bun-webkit/Source/JavaScriptCore
echo "{ \"name\": \"$PACKAGE_JSON_LABEL\", \"version\": \"0.0.1-$GITHUB_SHA\", \"os\": [\"darwin\"], \"cpu\": [\"$PACKAGE_JSON_ARCH\"], \"repository\": \"https://github.com/$GITHUB_REPOSITORY\" }" >$RUNNER_TEMP/bun-webkit/package.json
cd $RUNNER_TEMP
tar -czf bun-webkit.tar.gz bun-webkit
