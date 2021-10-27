# BuildKit is required to build this
# Enable via DOCKER_BUILDKIT=1 
FROM ubuntu:latest

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --no-install-recommends -y wget gnupg2 curl lsb-release wget software-properties-common

RUN wget https://apt.llvm.org/llvm.sh --no-check-certificate
RUN chmod +x llvm.sh
RUN ./llvm.sh 12

# Use the same version of LLVM/clang used to build Zig
# This prevents the allocation failure
RUN apt-get update && apt-get install --no-install-recommends -y \
    bc \
    build-essential \
    ca-certificates \
    clang-12 \
    clang-format-12 \
    cmake \
    cpio \
    curl \
    file \
    g++ \
    gcc \
    git \
    gnupg2 \
    icu-devtools \ 
    libc++-12-dev \
    libc++abi-12-dev \
    libclang-12-dev \
    libicu-dev \
    liblld-12-dev \
    libssl-dev \
    lld-12 \
    make \
    ninja-build \
    perl \
    python \
    rsync \
    ruby \
    software-properties-common \
    unzip \
    wget

RUN update-alternatives --install /usr/bin/ld ld /usr/bin/lld-12 90 && \
    update-alternatives --install /usr/bin/cc cc /usr/bin/clang-12 90 && \
    update-alternatives --install /usr/bin/cpp cpp /usr/bin/clang++-12 90 && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-12 90


ENV WEBKIT_OUT_DIR=/webkitbuild

ENV CC=clang-12 
ENV CXX=clang++-12

COPY . /webkit
WORKDIR /webkit

RUN mkdir -p /output/lib /output/include /output/include/JavaScriptCore /output/include/wtf /output/include/bmalloc

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
    -DCMAKE_BUILD_TYPE=relwithdebuginfo \
    -DUSE_THIN_ARCHIVES=OFF \
    -DENABLE_FTL_JIT=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -G Ninja \ 
    -DCMAKE_CXX_COMPILER=$(which clang++-12) \
    -DCMAKE_C_COMPILER=$(which clang-12) \ 
    /webkit && \
    cd /webkitbuild && \
    CFLAGS="$CFLAGS -ffat-lto-objects" CXXFLAGS="$CXXFLAGS -ffat-lto-objects" cmake --build /webkitbuild --config relwithdebuginfo -- "jsc" -j$(nproc) && \
    cp -r $WEBKIT_OUT_DIR/lib/*.a /output/lib && \
    cp $WEBKIT_OUT_DIR/*.h /output/include && \
    find $WEBKIT_OUT_DIR/JavaScriptCore/Headers/JavaScriptCore/ -name "*.h" -exec cp {} /output/include/JavaScriptCore/ \; && \
    find $WEBKIT_OUT_DIR/JavaScriptCore/PrivateHeaders/JavaScriptCore/ -name "*.h" -exec cp {} /output/include/JavaScriptCore/ \; && \
    cp -r $WEBKIT_OUT_DIR/WTF/Headers/wtf/ /output/include && \
    cp -r $WEBKIT_OUT_DIR/bmalloc/Headers/bmalloc/ /output/include; echo "";

