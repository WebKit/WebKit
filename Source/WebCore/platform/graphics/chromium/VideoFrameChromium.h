/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VideoFrameChromium_h
#define VideoFrameChromium_h

#include "IntSize.h"

namespace WebCore {

// A class that represents a video frame in chromium.
class VideoFrameChromium {
public:
    static const unsigned maxPlanes;
    static const unsigned numRGBPlanes;
    static const unsigned rgbPlane;
    static const unsigned numYUVPlanes;
    static const unsigned yPlane;
    static const unsigned uPlane;
    static const unsigned vPlane;

    // These enums must be kept in sync with WebKit::WebVideoFrame.
    enum Format {
        Invalid,
        RGB555,
        RGB565,
        RGB24,
        RGB32,
        RGBA,
        YV12,
        YV16,
        NV12,
        Empty,
        ASCII,
    };

    enum SurfaceType {
        TypeSystemMemory,
        TypeTexture,
    };

    virtual SurfaceType surfaceType() const = 0;
    virtual Format format() const = 0;
    virtual unsigned width() const = 0;
    virtual unsigned width(unsigned plane) const = 0;
    virtual unsigned height() const = 0;
    virtual unsigned height(unsigned plane) const = 0;
    virtual unsigned planes() const = 0;
    virtual int stride(unsigned plane) const = 0;
    virtual const void* data(unsigned plane) const = 0;
    virtual unsigned texture(unsigned plane) const = 0;
    virtual const IntSize requiredTextureSize(unsigned plane) const = 0;
    virtual bool hasPaddingBytes(unsigned plane) const = 0;
};

} // namespace WebCore

#endif
