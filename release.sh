#!/bin/bash

set -euxo pipefail

export DOCKER_BUILDKIT=1

export BUILDKIT_ARCH=$(uname -m)
export ARCH=${BUILDKIT_ARCH}

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

temp=$(mktemp -d)

docker build . -t $CONTAINER_NAME --progress=plain --platform=linux/$BUILDKIT_ARCH

if $? -ne 0; then
    echo "Failed to build container"
    exit 1
fi

id=$(docker create $CONTAINER_NAME:latest)
docker cp $id:/output $temp/$CONTAINER_NAME
cd $temp && zip -r $CONTAINER_NAME.tar.gz $CONTAINER_NAME
docker rm -v $id

gh releases upload $TAG $temp/$CONTAINER_NAME.tar.gz --clobber --repo Jarred-Sumner/webkit
