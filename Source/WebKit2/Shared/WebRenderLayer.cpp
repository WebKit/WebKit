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
#include <WebCore/StyledElement.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebRenderLayer> WebRenderLayer::create(WebPage* page)
{
    Frame* mainFrame = page->mainFrame();
    if (!mainFrame)
        return 0;

    if (!mainFrame->loader().client().hasHTMLView())
        return 0;

    RenderView* contentRenderer = mainFrame->contentRenderer();
    if (!contentRenderer)
        return 0;

    RenderLayer* rootLayer = contentRenderer->layer();
    if (!rootLayer)
        return 0;

    return adoptRef(new WebRenderLayer(rootLayer));
}

PassRefPtr<WebRenderLayer> WebRenderLayer::create(PassRefPtr<WebRenderObject> renderer, bool isReflection, bool isClipping, bool isClipped, CompositingLayerType type, WebCore::IntRect absoluteBoundingBox, PassRefPtr<API::Array> negativeZOrderList, PassRefPtr<API::Array> normalFlowList, PassRefPtr<API::Array> positiveZOrderList)
{
    return adoptRef(new WebRenderLayer(renderer, isReflection, isClipping, isClipped, type, absoluteBoundingBox, negativeZOrderList, normalFlowList, positiveZOrderList));
}

PassRefPtr<API::Array> WebRenderLayer::createArrayFromLayerList(Vector<RenderLayer*>* list)
{
    if (!list || !list->size())
        return nullptr;

    Vector<RefPtr<API::Object>> layers;
    layers.reserveInitialCapacity(list->size());

    for (const auto& layer : *list)
        layers.uncheckedAppend(adoptRef(new WebRenderLayer(layer)));

    return API::Array::create(std::move(layers));
}

WebRenderLayer::WebRenderLayer(RenderLayer* layer)
{
    m_renderer = WebRenderObject::create(&layer->renderer());
    m_isReflection = layer->isReflection();

    if (layer->isComposited()) {
        RenderLayerBacking* backing = layer->backing();
        m_isClipping = backing->hasClippingLayer();
        m_isClipped = backing->hasAncestorClippingLayer();
        switch (backing->compositingLayerType()) {
        case NormalCompositingLayer:
            m_compositingLayerType = Normal;
            break;
        case TiledCompositingLayer:
            m_compositingLayerType = Tiled;
            break;
        case MediaCompositingLayer:
            m_compositingLayerType = Media;
            break;
        case ContainerCompositingLayer:
            m_compositingLayerType = Container;
            break;
        }
    } else {
        m_isClipping = false;
        m_isClipped = false;
        m_compositingLayerType = None;
    }

    m_absoluteBoundingBox = layer->absoluteBoundingBox();

    m_negativeZOrderList = createArrayFromLayerList(layer->negZOrderList());
    m_normalFlowList = createArrayFromLayerList(layer->normalFlowList());
    m_positiveZOrderList = createArrayFromLayerList(layer->posZOrderList());
}

WebRenderLayer::WebRenderLayer(PassRefPtr<WebRenderObject> renderer, bool isReflection, bool isClipping, bool isClipped, CompositingLayerType type, WebCore::IntRect absoluteBoundingBox, PassRefPtr<API::Array> negativeZOrderList, PassRefPtr<API::Array> normalFlowList, PassRefPtr<API::Array> positiveZOrderList)
    : m_renderer(renderer)
    , m_isReflection(isReflection)
    , m_isClipping(isClipping)
    , m_isClipped(isClipped)
    , m_compositingLayerType(type)
    , m_absoluteBoundingBox(absoluteBoundingBox)
    , m_negativeZOrderList(negativeZOrderList)
    , m_normalFlowList(normalFlowList)
    , m_positiveZOrderList(positiveZOrderList)
{
}

} // namespace WebKit
