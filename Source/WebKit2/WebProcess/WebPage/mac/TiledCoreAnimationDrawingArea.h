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

#ifndef TiledCoreAnimationDrawingArea_h
#define TiledCoreAnimationDrawingArea_h

#include "DrawingArea.h"
#include <WebCore/LayerFlushScheduler.h>
#include <WebCore/LayerFlushSchedulerClient.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;
OBJC_CLASS WKContentLayer;

typedef struct __WKCARemoteLayerClientRef* WKCARemoteLayerClientRef;

namespace WebKit {

class TiledCoreAnimationDrawingArea : public DrawingArea, private WebCore::LayerFlushSchedulerClient {
public:
    static PassOwnPtr<TiledCoreAnimationDrawingArea> create(WebPage*, const WebPageCreationParameters&);
    virtual ~TiledCoreAnimationDrawingArea();

private:
    TiledCoreAnimationDrawingArea(WebPage*, const WebPageCreationParameters&);

    // DrawingArea
    virtual void setNeedsDisplay(const WebCore::IntRect&) OVERRIDE;
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset) OVERRIDE;

    virtual void setLayerTreeStateIsFrozen(bool) OVERRIDE;
    virtual bool layerTreeStateIsFrozen() const OVERRIDE;
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) OVERRIDE;
    virtual void scheduleCompositingLayerSync() OVERRIDE;

    // WebCore::LayerFlushSchedulerClient
    virtual bool flushLayers() OVERRIDE;

    // Message handlers.
    virtual void updateGeometry(const WebCore::IntSize& viewSize) OVERRIDE;

    bool m_layerTreeStateIsFrozen;
    WebCore::LayerFlushScheduler m_layerFlushScheduler;

    RetainPtr<WKCARemoteLayerClientRef> m_remoteLayerClient;
    RetainPtr<CALayer> m_rootLayer;
};

} // namespace WebKit

#endif // TiledCoreAnimationDrawingArea_h
