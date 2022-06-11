# BuildKit is required to build this
# Enable via DOCKER_BUILDKIT=1 
FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --no-install-recommends -y wget gnupg2 curl lsb-release wget software-properties-common

RUN wget https://apt.llvm.org/llvm.sh --no-check-certificate
RUN chmod +x llvm.sh
RUN ./llvm.sh 13

# Use the same version of LLVM/clang used to build Zig
# This prevents the allocation failure
RUN apt-get update && apt-get install --no-install-recommends -y \
    bc \
    build-essential \
    ca-certificates \
    clang-13 \
    clang-format-13 \
    cmake \
    cpio \
    curl \
    file \
    g++ \
    gcc \
    git \
    gnupg2 \
    libicu66 \ 
    libc++-13-dev \
    libc++abi-13-dev \
    libclang-13-dev \
    liblld-13-dev \
    libssl-dev \
    lld-13 \
    make \
    ninja-build \
    perl \
    python2 \
    rsync \
    ruby \
    software-properties-common \
    unzip \
    wget

RUN update-alternatives --install /usr/bin/ld ld /usr/bin/lld-13 90 && \
    update-alternatives --install /usr/bin/cc cc /usr/bin/clang-13 90 && \
    update-alternatives --install /usr/bin/cpp cpp /usr/bin/clang++-13 90 && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-13 90


ENV WEBKIT_OUT_DIR=/webkitbuild

ENV CC=clang-13 
ENV CXX=clang++-13

COPY . /webkit
WORKDIR /webkit

RUN mkdir -p /output/lib /output/include /output/include/JavaScriptCore /output/include/wtf /output/include/bmalloc

ARG WEBKIT_RELEASE_TYPE=Release

# | Explanation                                                                                    |                                                Flag 
# -----------------------------------------------------------------------------------------------------------------------------------------------------
# | Use the "JSCOnly" WebKit port                                                                  |                                  -DPORT="JSCOnly" |
# | Build JavaScriptCore as a static library                                                       |                            -DENABLE_STATIC_JSC=ON |
# | Build for release mode but include debug symbols                                               |               -DCMAKE_BUILD_TYPE=relwithdebuginfo |            
# | The .a files shouldn't be symlinks to UnifiedSource.cpp files or else you can't move the files |                           -DUSE_THIN_ARCHIVES=OFF |        
# | Enable the FTL Just-In-Time Compiler                                                           |                               -DENABLE_FTL_JIT=ON |        
# -----------------------------------------------------------------------------------------------------------------------------------------------------

# Using tmpfs this way makes it compile 2x faster
# But means everything has to be one "RUN" statement
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
    CFLAGS="$CFLAGS -ffat-lto-objects" CXXFLAGS="$CXXFLAGS -ffat-lto-objects" cmake --build /webkitbuild --config $WEBKIT_RELEASE_TYPE -- "jsc" -j$(nproc) && \
    cp -r $WEBKIT_OUT_DIR/lib/*.a /output/lib && \
    cp $WEBKIT_OUT_DIR/*.h /output/include && \
    find $WEBKIT_OUT_DIR/JavaScriptCore/Headers/JavaScriptCore/ -name "*.h" -exec cp {} /output/include/JavaScriptCore/ \; && \
    find $WEBKIT_OUT_DIR/JavaScriptCore/PrivateHeaders/JavaScriptCore/ -name "*.h" -exec cp {} /output/include/JavaScriptCore/ \; && \
    cp -r $WEBKIT_OUT_DIR/WTF/Headers/wtf/ /output/include && \
    cp -r $WEBKIT_OUT_DIR/bmalloc/Headers/bmalloc/ /output/include; echo "";
