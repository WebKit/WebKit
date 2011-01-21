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

#ifndef CACFLayerTreeHost_h
#define CACFLayerTreeHost_h

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

class CACFLayerTreeHostClient {
public:
    virtual ~CACFLayerTreeHostClient() { }
    virtual void flushPendingGraphicsLayerChanges() { }
};

// FIXME: Currently there is a CACFLayerTreeHost for each WebView and each
// has its own CARenderOGLContext and Direct3DDevice9, which is inefficient.
// (https://bugs.webkit.org/show_bug.cgi?id=31855)
class CACFLayerTreeHost : public RefCounted<CACFLayerTreeHost> {
    friend PlatformCALayer;

public:
    static PassRefPtr<CACFLayerTreeHost> create();
    ~CACFLayerTreeHost();

    static bool acceleratedCompositingAvailable();

    void setClient(CACFLayerTreeHostClient* client) { m_client = client; }

    void setRootChildLayer(PlatformCALayer*);
    void layerTreeDidChange();
    void setWindow(HWND);
    void paint();
    void resize();
    void flushPendingGraphicsLayerChangesSoon();
    void flushPendingLayerChangesNow();

protected:
    PlatformCALayer* rootLayer() const;
    void addPendingAnimatedLayer(PassRefPtr<PlatformCALayer>);

private:
    CACFLayerTreeHost();

    bool createRenderer();
    void destroyRenderer();
    void renderSoon();
    void renderTimerFired(Timer<CACFLayerTreeHost>*);

    CGRect bounds() const;

    void initD3DGeometry();

    // Call this when the device window has changed size or when IDirect3DDevice9::Present returns
    // D3DERR_DEVICELOST. Returns true if the device was recovered, false if rendering must be
    // aborted and reattempted soon.
    enum ResetReason { ChangedWindowSize, LostDevice };
    bool resetDevice(ResetReason);

    void render(const Vector<CGRect>& dirtyRects = Vector<CGRect>());

    CACFLayerTreeHostClient* m_client;
    bool m_mightBeAbleToCreateDeviceLater;
    COMPtr<IDirect3DDevice9> m_d3dDevice;
    RefPtr<PlatformCALayer> m_rootLayer;
    RefPtr<PlatformCALayer> m_rootChildLayer;
    WKCACFContext* m_context;
    HWND m_window;
    Timer<CACFLayerTreeHost> m_renderTimer;
    bool m_mustResetLostDeviceBeforeRendering;
    bool m_shouldFlushPendingGraphicsLayerChanges;
    bool m_isFlushingLayerChanges;
    HashSet<RefPtr<PlatformCALayer> > m_pendingAnimatedLayers;

#ifndef NDEBUG
    bool m_printTree;
#endif
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // CACFLayerTreeHost_h
