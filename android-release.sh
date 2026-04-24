#!/bin/bash

set -euxo pipefail

export DOCKER_BUILDKIT=1

# Android is always cross-compiled (NDK only ships linux-x86_64 prebuilts),
# so the target arch comes from ANDROID_ARCH, not the host. Run this on an
# x64 host regardless of target.
export ANDROID_ARCH="${ANDROID_ARCH:-aarch64}"
export ANDROID_API="${ANDROID_API:-28}"
export WEBKIT_RELEASE_TYPE="${WEBKIT_RELEASE_TYPE:-Release}"
export LTO_FLAG="${LTO_FLAG:-}"

if [ -z "${MARCH_FLAG:-}" ]; then
    if [ "$ANDROID_ARCH" == "aarch64" ]; then
        export MARCH_FLAG="-march=armv8-a+crc -mtune=cortex-a78"
    else
        export MARCH_FLAG="-march=x86-64-v2"
    fi
fi

export CONTAINER_NAME="bun-webkit-linux-android-${ANDROID_ARCH}"
if [ "$WEBKIT_RELEASE_TYPE" == "Debug" ]; then
    CONTAINER_NAME="${CONTAINER_NAME}-debug"
fi

mkdir -p "$temp"
rm -rf "$temp/bun-webkit"

docker buildx build -f Dockerfile.android -t "$CONTAINER_NAME" \
    --build-arg ANDROID_ARCH="$ANDROID_ARCH" \
    --build-arg ANDROID_API="$ANDROID_API" \
    --build-arg LTO_FLAG="$LTO_FLAG" \
    --build-arg MARCH_FLAG="$MARCH_FLAG" \
    --build-arg WEBKIT_RELEASE_TYPE="$WEBKIT_RELEASE_TYPE" \
    --progress=plain \
    --platform=linux/amd64 \
    --target=artifact \
    --output type=local,dest="$temp/bun-webkit" .
