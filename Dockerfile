FROM alpine:latest as base

ARG GLIBC_VERSION='2.27-r0'

RUN apk update
RUN apk add --no-cache cpio \
    build-base \
    "clang<14.0.0" \
    "clang-dev<14.0.0" \
    "clang-static<14.0.0" \
    cmake \
    curl \
    curl \
    file \
    g++ \
    gcc \
    git \
    gnupg \
    libc-dev \
    libgcc \
    libstdc++ \
    libxml2 \
    libxml2-dev \
    linux-headers \
    lld \
    lld-dev \
    lld-static \
    llvm13-dev \
    llvm13-libs \
    llvm13-static \
    make \
    ninja \
    openssl \
    openssl-dev \
    perl \
    python3 \
    rsync \
    ruby \
    unzip \
    xz \
    zlib \
    zlib-dev \
    icu-static \
    icu-dev


RUN curl -LO https://alpine-pkgs.sgerrand.com/sgerrand.rsa.pub && \
    curl -LO https://github.com/sgerrand/alpine-pkg-glibc/releases/download/${GLIBC_VERSION}/glibc-${GLIBC_VERSION}.apk && \
    cp sgerrand.rsa.pub /etc/apk/keys && \
    apk --no-cache add glibc-${GLIBC_VERSION}.apk 

ENV CXX=clang++
ENV CC=clang
ENV LDFLAGS='-L/usr/include -L/usr/include/llvm13'
ENV CXXFLAGS=" -I/usr/include -I/usr/include/llvm13"
ENV PATH="/usr/glibc-compat/bin/:/usr/bin:/usr/local/bin:/zig/bin:$PATH"


ENV WEBKIT_OUT_DIR=/webkitbuild
RUN mkdir -p /output/lib /output/include /output/include/JavaScriptCore /output/include/wtf /output/include/bmalloc

RUN cp /usr/lib/libicu* /output/lib && \
    mkdir -p /output/include/unicode && \
    cp -r /usr/include/unicode/* /output/include/unicode

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
    cp -r $WEBKIT_OUT_DIR/bmalloc/Headers/bmalloc/ /output/include; echo "";


FROM scratch as artifact

COPY --from=base /output /

