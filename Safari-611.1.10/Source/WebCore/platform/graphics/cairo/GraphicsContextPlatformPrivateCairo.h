/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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

#pragma once

#include "GraphicsContext.h"

#if USE(CAIRO)

#include "PlatformContextCairo.h"
#include "RefPtrCairo.h"
#include <cairo.h>
#include <math.h>
#include <memory>
#include <stdio.h>

#if PLATFORM(WIN)
#include <cairo-win32.h>
#endif

namespace WebCore {

class GraphicsContextPlatformPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GraphicsContextPlatformPrivate(PlatformContextCairo& platformContext)
        : platformContext(platformContext)
    {
    }

    GraphicsContextPlatformPrivate(std::unique_ptr<PlatformContextCairo>&& platformContext)
        : ownedPlatformContext(WTFMove(platformContext))
        , platformContext(*ownedPlatformContext)
    {
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
    void setCTM(const AffineTransform&);
    void syncContext(cairo_t* cr);
#else
    // On everything else, we do nothing.
    void save() { }
    void restore() { }
    void flush() { }
    void clip(const FloatRect&) { }
    void clip(const Path&) { }
    void scale(const FloatSize&) { }
    void rotate(float) { }
    void translate(float, float) { }
    void concatCTM(const AffineTransform&) { }
    void setCTM(const AffineTransform&) { }
    void syncContext(cairo_t*) { }
#endif

    std::unique_ptr<PlatformContextCairo> ownedPlatformContext;
    PlatformContextCairo& platformContext;

#if PLATFORM(WIN) || (PLATFORM(GTK) && OS(WINDOWS))
    // NOTE: These may note be needed: review and remove once Cairo implementation is complete
    HDC m_hdc { 0 };
    bool m_shouldIncludeChildWindows { false };
#endif
};

} // namespace WebCore

#endif // USE(CAIRO)
