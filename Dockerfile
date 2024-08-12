ARG MARCH_FLAG=""
ARG WEBKIT_RELEASE_TYPE=Release
ARG CPU=native
ARG LTO_FLAG="-flto=full -fwhole-program-vtables -fforce-emit-vtables "
ARG LLVM_VERSION="16"
ARG DEFAULT_CFLAGS="-mno-omit-leaf-frame-pointer -fno-omit-frame-pointer -ffunction-sections -fdata-sections -faddrsig -fno-unwind-tables -fno-asynchronous-unwind-tables -DU_STATIC_IMPLEMENTATION=1 "
ARG DEBIAN_VERSION="bullseye"

FROM bitnami/minideb:${DEBIAN_VERSION} as base

ARG MARCH_FLAG
ARG WEBKIT_RELEASE_TYPE
ARG CPU
ARG LTO_FLAG
ARG LLVM_VERSION
ARG DEFAULT_CFLAGS

RUN install_packages ca-certificates curl wget lsb-release software-properties-common gnupg gnupg1 gnupg2

RUN wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh ${LLVM_VERSION}

RUN install_packages \
    curl \
    file \
    git \
    gnupg \
    libc-dev \
    libxml2 \
    libxml2-dev \
    make \
    ninja-build \
    perl \
    python3 \
    rsync \
    ruby \
    unzip \
    bash tar gzip \
    clang-${LLVM_VERSION} \
    lld-${LLVM_VERSION} \
    libc++-${LLVM_VERSION}-dev \
    libc++abi-${LLVM_VERSION}-dev \
    lldb-${LLVM_VERSION} \
    pkg-config \
    ruby-dev \
    llvm-${LLVM_VERSION}-runtime \
    llvm-${LLVM_VERSION}-dev

RUN  for f in /usr/lib/llvm-${LLVM_VERSION}/bin/*; do ln -sf "$f" /usr/bin; done \
    && ln -sf /usr/bin/clang-${LLVM_VERSION} /usr/bin/clang \
    && ln -sf /usr/bin/clang++-${LLVM_VERSION} /usr/bin/clang++ \
    && ln -sf /usr/bin/lld-${LLVM_VERSION} /usr/bin/lld \
    && ln -sf /usr/bin/lldb-${LLVM_VERSION} /usr/bin/lldb \
    && ln -sf /usr/bin/clangd-${LLVM_VERSION} /usr/bin/clangd \
    && ln -sf /usr/bin/llvm-ar-${LLVM_VERSION} /usr/bin/llvm-ar \
    && ln -sf /usr/bin/ld.lld /usr/bin/ld \
    && ln -sf /usr/bin/clang /usr/bin/cc \
    && ln -sf /usr/bin/clang /usr/bin/c89 \
    && ln -sf /usr/bin/clang /usr/bin/c99 \
    && ln -sf /usr/bin/clang++ /usr/bin/c++ \
    && ln -sf /usr/bin/clang++ /usr/bin/g++ \
    && ln -sf /usr/bin/llvm-ar /usr/bin/ar \
    && ln -sf /usr/bin/clang /usr/bin/gcc 

ENV CC clang-${LLVM_VERSION}
ENV CXX clang++-${LLVM_VERSION}
ENV AR llvm-ar-${LLVM_VERSION}
ENV RANLIB llvm-ranlib-${LLVM_VERSION}
ENV LD lld-${LLVM_VERSION}
ENV LTO_FLAG="${LTO_FLAG}"

ENV WEBKIT_OUT_DIR=/webkitbuild
RUN mkdir -p /output/lib /output/include /output/include/JavaScriptCore /output/include/wtf /output/include/bmalloc /output/include/unicode

# Debian repos may not have the latest ICU version, so we ensure build reliability by downloading
# the exact version we need. Unfortunately, aarch64 is not pre-built so we have to build it from source.
ADD https://github.com/unicode-org/icu/releases/download/release-75-1/icu4c-75_1-src.tgz /icu.tgz
RUN --mount=type=tmpfs,target=/icu \ 
    export CFLAGS="${DEFAULT_CFLAGS} $CFLAGS -O3 -std=c17 $LTO_FLAG" && \
    export CXXFLAGS="${DEFAULT_CFLAGS} $CXXFLAGS -O3 -std=c++20 -fno-exceptions $LTO_FLAG -fno-c++-static-destructors " && \
    export LDFLAGS="-fuse-ld=lld " && \
    cd /icu && \
    tar -xf /icu.tgz --strip-components=1 && \
    rm /icu.tgz && \
    cd source && \
    ./configure  --enable-static --disable-shared --with-data-packaging=static --disable-samples --disable-debug --disable-tests && \ 
    make -j$(nproc) && \
    make install && cp -r /icu/source/lib/* /output/lib && cp -r /icu/source/i18n/unicode/* /icu/source/common/unicode/* /output/include/unicode

# WebKit has a minimum cmake version of 3.20
ADD https://github.com/Kitware/CMake/releases/download/v3.29.6/cmake-3.29.6.tar.gz /cmake.tar.gz
RUN --mount=type=tmpfs,target=/cmakebuild install_packages libssl-dev && tar -xf /cmake.tar.gz -C /cmakebuild && \
    cd /cmakebuild/cmake-3.29.6 && \
    ./configure && \
    make -j$(nproc) && \
    make install && \
    rm -rf /cmakebuild/cmake-3.29.6 /cmake.tar.gz


COPY . /webkit
WORKDIR /webkit

ENV CPU=${CPU}
ENV MARCH_FLAG=${MARCH_FLAG}


RUN --mount=type=tmpfs,target=/webkitbuild \
    export CFLAGS="${DEFAULT_CFLAGS} $CFLAGS $LTO_FLAG -ffile-prefix-map=/webkit/Source=src/bun.js/WebKit/Source  -ffile-prefix-map=/webkitbuild/=. " && \
    export CXXFLAGS="${DEFAULT_CFLAGS} $CXXFLAGS $LTO_FLAG -fno-c++-static-destructors -ffile-prefix-map=/webkit/Source=src/bun.js/WebKit/Source -ffile-prefix-map=/webkitbuild/=. " && \
    export LDFLAGS="-fuse-ld=lld $LDFLAGS " && \
    cd /webkitbuild && \
    cmake \
    -DPORT="JSCOnly" \
    -DENABLE_STATIC_JSC=ON \
    -DENABLE_BUN_SKIP_FAILING_ASSERTIONS=ON \
    -DCMAKE_BUILD_TYPE=$WEBKIT_RELEASE_TYPE \
    -DUSE_THIN_ARCHIVES=OFF \
    -DUSE_BUN_JSC_ADDITIONS=ON \
    -DENABLE_FTL_JIT=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DALLOW_LINE_AND_COLUMN_NUMBER_IN_BUILTINS=ON \
    -DENABLE_SINGLE_THREADED_VM_ENTRY_SCOPE=ON \
    -DENABLE_REMOTE_INSPECTOR=ON \
    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" \
    -DCMAKE_AR=$(which llvm-ar) \
    -DCMAKE_RANLIB=$(which llvm-ranlib) \
    -DCMAKE_C_FLAGS="$CFLAGS" \
    -DCMAKE_CXX_FLAGS="$CXXFLAGS" \
    -DICU_ROOT=/icu \
    -G Ninja \
    /webkit && \
    cd /webkitbuild && \
    cmake --build /webkitbuild --config $WEBKIT_RELEASE_TYPE --target "jsc" && \
    cp -r $WEBKIT_OUT_DIR/lib/*.a /output/lib && \
    cp $WEBKIT_OUT_DIR/*.h /output/include && \
    cp -r $WEBKIT_OUT_DIR/bin /output/bin && \
    cp $WEBKIT_OUT_DIR/*.json /output && \
    find $WEBKIT_OUT_DIR/JavaScriptCore/Headers/JavaScriptCore/ -name "*.h" -exec cp {} /output/include/JavaScriptCore/ \; && \
    find $WEBKIT_OUT_DIR/JavaScriptCore/PrivateHeaders/JavaScriptCore/ -name "*.h" -exec cp {} /output/include/JavaScriptCore/ \; && \
    cp -r $WEBKIT_OUT_DIR/WTF/Headers/wtf/ /output/include && \
    cp -r $WEBKIT_OUT_DIR/bmalloc/Headers/bmalloc/ /output/include && \
    mkdir -p /output/Source/JavaScriptCore && \
    cp -r /webkit/Source/JavaScriptCore/Scripts /output/Source/JavaScriptCore && \
    cp /webkit/Source/JavaScriptCore/create_hash_table /output/Source/JavaScriptCore && \
    echo "";

FROM scratch as artifact

COPY --from=base /output /
