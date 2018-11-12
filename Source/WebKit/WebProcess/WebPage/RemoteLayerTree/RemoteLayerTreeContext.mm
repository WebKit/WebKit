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
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeTransaction.h"
#import "WebPage.h"
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/Page.h>
#import <wtf/SetForScope.h>
#import <wtf/SystemTracing.h>

namespace WebKit {
using namespace WebCore;

RemoteLayerTreeContext::RemoteLayerTreeContext(WebPage& webPage)
    : m_webPage(webPage)
    , m_currentTransaction(nullptr)
{
}

RemoteLayerTreeContext::~RemoteLayerTreeContext()
{
    for (auto& layer : m_liveLayers.values())
        layer->clearContext();
}

float RemoteLayerTreeContext::deviceScaleFactor() const
{
    return m_webPage.deviceScaleFactor();
}

LayerHostingMode RemoteLayerTreeContext::layerHostingMode() const
{
    return m_webPage.layerHostingMode();
}

void RemoteLayerTreeContext::layerWasCreated(PlatformCALayerRemote& layer, PlatformCALayer::LayerType type)
{
    GraphicsLayer::PlatformLayerID layerID = layer.layerID();

    RemoteLayerTreeTransaction::LayerCreationProperties creationProperties;
    creationProperties.layerID = layerID;
    creationProperties.type = type;
    creationProperties.embeddedViewID = layer.embeddedViewID();

    if (layer.isPlatformCALayerRemoteCustom()) {
        creationProperties.hostingContextID = layer.hostingContextID();
        creationProperties.hostingDeviceScaleFactor = deviceScaleFactor();
    }

    m_createdLayers.add(layerID, WTFMove(creationProperties));
    m_liveLayers.add(layerID, &layer);
}

void RemoteLayerTreeContext::layerWillBeDestroyed(PlatformCALayerRemote& layer)
{
    ASSERT(layer.layerID());
    GraphicsLayer::PlatformLayerID layerID = layer.layerID();

    m_createdLayers.remove(layerID);
    m_liveLayers.remove(layerID);

    ASSERT(!m_destroyedLayers.contains(layerID));
    m_destroyedLayers.append(layerID);
    
    m_layersWithAnimations.remove(layerID);
}

void RemoteLayerTreeContext::backingStoreWasCreated(RemoteLayerBackingStore& backingStore)
{
    m_backingStoreCollection.backingStoreWasCreated(backingStore);
}

void RemoteLayerTreeContext::backingStoreWillBeDestroyed(RemoteLayerBackingStore& backingStore)
{
    m_backingStoreCollection.backingStoreWillBeDestroyed(backingStore);
}

bool RemoteLayerTreeContext::backingStoreWillBeDisplayed(RemoteLayerBackingStore& backingStore)
{
    return m_backingStoreCollection.backingStoreWillBeDisplayed(backingStore);
}

Ref<GraphicsLayer> RemoteLayerTreeContext::createGraphicsLayer(WebCore::GraphicsLayer::Type layerType, GraphicsLayerClient& client)
{
    return adoptRef(*new GraphicsLayerCARemote(layerType, client, *this));
}

void RemoteLayerTreeContext::buildTransaction(RemoteLayerTreeTransaction& transaction, PlatformCALayer& rootLayer)
{
    TraceScope tracingScope(BuildTransactionStart, BuildTransactionEnd);

    PlatformCALayerRemote& rootLayerRemote = downcast<PlatformCALayerRemote>(rootLayer);
    transaction.setRootLayerID(rootLayerRemote.layerID());

    m_currentTransaction = &transaction;
    rootLayerRemote.recursiveBuildTransaction(*this, transaction);
    m_currentTransaction = nullptr;

    transaction.setCreatedLayers(copyToVector(m_createdLayers.values()));
    transaction.setDestroyedLayerIDs(WTFMove(m_destroyedLayers));
    
    m_createdLayers.clear();
}

void RemoteLayerTreeContext::layerPropertyChangedWhileBuildingTransaction(PlatformCALayerRemote& layer)
{
    if (m_currentTransaction)
        m_currentTransaction->layerPropertiesChanged(layer);
}

void RemoteLayerTreeContext::willStartAnimationOnLayer(PlatformCALayerRemote& layer)
{
    m_layersWithAnimations.add(layer.layerID(), &layer);
}

void RemoteLayerTreeContext::animationDidStart(WebCore::GraphicsLayer::PlatformLayerID layerID, const String& key, MonotonicTime startTime)
{
    auto it = m_layersWithAnimations.find(layerID);
    if (it != m_layersWithAnimations.end())
        it->value->animationStarted(key, startTime);
}

void RemoteLayerTreeContext::animationDidEnd(WebCore::GraphicsLayer::PlatformLayerID layerID, const String& key)
{
    auto it = m_layersWithAnimations.find(layerID);
    if (it != m_layersWithAnimations.end())
        it->value->animationEnded(key);
}

} // namespace WebKit
