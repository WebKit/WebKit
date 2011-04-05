/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LayerTreeHostCAWin_h
#define LayerTreeHostCAWin_h

#include "HeaderDetection.h"

#if HAVE(WKQCA)

#include "LayerTreeHostCA.h"
#include <WebCore/AbstractCACFLayerTreeHost.h>
#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>

typedef struct _WKCACFView* WKCACFViewRef;

namespace WebKit {

class LayerTreeHostCAWin : public LayerTreeHostCA, private WebCore::AbstractCACFLayerTreeHost {
public:
    static bool supportsAcceleratedCompositing();

    static PassRefPtr<LayerTreeHostCAWin> create(WebPage*);
    virtual ~LayerTreeHostCAWin();

private:
    explicit LayerTreeHostCAWin(WebPage*);

    static void contextDidChangeCallback(WKCACFViewRef, void* info);
    void contextDidChange();

    // LayerTreeHost
    virtual void invalidate();
    virtual void forceRepaint();
    virtual void sizeDidChange(const WebCore::IntSize& newSize);
    virtual void scheduleLayerFlush();
    virtual bool participatesInDisplay();
    virtual bool needsDisplay();
    virtual double timeUntilNextDisplay();
    virtual void display(UpdateInfo&);

    // LayerTreeHostCA
    virtual void platformInitialize(LayerTreeContext&);

    // AbstractCACFLayerTreeHost
    virtual WebCore::PlatformCALayer* rootLayer() const;
    virtual void addPendingAnimatedLayer(PassRefPtr<WebCore::PlatformCALayer>);
    virtual void layerTreeDidChange();
    virtual void flushPendingLayerChangesNow();

    RetainPtr<WKCACFViewRef> m_view;
    HashSet<RefPtr<WebCore::PlatformCALayer> > m_pendingAnimatedLayers;
    bool m_isFlushingLayerChanges;
    double m_nextDisplayTime;
};

} // namespace WebKit

#endif // HAVE(WKQCA)

#endif // LayerTreeHostCAWin_h
