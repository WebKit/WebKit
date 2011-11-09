/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NonCompositedContentHost_h
#define NonCompositedContentHost_h

#include "GraphicsLayerClient.h"
#include "IntSize.h"

#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class Color;
class GraphicsLayer;
class GraphicsContext;
class IntPoint;
class IntRect;
class LayerChromium;
class LayerPainterChromium;

class NonCompositedContentHost : public GraphicsLayerClient {
WTF_MAKE_NONCOPYABLE(NonCompositedContentHost);
public:
    static PassOwnPtr<NonCompositedContentHost> create(PassOwnPtr<LayerPainterChromium> contentPaint)
    {
        return adoptPtr(new NonCompositedContentHost(contentPaint));
    }
    virtual ~NonCompositedContentHost();

    void invalidateRect(const IntRect&);
    void setBackgroundColor(const Color&);
    void setScrollLayer(GraphicsLayer*);
    void setViewport(const IntSize& viewportSize, const IntSize& contentsSize, const IntPoint& scrollPosition, float pageScale);
    void protectVisibleTileTextures();
    GraphicsLayer* topLevelRootLayer() const { return m_graphicsLayer.get(); }

private:
    explicit NonCompositedContentHost(PassOwnPtr<LayerPainterChromium> contentPaint);

    // GraphicsLayerClient
    virtual void notifyAnimationStarted(const GraphicsLayer*, double time);
    virtual void notifySyncRequired(const GraphicsLayer*);
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& clipRect);
    virtual bool showDebugBorders() const;
    virtual bool showRepaintCounter() const;

    LayerChromium* scrollLayer();

    OwnPtr<GraphicsLayer> m_graphicsLayer;
    OwnPtr<LayerPainterChromium> m_contentPaint;
    IntSize m_viewportSize;
};

} // namespace WebCore

#endif // NonCompositedContentHost_h
