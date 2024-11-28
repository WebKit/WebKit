/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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

#import "DrawingArea.h"
#import "GraphicsLayerCARemote.h"
#import "PlatformCALayerRemote.h"
#import "RemoteLayerTreeDrawingArea.h"
#import "RemoteLayerTreeTransaction.h"
#import "VideoPresentationManager.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/HTMLMediaElementIdentifier.h>
#import <WebCore/HTMLVideoElement.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/Page.h>
#import <wtf/SetForScope.h>
#import <wtf/SystemTracing.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeContext);

RemoteLayerTreeContext::RemoteLayerTreeContext(WebPage& webPage)
    : m_webPage(webPage)
    , m_backingStoreCollection(makeUniqueRefWithoutRefCountedCheck<RemoteLayerBackingStoreCollection>(*this))
{
}

RemoteLayerTreeContext::~RemoteLayerTreeContext()
{
    // Make sure containers are empty before destruction to avoid hitting the assertion in CanMakeCheckedPtr.
    m_livePlatformLayers.clear();
    m_liveGraphicsLayers.clear();
    m_layersWithAnimations.clear();
}

void RemoteLayerTreeContext::adoptLayersFromContext(RemoteLayerTreeContext& oldContext)
{
    auto& platformLayers = oldContext.m_livePlatformLayers;
    while (!platformLayers.isEmpty())
        RefPtr { platformLayers.begin()->value.get() }->moveToContext(*this);

    auto& graphicsLayers = oldContext.m_liveGraphicsLayers;
    while (!graphicsLayers.isEmpty())
        Ref { (*graphicsLayers.begin()).get() }->moveToContext(*this);
}

float RemoteLayerTreeContext::deviceScaleFactor() const
{
    return m_webPage->deviceScaleFactor();
}

LayerHostingMode RemoteLayerTreeContext::layerHostingMode() const
{
    return m_webPage->layerHostingMode();
}

std::optional<DrawingAreaIdentifier> RemoteLayerTreeContext::drawingAreaIdentifier() const
{
    if (!m_webPage->drawingArea())
        return std::nullopt;
    return m_webPage->drawingArea()->identifier();
}

std::optional<WebCore::DestinationColorSpace> RemoteLayerTreeContext::displayColorSpace() const
{
    if (auto* drawingArea = m_webPage->drawingArea())
        return drawingArea->displayColorSpace();
    
    return { };
}

#if PLATFORM(IOS_FAMILY)
bool RemoteLayerTreeContext::canShowWhileLocked() const
{
    return m_webPage->canShowWhileLocked();
}
#endif

void RemoteLayerTreeContext::layerDidEnterContext(PlatformCALayerRemote& layer, PlatformCALayer::LayerType type)
{
    PlatformLayerIdentifier layerID = layer.layerID();

    RemoteLayerTreeTransaction::LayerCreationProperties creationProperties;
    layer.populateCreationProperties(creationProperties, *this, type);

    m_createdLayers.add(layerID, WTFMove(creationProperties));
    m_livePlatformLayers.add(layerID, &layer);
}

#if HAVE(AVKIT)
void RemoteLayerTreeContext::layerDidEnterContext(PlatformCALayerRemote& layer, PlatformCALayer::LayerType type, WebCore::HTMLVideoElement& videoElement)
{
    PlatformLayerIdentifier layerID = layer.layerID();

    RemoteLayerTreeTransaction::LayerCreationProperties creationProperties;
    layer.populateCreationProperties(creationProperties, *this, type);
    ASSERT(!creationProperties.videoElementData);
    creationProperties.videoElementData = RemoteLayerTreeTransaction::LayerCreationProperties::VideoElementData {
        videoElement.identifier(),
        videoElement.videoLayerSize(),
        videoElement.naturalSize()
    };

    protectedWebPage()->protectedVideoPresentationManager()->setupRemoteLayerHosting(videoElement);
    m_videoLayers.add(layerID, videoElement.identifier());

    m_createdLayers.add(layerID, WTFMove(creationProperties));
    m_livePlatformLayers.add(layerID, &layer);
}
#endif

WebPage& RemoteLayerTreeContext::webPage()
{
    return m_webPage.get();
}

Ref<WebPage> RemoteLayerTreeContext::protectedWebPage()
{
    return m_webPage.get();
}

void RemoteLayerTreeContext::layerWillLeaveContext(PlatformCALayerRemote& layer)
{
    auto layerID = layer.layerID();

#if HAVE(AVKIT)
    auto videoLayerIter = m_videoLayers.find(layerID);
    if (videoLayerIter != m_videoLayers.end()) {
        protectedWebPage()->protectedVideoPresentationManager()->willRemoveLayerForID(videoLayerIter->value);
        m_videoLayers.remove(videoLayerIter);
    }
#endif

    m_createdLayers.remove(layerID);
    m_livePlatformLayers.remove(layerID);

    ASSERT(!m_destroyedLayers.contains(layerID));
    m_destroyedLayers.append(layerID);
    
    m_layersWithAnimations.remove(layerID);
}

void RemoteLayerTreeContext::graphicsLayerDidEnterContext(GraphicsLayerCARemote& layer)
{
    m_liveGraphicsLayers.add(layer);
}

void RemoteLayerTreeContext::graphicsLayerWillLeaveContext(GraphicsLayerCARemote& layer)
{
    m_liveGraphicsLayers.remove(layer);
}

Ref<GraphicsLayer> RemoteLayerTreeContext::createGraphicsLayer(WebCore::GraphicsLayer::Type layerType, GraphicsLayerClient& client)
{
    return adoptRef(*new GraphicsLayerCARemote(layerType, client, *this));
}

void RemoteLayerTreeContext::buildTransaction(RemoteLayerTreeTransaction& transaction, PlatformCALayer& rootLayer, WebCore::FrameIdentifier rootFrameID)
{
    TraceScope tracingScope(BuildTransactionStart, BuildTransactionEnd);

    PlatformCALayerRemote& rootLayerRemote = downcast<PlatformCALayerRemote>(rootLayer);
    transaction.setRootLayerID(rootLayerRemote.layerID());
    if (auto* rootFrame = WebProcess::singleton().webFrame(rootFrameID))
        transaction.setRemoteContextHostedIdentifier(rootFrame->layerHostingContextIdentifier());

    m_currentTransaction = &transaction;
    rootLayerRemote.recursiveBuildTransaction(*this, transaction);
    m_backingStoreCollection->prepareBackingStoresForDisplay(transaction);

    bool paintedAnyBackingStore = m_backingStoreCollection->paintReachableBackingStoreContents();
    if (paintedAnyBackingStore)
        m_nextRenderingUpdateRequiresSynchronousImageDecoding = false;

    m_currentTransaction = nullptr;

    transaction.setCreatedLayers(moveToVector(std::exchange(m_createdLayers, { }).values()));
    transaction.setDestroyedLayerIDs(WTFMove(m_destroyedLayers));
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

void RemoteLayerTreeContext::animationDidStart(WebCore::PlatformLayerIdentifier layerID, const String& key, MonotonicTime startTime)
{
    auto it = m_layersWithAnimations.find(layerID);
    if (it != m_layersWithAnimations.end())
        RefPtr { it->value.get() }->animationStarted(key, startTime);
}

void RemoteLayerTreeContext::animationDidEnd(WebCore::PlatformLayerIdentifier layerID, const String& key)
{
    auto it = m_layersWithAnimations.find(layerID);
    if (it != m_layersWithAnimations.end())
        RefPtr { it->value.get() }->animationEnded(key);
}

RemoteRenderingBackendProxy& RemoteLayerTreeContext::ensureRemoteRenderingBackendProxy()
{
    return protectedWebPage()->ensureRemoteRenderingBackendProxy();
}

void RemoteLayerTreeContext::gpuProcessConnectionWasDestroyed()
{
    m_backingStoreCollection->gpuProcessConnectionWasDestroyed();
}

} // namespace WebKit
