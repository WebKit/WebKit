FROM bitnami/minideb:bullseye as base

RUN install_packages ca-certificates curl wget lsb-release software-properties-common gnupg gnupg1 gnupg2

RUN wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh 13

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

ENV CXX=clang++-13
ENV CC=clang-13


ENV WEBKIT_OUT_DIR=/webkitbuild
RUN mkdir -p /output/lib /output/include /output/include/JavaScriptCore /output/include/wtf /output/include/bmalloc

RUN cp -r /usr/lib/$(uname -m)-linux-gnu/libicu* /output/lib

COPY . /webkit
WORKDIR /webkit


ARG WEBKIT_RELEASE_TYPE=Release

RUN --mount=type=tmpfs,target=/webkitbuild \
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
    -DCMAKE_CXX_COMPILER=$(which clang++-13) \
    -DCMAKE_C_COMPILER=$(which clang-13) \
    /webkit && \
    cd /webkitbuild && \
    CFLAGS="$CFLAGS -flto -ffat-lto-objects" CXXFLAGS="$CXXFLAGS -flto -ffat-lto-objects" cmake --build /webkitbuild --config $WEBKIT_RELEASE_TYPE --target "jsc" && \
    cp -r $WEBKIT_OUT_DIR/lib/*.a /output/lib && \
    cp $WEBKIT_OUT_DIR/*.h /output/include && \
    find $WEBKIT_OUT_DIR/JavaScriptCore/Headers/JavaScriptCore/ -name "*.h" -exec cp {} /output/include/JavaScriptCore/ \; && \
    find $WEBKIT_OUT_DIR/JavaScriptCore/PrivateHeaders/JavaScriptCore/ -name "*.h" -exec cp {} /output/include/JavaScriptCore/ \; && \
    cp -r $WEBKIT_OUT_DIR/WTF/Headers/wtf/ /output/include && \
    cp -r $WEBKIT_OUT_DIR/bmalloc/Headers/bmalloc/ /output/include && \
    echo "";



FROM scratch as artifact

COPY --from=base /output /

