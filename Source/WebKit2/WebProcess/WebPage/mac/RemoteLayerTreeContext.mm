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

#import "GraphicsLayerCARemote.h"
#import "PlatformCALayerRemote.h"
#import "RemoteLayerTreeTransaction.h"
#import "RemoteLayerTreeHostMessages.h"
#import "WebPage.h"
#import <WebCore/FrameView.h>
#import <WebCore/MainFrame.h>
#import <WebCore/Page.h>
#import <wtf/TemporaryChange.h>

using namespace WebCore;

namespace WebKit {

RemoteLayerTreeContext::RemoteLayerTreeContext(WebPage* webPage)
    : m_webPage(webPage)
    , m_layerFlushTimer(this, &RemoteLayerTreeContext::layerFlushTimerFired)
{
}

RemoteLayerTreeContext::~RemoteLayerTreeContext()
{
}

void RemoteLayerTreeContext::setRootLayer(GraphicsLayer* rootLayer)
{
    if (!rootLayer) {
        m_rootLayer = nullptr;
        return;
    }

    m_rootLayer = static_cast<PlatformCALayerRemote*>(static_cast<GraphicsLayerCARemote*>(rootLayer)->platformCALayer());
}

void RemoteLayerTreeContext::layerWasCreated(PlatformCALayerRemote* layer, PlatformCALayer::LayerType type)
{
    RemoteLayerTreeTransaction::LayerCreationProperties creationProperties;
    creationProperties.layerID = layer->layerID();
    creationProperties.type = type;
    m_createdLayers.append(creationProperties);
}

void RemoteLayerTreeContext::layerWillBeDestroyed(PlatformCALayerRemote* layer)
{
    ASSERT(!m_destroyedLayers.contains(layer->layerID()));
    m_destroyedLayers.append(layer->layerID());
}

void RemoteLayerTreeContext::scheduleLayerFlush()
{
    if (m_layerFlushTimer.isActive())
        return;

    m_layerFlushTimer.startOneShot(0);
}

std::unique_ptr<GraphicsLayer> RemoteLayerTreeContext::createGraphicsLayer(GraphicsLayerClient* client)
{
    return std::make_unique<GraphicsLayerCARemote>(client, this);
}

void RemoteLayerTreeContext::layerFlushTimerFired(WebCore::Timer<RemoteLayerTreeContext>*)
{
    flushLayers();
}

void RemoteLayerTreeContext::flushLayers()
{
    if (!m_rootLayer)
        return;

    RemoteLayerTreeTransaction transaction;

    transaction.setRootLayerID(m_rootLayer->layerID());

    m_webPage->layoutIfNeeded();
    m_webPage->corePage()->mainFrame().view()->flushCompositingStateIncludingSubframes();

    transaction.setCreatedLayers(std::move(m_createdLayers));
    transaction.setDestroyedLayerIDs(std::move(m_destroyedLayers));
    m_rootLayer->recursiveBuildTransaction(transaction);

    m_webPage->send(Messages::RemoteLayerTreeHost::Commit(transaction));
}

} // namespace WebKit
