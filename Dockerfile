ARG MARCH_FLAG=""
ARG WEBKIT_RELEASE_TYPE=Release
ARG CPU=native
ARG LTO_FLAG="-flto='full'"
ARG LLVM_VERSION="16"

FROM bitnami/minideb:bullseye as base

ARG MARCH_FLAG
ARG WEBKIT_RELEASE_TYPE
ARG CPU
ARG LTO_FLAG
ARG LLVM_VERSION

RUN install_packages ca-certificates curl wget lsb-release software-properties-common gnupg gnupg1 gnupg2

RUN wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh ${LLVM_VERSION}

RUN install_packages \
    cmake \
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
    ruby-dev

RUN for f in /usr/lib/llvm-${LLVM_VERSION}/bin/*; do ln -sf "$f" /usr/bin; done && \
    ln -sf clang /usr/bin/cc && \
    ln -sf clang /usr/bin/c89 && \
    ln -sf clang /usr/bin/c99 && \
    ln -sf clang++ /usr/bin/c++ && \
    ln -sf clang++ /usr/bin/g++ && \
    ln -sf llvm-ar /usr/bin/ar && \ 
    ln -sf llvm-ranlib /usr/bin/ranlib && \
    ln -sf ld.lld /usr/bin/ld

# Debian repos may not have the latest ICU version, so we ensure build reliability by downloading
# the exact version we need. Unfortunately, aarch64 is not pre-built so we have to build it from source.
RUN mkdir /icu && \
    cd /icu && \
    wget https://github.com/unicode-org/icu/releases/download/release-75-1/icu4c-75_1-src.tgz -O icu.tgz && \
    tar -xf icu.tgz --strip-components=1 && \
    rm icu.tgz && \
    cd source && \
    ./runConfigureICU Linux/clang --enable-static --disable-shared --with-data-packaging=static --disable-samples --disable-debug --enable-release && \
    make -j$(nproc) && \
    make install

ENV WEBKIT_OUT_DIR=/webkitbuild
RUN mkdir -p /output/lib /output/include /output/include/JavaScriptCore /output/include/wtf /output/include/bmalloc

RUN cp -r /icu/source/lib/* /output/lib

COPY . /webkit
WORKDIR /webkit

ENV CPU=${CPU}
ENV MARCH_FLAG=${MARCH_FLAG}
ENV LTO_FLAG=${LTO_FLAG}

RUN --mount=type=tmpfs,target=/webkitbuild \
    export CFLAGS="$CFLAGS -mno-omit-leaf-frame-pointer -fno-omit-frame-pointer $LTO_FLAG" && \
    export CXXFLAGS="$CXXFLAGS -mno-omit-leaf-frame-pointer -fno-omit-frame-pointer $LTO_FLAG" && \
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
    rm -rf /output/include/unicode && \
    cp -r /usr/include/unicode /output/include/unicode && \
    echo "";

FROM scratch as artifact

COPY --from=base /output /
