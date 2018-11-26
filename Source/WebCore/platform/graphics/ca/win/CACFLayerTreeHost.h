/*
 * Copyright (C) 2009-2012, 2015 Apple Inc. All rights reserved.
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

#ifndef CACFLayerTreeHost_h
#define CACFLayerTreeHost_h

#include "AbstractCACFLayerTreeHost.h"
#include "COMPtr.h"
#include "Page.h"
#include "Timer.h"

#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

#include <CoreGraphics/CGGeometry.h>

interface IDirect3DDevice9;

typedef struct CGImage* CGImageRef;

#if USE(AVFOUNDATION)
struct GraphicsDeviceAdapter;
#endif

namespace WebCore {

class CACFLayerTreeHostClient;
class PlatformCALayer;
class TiledBacking;

class CACFLayerTreeHost : public RefCounted<CACFLayerTreeHost>, private AbstractCACFLayerTreeHost {
public:
    static RefPtr<CACFLayerTreeHost> create();
    virtual ~CACFLayerTreeHost();

    static bool acceleratedCompositingAvailable();

    void setClient(CACFLayerTreeHostClient* client) { m_client = client; }

    void setRootChildLayer(PlatformCALayer*);
    void setWindow(HWND);
    void setPage(Page*);
    virtual void paint(HDC = nullptr);
    virtual void resize() = 0;
    virtual void setScaleFactor(float) = 0;
    void flushPendingGraphicsLayerChangesSoon();
    virtual void setShouldInvertColors(bool);
#if USE(AVFOUNDATION)
    virtual GraphicsDeviceAdapter* graphicsDeviceAdapter() const { return 0; }
#endif

    virtual bool createRenderer() = 0;

    // AbstractCACFLayerTreeHost
    virtual void flushPendingLayerChangesNow();

    String layerTreeAsString() const;
    void updateDebugInfoLayer(bool);

protected:
    CACFLayerTreeHost();

    CGRect bounds() const;
    HWND window() const { return m_window; }
    void notifyAnimationsStarted();

    TiledBacking* mainFrameTiledBacking() const;

    // AbstractCACFLayerTreeHost
    virtual PlatformCALayer* rootLayer() const;

    virtual void destroyRenderer();
    virtual void contextDidChange();

private:
    void initialize();

    // AbstractCACFLayerTreeHost
    virtual void addPendingAnimatedLayer(PlatformCALayer&);
    virtual void layerTreeDidChange();


    virtual void flushContext() = 0;
    virtual CFTimeInterval lastCommitTime() const = 0;
    virtual void render(const Vector<CGRect>& dirtyRects = Vector<CGRect>(), HDC dc = nullptr) = 0;
    virtual void initializeContext(void* userData, PlatformCALayer*) = 0;

    CACFLayerTreeHostClient* m_client { nullptr };
    Page* m_page { nullptr };
    RefPtr<PlatformCALayer> m_rootLayer;
    RefPtr<PlatformCALayer> m_rootChildLayer;
    RefPtr<PlatformCALayer> m_debugInfoLayer;
    HashSet<RefPtr<PlatformCALayer> > m_pendingAnimatedLayers;
    HWND m_window { nullptr };
    bool m_shouldFlushPendingGraphicsLayerChanges { false };
    bool m_isFlushingLayerChanges { false };

#if !ASSERT_DISABLED
    enum { WindowNotSet, WindowSet, WindowCleared } m_state { WindowNotSet };
#endif
};

}

#endif // CACFLayerTreeHost_h
