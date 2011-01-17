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

namespace WebKit {

// A proxy video frame interface to communicate frame data between chromium
// and WebKit.
class WebVideoFrame {
public:
    enum Format {
        FormatInvalid,
        FormatRGB555,
        FormatRGB565,
        FormatRGB24,
        FormatRGB32,
        FormatRGBA,
        FormatYV12,
        FormatYV16,
        FormatNV12,
        FormatEmpty,
        FormatASCII,
    };

    enum SurfaceType {
        SurfaceTypeSystemMemory,
        SurfaceTypeTexture,
    };

    virtual SurfaceType surfaceType() const = 0;
    virtual Format format() const = 0;
    virtual unsigned width() const = 0;
    virtual unsigned height() const = 0;
    virtual unsigned planes() const = 0;
    virtual int stride(unsigned plane) const = 0;
    virtual const void* data(unsigned plane) const = 0;
    virtual unsigned texture(unsigned plane) const = 0;
};

} // namespace WebKit

#endif
