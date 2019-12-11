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

#ifndef PlatformContextCairo_h
#define PlatformContextCairo_h

#if USE(CAIRO)

#include "GraphicsContext.h"
#include "RefPtrCairo.h"

namespace WebCore {

class GraphicsContextPlatformPrivate;
struct GraphicsContextState;

namespace Cairo {
struct FillSource;
struct StrokeSource;
struct ShadowState;
}

// Much like PlatformContextSkia in the Skia port, this class holds information that
// would normally be private to GraphicsContext, except that we want to allow access
// to it in Font and Image code. This allows us to separate the concerns of Cairo-specific
// code from the platform-independent GraphicsContext.

class PlatformContextCairo {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PlatformContextCairo);
public:
    PlatformContextCairo(cairo_t*);
    ~PlatformContextCairo();

    cairo_t* cr() { return m_cr.get(); }
    void setCr(cairo_t* cr) { m_cr = cr; }

    GraphicsContextPlatformPrivate* graphicsContextPrivate() { return m_graphicsContextPrivate; }
    void setGraphicsContextPrivate(GraphicsContextPlatformPrivate* graphicsContextPrivate) { m_graphicsContextPrivate = graphicsContextPrivate; }

    Vector<float>& layers() { return m_layers; }

    void save();
    void restore();

    void pushImageMask(cairo_surface_t*, const FloatRect&);

private:
    RefPtr<cairo_t> m_cr;

    // Keeping a pointer to GraphicsContextPlatformPrivate here enables calling
    // Windows-specific methods from CairoOperations (where only PlatformContextCairo
    // can be leveraged).
    GraphicsContextPlatformPrivate* m_graphicsContextPrivate { nullptr };

    class State;
    State* m_state;
    WTF::Vector<State> m_stateStack;

    // Transparency layers.
    Vector<float> m_layers;
};

} // namespace WebCore

#endif // USE(CAIRO)

#endif // PlatformContextCairo_h
