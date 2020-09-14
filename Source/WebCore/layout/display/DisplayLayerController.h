/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayRect.h"
#include "GraphicsLayer.h"
#include "LayoutUnits.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class GraphicsLayer;
class GraphicsLayerFactory;

namespace Layout {
class LayoutState;
}

namespace Display {

class View;

// A controller object that makes layerization decisions for a display tree, for a single document.
class LayerController {
    WTF_MAKE_ISO_ALLOCATED(LayerController);
public:
    explicit LayerController(View&);
    ~LayerController();
    
    void prepareForDisplay(const Layout::LayoutState&);
    void flushLayers();

    const View& view() const { return m_view; }

private:
    void ensureRootLayer(LayoutSize viewSize, LayoutSize contentSize);
    void attachRootLayer();
    void setupRootLayerHierarchy();
    void updateRootLayerGeometry(LayoutSize viewSize, LayoutSize contentSize);
    void scheduleRenderingUpdate();
    
    GraphicsLayer* rootGraphicsLayer() const { return m_rootLayer.get(); }
    GraphicsLayer* contentLayer() const { return m_contentLayer.get(); }

    GraphicsLayerFactory* graphicsLayerFactory() const;

    FloatRect visibleRectForLayerFlushing() const;

    class RootLayerClient : public GraphicsLayerClient {
    public:
        RootLayerClient(LayerController&);

    private:
        // GraphicsLayerClient implementation
        void notifyFlushRequired(const GraphicsLayer*) final;
        void paintContents(const GraphicsLayer*, GraphicsContext&, const FloatRect&, GraphicsLayerPaintBehavior) final;

        LayerController& m_layerController;
    };

    View& m_view;
    RootLayerClient m_rootLayerClient;

    RefPtr<GraphicsLayer> m_rootLayer;
    RefPtr<GraphicsLayer> m_contentHostLayer;
    RefPtr<GraphicsLayer> m_contentLayer;
};

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
