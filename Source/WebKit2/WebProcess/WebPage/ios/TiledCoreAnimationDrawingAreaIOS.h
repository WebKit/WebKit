/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef TiledCoreAnimationDrawingAreaIOS_h
#define TiledCoreAnimationDrawingAreaIOS_h

#include "DrawingArea.h"
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/LayerFlushScheduler.h>
#include <WebCore/LayerFlushSchedulerClient.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS CAContext;
OBJC_CLASS CALayer;

namespace WebKit {

class TiledCoreAnimationDrawingAreaIOS : public DrawingArea, private WebCore::GraphicsLayerClient, private WebCore::LayerFlushSchedulerClient {
public:
    TiledCoreAnimationDrawingAreaIOS(WebPage*, const WebPageCreationParameters&);
    virtual ~TiledCoreAnimationDrawingAreaIOS();

private:
    // DrawingArea
    virtual void setNeedsDisplay() OVERRIDE;
    virtual void setNeedsDisplayInRect(const WebCore::IntRect&) OVERRIDE;
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset) OVERRIDE;
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) OVERRIDE;
    virtual void scheduleCompositingLayerFlush() OVERRIDE;

    // GraphicsLayerClient
    virtual bool shouldUseTiledBacking(const WebCore::GraphicsLayer*) const OVERRIDE;
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double time) OVERRIDE;
    virtual void notifyFlushRequired(const WebCore::GraphicsLayer*) OVERRIDE;
    
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& inClip) OVERRIDE;
    
    virtual float minimumDocumentScale() const OVERRIDE;
    virtual bool allowCompositingLayerVisualDegradation() const OVERRIDE;

    // LayerFlushSchedulerClient
    virtual bool flushLayers() OVERRIDE;

    // Message handlers.
    virtual void updateGeometry(const WebCore::IntSize& viewSize, const WebCore::IntSize& layerPosition);
    virtual void setDeviceScaleFactor(float) OVERRIDE;

    WebCore::LayerFlushScheduler m_layerFlushScheduler;

    RetainPtr<CAContext> m_context;
    RetainPtr<CALayer> m_rootLayer;

    OwnPtr<WebCore::GraphicsLayer> m_contentLayer;
};

} // namespace WebKit

#endif // TiledCoreAnimationDrawingAreaIOS_h
