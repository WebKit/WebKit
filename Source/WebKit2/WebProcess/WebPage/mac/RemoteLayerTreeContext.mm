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

#import "config.h"
#import "RemoteLayerTreeContext.h"

#import "GenericCallback.h"
#import "GraphicsLayerCARemote.h"
#import "PlatformCALayerRemote.h"
#import "RemoteLayerTreeTransaction.h"
#import "WebPage.h"
#import <WebCore/FrameView.h>
#import <WebCore/MainFrame.h>
#import <WebCore/Page.h>
#import <wtf/TemporaryChange.h>

using namespace WebCore;

namespace WebKit {

RemoteLayerTreeContext::RemoteLayerTreeContext(WebPage* webPage)
    : m_webPage(webPage)
{
}

RemoteLayerTreeContext::~RemoteLayerTreeContext()
{
}

void RemoteLayerTreeContext::layerWasCreated(PlatformCALayerRemote* layer, PlatformCALayer::LayerType type)
{
    RemoteLayerTreeTransaction::LayerCreationProperties creationProperties;
    creationProperties.layerID = layer->layerID();
    creationProperties.type = type;

    if (type == PlatformCALayer::LayerTypeCustom)
        creationProperties.hostingContextID = layer->hostingContextID();

    m_createdLayers.append(creationProperties);
}

void RemoteLayerTreeContext::layerWillBeDestroyed(PlatformCALayerRemote* layer)
{
    ASSERT(!m_destroyedLayers.contains(layer->layerID()));
    m_destroyedLayers.append(layer->layerID());
}

void RemoteLayerTreeContext::outOfTreeLayerWasAdded(GraphicsLayer* layer)
{
    ASSERT(!m_outOfTreeLayers.contains(layer));
    m_outOfTreeLayers.append(layer);
}

void RemoteLayerTreeContext::outOfTreeLayerWillBeRemoved(GraphicsLayer* layer)
{
    size_t layerIndex = m_outOfTreeLayers.find(layer);
    ASSERT(layerIndex != notFound);
    m_outOfTreeLayers.remove(layerIndex);
}

std::unique_ptr<GraphicsLayer> RemoteLayerTreeContext::createGraphicsLayer(GraphicsLayerClient* client)
{
    return std::make_unique<GraphicsLayerCARemote>(client, this);
}

void RemoteLayerTreeContext::flushOutOfTreeLayers()
{
    for (const auto& layer : m_outOfTreeLayers)
        layer->flushCompositingStateForThisLayerOnly();
}

void RemoteLayerTreeContext::buildTransaction(RemoteLayerTreeTransaction& transaction, PlatformCALayer& rootLayer)
{
    PlatformCALayerRemote& rootLayerRemote = toPlatformCALayerRemote(rootLayer);
    transaction.setRootLayerID(rootLayerRemote.layerID());

    rootLayerRemote.recursiveBuildTransaction(transaction);

    transaction.setCreatedLayers(std::move(m_createdLayers));
    transaction.setDestroyedLayerIDs(std::move(m_destroyedLayers));
}

} // namespace WebKit
