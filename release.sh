#!/bin/bash

set -euxo pipefail

export DOCKER_BUILDKIT=1

export BUILDKIT_ARCH=$(uname -m)
export ARCH=${BUILDKIT_ARCH}
export LTO_FLAG="${LTO_FLAG:-""}"
if [ "$BUILDKIT_ARCH" == "amd64" ]; then
    export BUILDKIT_ARCH="amd64"
    export ARCH=x64
fi

if [ "$BUILDKIT_ARCH" == "x86_64" ]; then
    export BUILDKIT_ARCH="amd64"
    export ARCH=x64
fi

if [ "$BUILDKIT_ARCH" == "arm64" ]; then
    export BUILDKIT_ARCH="arm64"
    export ARCH=aarch64
fi

if [ "$BUILDKIT_ARCH" == "aarch64" ]; then
    export BUILDKIT_ARCH="arm64"
    export ARCH=aarch64
fi

if [ "$BUILDKIT_ARCH" == "armv7l" ]; then
    echo "Unsupported platform: $BUILDKIT_ARCH"
    exit 1
fi

export WEBKIT_RELEASE_TYPE=${WEBKIT_RELEASE_TYPE:-"Release"}

export CONTAINER_NAME=bun-webkit-linux-$BUILDKIT_ARCH

if [ "$WEBKIT_RELEASE_TYPE" == "relwithdebuginfo" ]; then
    CONTAINER_NAME=bun-webkit-linux-$BUILDKIT_ARCH-dbg
fi

# If LTO is not enabled, we still need to be able to do a local release build.
# We use bullseye for CI to support older versions of glibc, but that requires libc++.
if [[ "$WEBKIT_RELEASE_TYPE" == "Debug" || "$LTO_FLAG" == "" ]]; then
    export DEBIAN_VERSION="bookworm"
else
    export DEBIAN_VERSION="bullseye"
fi

mkdir -p $temp
rm -rf $temp/bun-webkit

docker buildx build -f Dockerfile -t $CONTAINER_NAME --build-arg LTO_FLAG="$LTO_FLAG" --build-arg WEBKIT_RELEASE_TYPE=$WEBKIT_RELEASE_TYPE --build-arg DEBIAN_VERSION=$DEBIAN_VERSION --progress=plain --platform=linux/$BUILDKIT_ARCH --target=artifact --output type=local,dest=$temp/bun-webkit .
