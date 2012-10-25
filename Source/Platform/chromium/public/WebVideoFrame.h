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

#ifndef WebVideoFrame_h
#define WebVideoFrame_h

#include "WebRect.h"
#include "WebSize.h"

namespace WebKit {

// A proxy video frame interface to communicate frame data between chromium
// and WebKit.
// Keep in sync with chromium's media::VideoFrame::Format.
class WebVideoFrame {
public:
    enum {
        rgbPlane = 0,
        numRGBPlanes = 1
    };
    enum {
        yPlane = 0,
        uPlane = 1,
        vPlane = 2,
        numYUVPlanes = 3
    };
    enum { maxPlanes = 3 };

    enum Format {
        FormatInvalid = 0,
        FormatRGB32 = 4,
        FormatYV12 = 6,
        FormatYV16 = 7,
        FormatEmpty = 9,
        FormatI420 = 11,
        FormatNativeTexture = 12,
    };

    virtual ~WebVideoFrame() { }
    virtual Format format() const { return FormatInvalid; }
    virtual unsigned width() const { return 0; }
    virtual unsigned height() const { return 0; }
    virtual unsigned planes() const { return 0; }
    virtual int stride(unsigned plane) const { return 0; }
    virtual const void* data(unsigned plane) const { return 0; }
    virtual unsigned textureId() const { return 0; }
    virtual unsigned textureTarget() const { return 0; }
    virtual WebKit::WebRect visibleRect() const { return WebKit::WebRect(); }
    virtual WebKit::WebSize textureSize() const { return WebKit::WebSize(); }
};

} // namespace WebKit

#endif
