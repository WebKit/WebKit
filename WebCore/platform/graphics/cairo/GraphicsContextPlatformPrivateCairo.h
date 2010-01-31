/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Brent Fulgham <bfulgham@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "GraphicsContext.h"

#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <wtf/MathExtras.h>

#if PLATFORM(GTK)
#include <pango/pango.h>
typedef struct _GdkExposeEvent GdkExposeEvent;
#elif PLATFORM(WIN)
#include <cairo-win32.h>
#endif

namespace WebCore {

class GraphicsContextPlatformPrivate {
public:
    GraphicsContextPlatformPrivate()
        : cr(0)
#if PLATFORM(GTK)
        , expose(0)
#elif PLATFORM(WIN)
        // NOTE:  These may note be needed: review and remove once Cairo implementation is complete
        , m_hdc(0)
        , m_transparencyCount(0)
        , m_shouldIncludeChildWindows(false)
#endif
    {
    }

    ~GraphicsContextPlatformPrivate()
    {
        cairo_destroy(cr);
    }

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
    void concatCTM(const TransformationMatrix&);
    void beginTransparencyLayer() { m_transparencyCount++; }
    void endTransparencyLayer() { m_transparencyCount--; }
    void syncContext(PlatformGraphicsContext* cr);
#else
    // On everything else, we do nothing.
    void save() {}
    void restore() {}
    void flush() {}
    void clip(const FloatRect&) {}
    void clip(const Path&) {}
    void scale(const FloatSize&) {}
    void rotate(float) {}
    void translate(float, float) {}
    void concatCTM(const AffineTransform&) {}
    void concatCTM(const TransformationMatrix&) {}
    void beginTransparencyLayer() {}
    void endTransparencyLayer() {}
    void syncContext(PlatformGraphicsContext* cr) {}
#endif

    cairo_t* cr;
    Vector<float> layers;

#if PLATFORM(GTK)
    GdkEventExpose* expose;
#elif PLATFORM(WIN)
    HDC m_hdc;
    unsigned m_transparencyCount;
    bool m_shouldIncludeChildWindows;
#endif
};

} // namespace WebCore

