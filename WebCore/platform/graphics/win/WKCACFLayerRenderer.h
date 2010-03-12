/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WKCACFLayerRenderer_h
#define WKCACFLayerRenderer_h

#if USE(ACCELERATED_COMPOSITING)

#include "COMPtr.h"
#include "Timer.h"
#include "WKCACFLayer.h"

#include <wtf/Noncopyable.h>

#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

#include <CoreGraphics/CGGeometry.h>

interface IDirect3DDevice9;
typedef struct _CACFContext* CACFContextRef;
typedef struct _CARenderContext CARenderContext;
typedef struct _CARenderOGLContext CARenderOGLContext;

namespace WebCore {

class WKCACFRootLayer;

// FIXME: Currently there is a WKCACFLayerRenderer for each WebView and each
// has its own CARenderOGLContext and Direct3DDevice9, which is inefficient.
// (https://bugs.webkit.org/show_bug.cgi?id=31855)
class WKCACFLayerRenderer : public Noncopyable {
public:
    static PassOwnPtr<WKCACFLayerRenderer> create();
    ~WKCACFLayerRenderer();

    static bool acceleratedCompositingAvailable();
    static void didFlushContext(CACFContextRef);

    void setScrollFrame(const IntPoint&, const IntSize&);
    void setRootContents(CGImageRef);
    void setRootChildLayer(WKCACFLayer* layer);
    void setNeedsDisplay();
    void setHostWindow(HWND window) { m_hostWindow = window; }
    void setBackingStoreDirty(bool dirty) { m_backingStoreDirty = dirty; }
    bool createRenderer();
    void destroyRenderer();
    void resize();
    void renderSoon();
    void updateScrollFrame();

protected:
    WKCACFLayer* rootLayer() const;

private:
    WKCACFLayerRenderer();

    void renderTimerFired(Timer<WKCACFLayerRenderer>*);

    CGRect bounds() const;

    void initD3DGeometry();
    void resetDevice();

    void render(const Vector<CGRect>& dirtyRects = Vector<CGRect>());
    void paint();

    bool m_triedToCreateD3DRenderer;
    COMPtr<IDirect3DDevice9> m_d3dDevice;
    RefPtr<WKCACFRootLayer> m_rootLayer;
    RefPtr<WKCACFLayer> m_viewLayer;
    RefPtr<WKCACFLayer> m_scrollLayer;
    RefPtr<WKCACFLayer> m_rootChildLayer;
    RefPtr<WKCACFLayer> m_clipLayer;
    RetainPtr<CACFContextRef> m_context;
    CARenderContext* m_renderContext;
    CARenderOGLContext* m_renderer;
    HWND m_hostWindow;
    Timer<WKCACFLayerRenderer> m_renderTimer;
    IntPoint m_scrollPosition;
    IntSize m_scrollSize;
    bool m_backingStoreDirty;
#ifndef NDEBUG
    bool m_printTree;
#endif
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // WKCACFLayerRenderer_h
