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

#if USE(CAIRO)

#include "PlatformContextCairo.h"

namespace WebCore {

class GraphicsContextPlatformPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
#if PLATFORM(WIN)
    // On Windows, we need to update the HDC for form controls to draw in the right place.
    GraphicsContextPlatformPrivate(cairo_t*);
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
#else
    // On everything else, we do nothing.
    GraphicsContextPlatformPrivate(cairo_t*) { }
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
#endif

#if PLATFORM(WIN) || (PLATFORM(GTK) && OS(WINDOWS))
    // NOTE: These may note be needed: review and remove once Cairo implementation is complete
    HDC m_hdc { 0 };
    bool m_shouldIncludeChildWindows { false };
#endif
};

} // namespace WebCore

#endif // USE(CAIRO)
