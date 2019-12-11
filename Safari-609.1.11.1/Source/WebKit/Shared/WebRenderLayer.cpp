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

#include "config.h"
#include "WebRenderLayer.h"

#include "APIArray.h"
#include "APIString.h"
#include "WebPage.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/RenderLayerBacking.h>
#include <WebCore/RenderView.h>
#include <WebCore/RenderWidget.h>
#include <WebCore/StyledElement.h>

namespace WebKit {

RefPtr<WebRenderLayer> WebRenderLayer::create(WebPage* page)
{
    WebCore::Frame* mainFrame = page->mainFrame();
    if (!mainFrame)
        return nullptr;

    if (!mainFrame->loader().client().hasHTMLView())
        return nullptr;

    WebCore::RenderView* contentRenderer = mainFrame->contentRenderer();
    if (!contentRenderer)
        return nullptr;

    WebCore::RenderLayer* rootLayer = contentRenderer->layer();
    if (!rootLayer)
        return nullptr;

    return adoptRef(new WebRenderLayer(rootLayer));
}

Ref<WebRenderLayer> WebRenderLayer::create(RefPtr<WebRenderObject>&& renderer, bool isReflection, bool isClipping, bool isClipped, CompositingLayerType type, WebCore::IntRect absoluteBoundingBox, double backingStoreMemoryEstimate, RefPtr<API::Array>&& negativeZOrderList, RefPtr<API::Array>&& normalFlowList, RefPtr<API::Array>&& positiveZOrderList, RefPtr<WebRenderLayer>&& frameContentsLayer)
{
    return adoptRef(*new WebRenderLayer(WTFMove(renderer), isReflection, isClipping, isClipped, type, absoluteBoundingBox, backingStoreMemoryEstimate, WTFMove(negativeZOrderList), WTFMove(normalFlowList), WTFMove(positiveZOrderList), WTFMove(frameContentsLayer)));
}

WebRenderLayer::WebRenderLayer(WebCore::RenderLayer* layer)
{
    m_renderer = WebRenderObject::create(&layer->renderer());
    m_isReflection = layer->isReflection();

    if (layer->isComposited()) {
        WebCore::RenderLayerBacking* backing = layer->backing();
        m_isClipping = backing->hasClippingLayer();
        m_isClipped = backing->hasAncestorClippingLayers();
        switch (backing->compositingLayerType()) {
        case WebCore::NormalCompositingLayer:
            m_compositingLayerType = Normal;
            break;
        case WebCore::TiledCompositingLayer:
            m_compositingLayerType = Tiled;
            break;
        case WebCore::MediaCompositingLayer:
            m_compositingLayerType = Media;
            break;
        case WebCore::ContainerCompositingLayer:
            m_compositingLayerType = Container;
            break;
        }

        m_backingStoreMemoryEstimate = backing->backingStoreMemoryEstimate();
    } else {
        m_isClipping = false;
        m_isClipped = false;
        m_compositingLayerType = None;
        m_backingStoreMemoryEstimate = 0;
    }

    m_absoluteBoundingBox = layer->absoluteBoundingBox();

    auto createArrayFromLayerList = [] (WebCore::RenderLayer::LayerList list) -> RefPtr<API::Array> {
        if (!list.size())
            return nullptr;

        Vector<RefPtr<API::Object>> layers;
        layers.reserveInitialCapacity(list.size());

        for (auto* layer : list)
            layers.uncheckedAppend(adoptRef(new WebRenderLayer(layer)));

        return API::Array::create(WTFMove(layers));
    };

    m_negativeZOrderList = createArrayFromLayerList(layer->negativeZOrderLayers());
    m_normalFlowList = createArrayFromLayerList(layer->normalFlowLayers());
    m_positiveZOrderList = createArrayFromLayerList(layer->positiveZOrderLayers());

    if (is<WebCore::RenderWidget>(layer->renderer())) {
        if (WebCore::Document* contentDocument = downcast<WebCore::RenderWidget>(layer->renderer()).frameOwnerElement().contentDocument()) {
            if (WebCore::RenderView* view = contentDocument->renderView())
                m_frameContentsLayer = adoptRef(new WebRenderLayer(view->layer()));
        }
    }
}

WebRenderLayer::WebRenderLayer(RefPtr<WebRenderObject>&& renderer, bool isReflection, bool isClipping, bool isClipped, CompositingLayerType type, WebCore::IntRect absoluteBoundingBox, double backingStoreMemoryEstimate, RefPtr<API::Array>&& negativeZOrderList, RefPtr<API::Array>&& normalFlowList, RefPtr<API::Array>&& positiveZOrderList, RefPtr<WebRenderLayer>&& frameContentsLayer)
    : m_renderer(WTFMove(renderer))
    , m_isReflection(isReflection)
    , m_isClipping(isClipping)
    , m_isClipped(isClipped)
    , m_compositingLayerType(type)
    , m_absoluteBoundingBox(absoluteBoundingBox)
    , m_backingStoreMemoryEstimate(backingStoreMemoryEstimate)
    , m_negativeZOrderList(WTFMove(negativeZOrderList))
    , m_normalFlowList(WTFMove(normalFlowList))
    , m_positiveZOrderList(WTFMove(positiveZOrderList))
    , m_frameContentsLayer(WTFMove(frameContentsLayer))
{
}

} // namespace WebKit
