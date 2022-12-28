ARG MARCH_FLAG=""
ARG WEBKIT_RELEASE_TYPE=Release
ARG CPU=native
ARG LTO_FLAG="-flto='full'"

FROM bitnami/minideb:bullseye as base

ARG MARCH_FLAG
ARG WEBKIT_RELEASE_TYPE
ARG CPU
ARG LTO_FLAG
RUN install_packages ca-certificates curl wget lsb-release software-properties-common gnupg gnupg1 gnupg2

RUN wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh 15

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
    libicu-dev

ENV CXX=clang++-15
ENV CC=clang-15


ENV WEBKIT_OUT_DIR=/webkitbuild
RUN mkdir -p /output/lib /output/include /output/include/JavaScriptCore /output/include/wtf /output/include/bmalloc

RUN cp -r /usr/lib/$(uname -m)-linux-gnu/libicu* /output/lib

COPY . /webkit
WORKDIR /webkit



ENV CPU=${CPU}
ENV MARCH_FLAG=${MARCH_FLAG}
ENV LTO_FLAG=${LTO_FLAG}


RUN --mount=type=tmpfs,target=/webkitbuild \
    export CFLAGS="$CFLAGS $LTO_FLAG -ffat-lto-objects $MARCH_FLAG -mtune=$CPU" && \
    export CXXFLAGS="$CXXFLAGS $LTO_FLAG -ffat-lto-objects $MARCH_FLAG -mtune=$CPU" && \
    cd /webkitbuild && \
    cmake \
    -DPORT="JSCOnly" \
    -DENABLE_STATIC_JSC=ON \
    -DCMAKE_BUILD_TYPE=$WEBKIT_RELEASE_TYPE \
    -DUSE_THIN_ARCHIVES=OFF \
    -DENABLE_FTL_JIT=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DENABLE_SINGLE_THREADED_VM_ENTRY_SCOPE=ON \
    -G Ninja \ 
    -DCMAKE_CXX_COMPILER=$(which clang++-15) \
    -DCMAKE_C_COMPILER=$(which clang-15) \
    -DCMAKE_C_FLAGS="$CFLAGS" \
    -DCMAKE_CXX_FLAGS="$CXXFLAGS" \
    /webkit && \
    cd /webkitbuild && \
    CFLAGS="$CFLAGS" CXXFLAGS="$CXXFLAGS" cmake --build /webkitbuild --config $WEBKIT_RELEASE_TYPE --target "jsc" && \
    cp -r $WEBKIT_OUT_DIR/lib/*.a /output/lib && \
    cp $WEBKIT_OUT_DIR/*.h /output/include && \
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

