/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsLayerCARemote.h"

#include "ImageBufferBackendHandleSharing.h"
#include "PlatformCAAnimationRemote.h"
#include "PlatformCALayerRemote.h"
#include "PlatformCALayerRemoteHost.h"
#include "RemoteLayerTreeContext.h"
#include "RemoteLayerTreeDrawingAreaProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/GraphicsLayerContentsDisplayDelegate.h>
#include <WebCore/HTMLVideoElement.h>
#include <WebCore/Model.h>
#include <WebCore/PlatformCALayerDelegatedContents.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/RemoteFrame.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(GraphicsLayerCARemote);

GraphicsLayerCARemote::GraphicsLayerCARemote(Type layerType, GraphicsLayerClient& client, RemoteLayerTreeContext& context)
    : GraphicsLayerCA(layerType, client)
    , m_context(&context)
{
    context.graphicsLayerDidEnterContext(*this);
}

GraphicsLayerCARemote::~GraphicsLayerCARemote()
{
    if (RefPtr<RemoteLayerTreeContext> protectedContext = m_context.get())
        protectedContext->graphicsLayerWillLeaveContext(*this);
}

bool GraphicsLayerCARemote::filtersCanBeComposited(const FilterOperations& filters)
{
    return PlatformCALayerRemote::filtersCanBeComposited(filters);
}

Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayer(PlatformCALayer::LayerType layerType, PlatformCALayerClient* owner)
{
    RELEASE_ASSERT(m_context.get());
    auto result = PlatformCALayerRemote::create(layerType, owner, *m_context);

    if (result->canHaveBackingStore()) {
        auto* localMainFrameView = m_context->webPage().localMainFrameView();
        result->setContentsFormat(screenContentsFormat(localMainFrameView, owner));
    }

    return WTFMove(result);
}

Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayer(PlatformLayer* platformLayer, PlatformCALayerClient* owner)
{
    RELEASE_ASSERT(m_context.get());
    return PlatformCALayerRemote::create(platformLayer, owner, *m_context);
}

Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayer(WebCore::LayerHostingContextIdentifier layerHostingContextIdentifier, PlatformCALayerClient* owner)
{
    RELEASE_ASSERT(m_context.get());
    return PlatformCALayerRemote::create(layerHostingContextIdentifier.toUInt64(), owner, *m_context);
}

#if ENABLE(MODEL_ELEMENT)
Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayer(Ref<WebCore::Model> model, PlatformCALayerClient* owner)
{
    RELEASE_ASSERT(m_context.get());
    return PlatformCALayerRemote::create(model, owner, *m_context);
}
#endif

Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayerHost(WebCore::LayerHostingContextIdentifier identifier, PlatformCALayerClient* owner)
{
    RELEASE_ASSERT(m_context.get());
    return PlatformCALayerRemoteHost::create(identifier, owner, *m_context);
}

#if HAVE(AVKIT)
Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformVideoLayer(WebCore::HTMLVideoElement& videoElement, PlatformCALayerClient* owner)
{
    RELEASE_ASSERT(m_context.get());
    return PlatformCALayerRemote::create(videoElement, owner, *m_context);
}
#endif

Ref<PlatformCAAnimation> GraphicsLayerCARemote::createPlatformCAAnimation(PlatformCAAnimation::AnimationType type, const String& keyPath)
{
    return PlatformCAAnimationRemote::create(type, keyPath);
}

void GraphicsLayerCARemote::moveToContext(RemoteLayerTreeContext& context)
{
    if (RefPtr protectedContext = m_context.get())
        protectedContext->graphicsLayerWillLeaveContext(*this);

    m_context = &context;

    context.graphicsLayerDidEnterContext(*this);
}

Color GraphicsLayerCARemote::pageTiledBackingBorderColor() const
{
    return SRGBA<uint8_t> { 28, 74, 120, 128 }; // remote tile cache layer: navy blue
}

class GraphicsLayerCARemoteAsyncContentsDisplayDelegate : public GraphicsLayerAsyncContentsDisplayDelegate {
public:
    GraphicsLayerCARemoteAsyncContentsDisplayDelegate(IPC::Connection& connection, DrawingAreaIdentifier identifier)
        : m_connection(connection)
        , m_drawingArea(identifier)
    { }

    bool tryCopyToLayer(ImageBuffer& buffer) final
    {
        auto clone = buffer.clone();
        if (!clone)
            return false;

        clone->flushDrawingContext();

        auto* sharing = dynamicDowncast<ImageBufferBackendHandleSharing>(clone->toBackendSharing());
        if (!sharing)
            return false;

        auto backendHandle = sharing->createBackendHandle(SharedMemory::Protection::ReadOnly);
        ASSERT(backendHandle);

        {
            Locker locker { m_surfaceLock };
            m_surfaceBackendHandle = ImageBufferBackendHandle { *backendHandle };
            m_surfaceIdentifier = clone->renderingResourceIdentifier();
        }

        m_connection->send(Messages::RemoteLayerTreeDrawingAreaProxy::AsyncSetLayerContents(*m_layerID, WTFMove(*backendHandle), clone->renderingResourceIdentifier()), m_drawingArea.toUInt64());

        return true;
    }

    void display(PlatformCALayer& layer) final
    {
        Locker locker { m_surfaceLock };
        if (m_surfaceBackendHandle)
            downcast<PlatformCALayerRemote>(layer).setRemoteDelegatedContents({ ImageBufferBackendHandle { *m_surfaceBackendHandle }, { }, std::optional<RenderingResourceIdentifier>(m_surfaceIdentifier) });
    }

    void setDestinationLayerID(WebCore::PlatformLayerIdentifier layerID)
    {
        m_layerID = layerID;
    }

    bool isGraphicsLayerCARemoteAsyncContentsDisplayDelegate() const final { return true; }

private:
    Ref<IPC::Connection> m_connection;
    DrawingAreaIdentifier m_drawingArea;
    Markable<WebCore::PlatformLayerIdentifier> m_layerID;
    Lock m_surfaceLock;
    std::optional<ImageBufferBackendHandle> m_surfaceBackendHandle WTF_GUARDED_BY_LOCK(m_surfaceLock);
    Markable<WebCore::RenderingResourceIdentifier> m_surfaceIdentifier WTF_GUARDED_BY_LOCK(m_surfaceLock);
};

RefPtr<WebCore::GraphicsLayerAsyncContentsDisplayDelegate> GraphicsLayerCARemote::createAsyncContentsDisplayDelegate(GraphicsLayerAsyncContentsDisplayDelegate* existing)
{
    RefPtr protectedContext = m_context.get();
    if (!protectedContext || !protectedContext->drawingAreaIdentifier() || !WebProcess::singleton().parentProcessConnection())
        return nullptr;

    RefPtr<GraphicsLayerCARemoteAsyncContentsDisplayDelegate> delegate;
    if (existing && existing->isGraphicsLayerCARemoteAsyncContentsDisplayDelegate())
        delegate = static_cast<GraphicsLayerCARemoteAsyncContentsDisplayDelegate*>(existing);

    if (!delegate) {
        ASSERT(!existing);
        delegate = adoptRef(new GraphicsLayerCARemoteAsyncContentsDisplayDelegate(*WebProcess::singleton().parentProcessConnection(), *protectedContext->drawingAreaIdentifier()));
    }

    auto layerID = setContentsToAsyncDisplayDelegate(delegate, ContentsLayerPurpose::Canvas);

    delegate->setDestinationLayerID(layerID);
    return delegate;
}

GraphicsLayer::LayerMode GraphicsLayerCARemote::layerMode() const
{
    if (m_context && m_context->layerHostingMode() == LayerHostingMode::InProcess)
        return GraphicsLayer::LayerMode::PlatformLayer;
    return GraphicsLayer::LayerMode::LayerHostingContextId;
}

} // namespace WebKit
