//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// NativeBufferImageSiblingAndroid.cpp: Implements the NativeBufferImageSiblingAndroid class

#include "libANGLE/renderer/gl/egl/android/NativeBufferImageSiblingAndroid.h"

#include "common/android_util.h"

namespace rx
{
NativeBufferImageSiblingAndroid::NativeBufferImageSiblingAndroid(EGLClientBuffer buffer)
    : mBuffer(buffer), mFormat(GL_NONE), mYUV(false)
{}

NativeBufferImageSiblingAndroid::~NativeBufferImageSiblingAndroid() {}

egl::Error NativeBufferImageSiblingAndroid::initialize(const egl::Display *display)
{
    int pixelFormat = 0;
    uint64_t usage  = 0;
    angle::android::GetANativeWindowBufferProperties(
        angle::android::ClientBufferToANativeWindowBuffer(mBuffer), &mSize.width, &mSize.height,
        &mSize.depth, &pixelFormat, &usage);
    mFormat = gl::Format(angle::android::NativePixelFormatToGLInternalFormat(pixelFormat));
    mYUV    = angle::android::NativePixelFormatIsYUV(pixelFormat);
    mHasProtectedContent = false;

    return egl::NoError();
}

gl::Format NativeBufferImageSiblingAndroid::getFormat() const
{
    return mFormat;
}

bool NativeBufferImageSiblingAndroid::isRenderable(const gl::Context *context) const
{
    return true;
}

bool NativeBufferImageSiblingAndroid::isTexturable(const gl::Context *context) const
{
    return true;
}

bool NativeBufferImageSiblingAndroid::isYUV() const
{
    return mYUV;
}

bool NativeBufferImageSiblingAndroid::hasProtectedContent() const
{
    return mHasProtectedContent;
}

gl::Extents NativeBufferImageSiblingAndroid::getSize() const
{
    return mSize;
}

size_t NativeBufferImageSiblingAndroid::getSamples() const
{
    return 0;
}

EGLClientBuffer NativeBufferImageSiblingAndroid::getBuffer() const
{
    return mBuffer;
}

}  // namespace rx
