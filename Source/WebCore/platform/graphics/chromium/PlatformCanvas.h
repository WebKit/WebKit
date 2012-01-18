/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformCanvas_h
#define PlatformCanvas_h

#include "IntSize.h"
#include <stdint.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

#if USE(CG)
#include <CoreGraphics/CGColorSpace.h>
#include <CoreGraphics/CGContext.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/RetainPtr.h>
#endif

#if USE(SKIA)
class SkBitmap;
class SkCanvas;
#endif

namespace WebCore {

class GraphicsContext;

#if USE(SKIA)
class PlatformContextSkia;
#endif

// A 2D buffer of pixels with an associated GraphicsContext.
class PlatformCanvas {
    WTF_MAKE_NONCOPYABLE(PlatformCanvas);
public:
    PlatformCanvas();
    ~PlatformCanvas();

    // Scoped lock class to get temporary access to this canvas's pixels.
    class AutoLocker {
        WTF_MAKE_NONCOPYABLE(AutoLocker);
    public:
        explicit AutoLocker(PlatformCanvas*);
        ~AutoLocker();

        const uint8_t* pixels() const { return m_pixels; }
    private:
        PlatformCanvas* m_canvas;
#if USE(SKIA)
        const SkBitmap* m_bitmap;
#endif
        uint8_t* m_pixels;
    };

    // Scoped lock class to get temporary access to paint into this canvas.
    class Painter {
        WTF_MAKE_NONCOPYABLE(Painter);
    public:
        enum TextOption { GrayscaleText, SubpixelText };

        Painter(PlatformCanvas*, TextOption);
        // Destructor restores canvas context to pre-construction state.
        ~Painter();

        GraphicsContext* context() const { return m_context.get(); }
        PlatformContextSkia* skiaContext() const { return m_skiaContext.get(); }
    private:
        OwnPtr<GraphicsContext> m_context;
#if USE(SKIA)
        OwnPtr<PlatformContextSkia> m_skiaContext;
#elif USE(CG)
        RetainPtr<CGColorSpaceRef> m_colorSpace;
        RetainPtr<CGContextRef> m_contextCG;
#endif
    };

    void resize(const IntSize&);
    IntSize size() const { return m_size; }

    void setOpaque(bool);
    bool opaque() const { return m_opaque; }

private:
    void createBackingCanvas();

#if USE(SKIA)
    OwnPtr<SkCanvas> m_skiaCanvas;
#elif USE(CG)
    OwnArrayPtr<uint8_t> m_pixelData;
#endif
    IntSize m_size;
    bool m_opaque;
};

} // namespace WebCore

#endif
