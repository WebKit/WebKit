/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef GraphicsContextPlatformPrivateCG_h
#define GraphicsContextPlatformPrivateCG_h

#include <wtf/RetainPtr.h>
#include <CoreGraphics/CGContext.h>

namespace WebCore {

enum class GraphicsContextCGFlag : uint8_t {
    IsLayerCGContext        = 1 << 0,
    IsAcceleratedCGContext  = 1 << 1
};

class GraphicsContextPlatformPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GraphicsContextPlatformPrivate(RetainPtr<CGContextRef>&& cgContext, OptionSet<GraphicsContextCGFlag> flags = { })
        : m_cgContext(WTFMove(cgContext))
#if PLATFORM(WIN)
        , m_hdc(0)
#endif
        , m_userToDeviceTransformKnownToBeIdentity(false)
        , m_contextFlags(flags)
    {
    }

#if PLATFORM(COCOA)
    // These methods do nothing on Mac.
    void save() {}
    void restore() {}
    void flush() {}
    void clip(const FloatRect&) {}
    void clip(const Path&) {}
    void scale(const FloatSize&) {}
    void rotate(float) {}
    void translate(float, float) {}
    void concatCTM(const AffineTransform&) {}
    void setCTM(const AffineTransform&) {}
#endif

#if PLATFORM(WIN)
    // On Windows, we need to update the HDC for form controls to draw in the right place.
    void save();
    void restore();
    void flush();
    void clip(const FloatRect&);
    void clip(const Path&);
    void scale(const FloatSize&);
    void rotate(float);
    void translate(float, float);
    void concatCTM(const AffineTransform&);
    void setCTM(const AffineTransform&);

    HDC m_hdc;
#endif

    RetainPtr<CGContextRef> m_cgContext;
    bool m_userToDeviceTransformKnownToBeIdentity;
    OptionSet<GraphicsContextCGFlag> m_contextFlags;
};

}

#endif // GraphicsContextPlatformPrivateCG_h
