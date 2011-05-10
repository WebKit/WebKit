/*
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef PlatformContextCairo_h
#define PlatformContextCairo_h

#include "ContextShadow.h"
#include "RefPtrCairo.h"

namespace WebCore {

// In Cairo image masking is immediate, so to emulate image clipping we must save masking
// details as part of the context state and apply them during platform restore.
class ImageMaskInformation {
public:
    void update(cairo_surface_t* maskSurface, const FloatRect& maskRect)
    {
        m_maskSurface = maskSurface;
        m_maskRect = maskRect;
    }

    bool isValid() const { return m_maskSurface; }
    cairo_surface_t* maskSurface() const { return m_maskSurface.get(); }
    const FloatRect& maskRect() const { return m_maskRect; }

private:
    RefPtr<cairo_surface_t> m_maskSurface;
    FloatRect m_maskRect;
};

// Much like PlatformContextSkia in the Skia port, this class holds information that
// would normally be private to GraphicsContext, except that we want to allow access
// to it in Font and Image code. This allows us to separate the concerns of Cairo-specific
// code from the platform-independent GraphicsContext.

class PlatformContextCairo {
    WTF_MAKE_NONCOPYABLE(PlatformContextCairo);
public:
    PlatformContextCairo(cairo_t*);

    cairo_t* cr() { return m_cr.get(); }
    void setCr(cairo_t* cr) { m_cr = cr; }

    void save();
    void restore();
    void pushImageMask(cairo_surface_t*, const FloatRect&);
    void drawSurfaceToContext(cairo_surface_t*, const FloatRect& destRect, const FloatRect& srcRect, GraphicsContext*);

private:
    RefPtr<cairo_t> m_cr;
    Vector<ImageMaskInformation> m_maskImageStack;
};

} // namespace WebCore

#endif // PlatformContextCairo_h
