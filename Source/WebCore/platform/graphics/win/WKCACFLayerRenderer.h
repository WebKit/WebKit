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

#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

#include <CoreGraphics/CGGeometry.h>

interface IDirect3DDevice9;
struct WKCACFContext;

typedef struct CGImage* CGImageRef;

namespace WebCore {

class PlatformCALayer;

class WKCACFLayerRendererClient {
public:
    virtual ~WKCACFLayerRendererClient() { }
    virtual bool shouldRender() const = 0;
    virtual void flushPendingGraphicsLayerChanges() { }
};

// FIXME: Currently there is a WKCACFLayerRenderer for each WebView and each
// has its own CARenderOGLContext and Direct3DDevice9, which is inefficient.
// (https://bugs.webkit.org/show_bug.cgi?id=31855)
class WKCACFLayerRenderer : public RefCounted<WKCACFLayerRenderer> {
    friend PlatformCALayer;

public:
    static PassRefPtr<WKCACFLayerRenderer> create();
    ~WKCACFLayerRenderer();

    static bool acceleratedCompositingAvailable();

    void setClient(WKCACFLayerRendererClient* client) { m_client = client; }

    void setRootChildLayer(PlatformCALayer*);
    void layerTreeDidChange();
    void setHostWindow(HWND);
    void paint();
    void resize();
    void flushPendingGraphicsLayerChangesSoon();

protected:
    PlatformCALayer* rootLayer() const;
    void addPendingAnimatedLayer(PassRefPtr<PlatformCALayer>);

private:
    WKCACFLayerRenderer();

    bool createRenderer();
    void destroyRenderer();
    void renderSoon();
    void renderTimerFired(Timer<WKCACFLayerRenderer>*);

    CGRect bounds() const;

    void initD3DGeometry();

    // Call this when the device window has changed size or when IDirect3DDevice9::Present returns
    // D3DERR_DEVICELOST. Returns true if the device was recovered, false if rendering must be
    // aborted and reattempted soon.
    enum ResetReason { ChangedWindowSize, LostDevice };
    bool resetDevice(ResetReason);

    void render(const Vector<CGRect>& dirtyRects = Vector<CGRect>());

    WKCACFLayerRendererClient* m_client;
    bool m_mightBeAbleToCreateDeviceLater;
    COMPtr<IDirect3DDevice9> m_d3dDevice;
    RefPtr<PlatformCALayer> m_rootLayer;
    RefPtr<PlatformCALayer> m_rootChildLayer;
    WKCACFContext* m_context;
    HWND m_hostWindow;
    Timer<WKCACFLayerRenderer> m_renderTimer;
    bool m_mustResetLostDeviceBeforeRendering;
    bool m_shouldFlushPendingGraphicsLayerChanges;
    HashSet<RefPtr<PlatformCALayer> > m_pendingAnimatedLayers;

#ifndef NDEBUG
    bool m_printTree;
#endif
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // WKCACFLayerRenderer_h
