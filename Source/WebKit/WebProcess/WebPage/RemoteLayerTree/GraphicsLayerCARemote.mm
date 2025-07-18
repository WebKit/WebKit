/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#if ENABLE(MODEL_PROCESS)
#include <WebCore/ModelContext.h>
#endif

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
    if (RefPtr context = m_context.get())
        context->graphicsLayerWillLeaveContext(*this);
}

bool GraphicsLayerCARemote::filtersCanBeComposited(const FilterOperations& filters)
{
    return PlatformCALayerRemote::filtersCanBeComposited(filters);
}

Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayer(PlatformCALayer::LayerType layerType, PlatformCALayerClient* owner)
{
    Ref context = *m_context;
    Ref result = PlatformCALayerRemote::create(layerType, owner, context.get());

    if (result->canHaveBackingStore()) {
        RefPtr localMainFrameView = context->protectedWebPage()->localMainFrameView();
        result->setContentsFormat(PlatformCALayer::contentsFormatForLayer(owner));
    }
    return WTFMove(result);
}

Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayer(PlatformLayer* platformLayer, PlatformCALayerClient* owner)
{
    Ref context = *m_context;
    return PlatformCALayerRemote::create(platformLayer, owner, context.get());
}

#if ENABLE(MODEL_PROCESS)
Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayer(Ref<WebCore::ModelContext> modelContext, PlatformCALayerClient* owner)
{
    Ref context = *m_context;
    return PlatformCALayerRemote::create(modelContext, owner, context.get());
}
#endif

#if ENABLE(MODEL_ELEMENT)
Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayer(Ref<WebCore::Model> model, PlatformCALayerClient* owner)
{
    Ref context = *m_context;
    return PlatformCALayerRemote::create(model, owner, context.get());
}
#endif

Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayerHost(WebCore::LayerHostingContextIdentifier identifier, PlatformCALayerClient* owner)
{
    Ref context = *m_context;
    return PlatformCALayerRemoteHost::create(identifier, owner, context.get());
}

#if HAVE(AVKIT)
Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformVideoLayer(WebCore::HTMLVideoElement& videoElement, PlatformCALayerClient* owner)
{
    Ref context = *m_context;
    return PlatformCALayerRemote::create(videoElement, owner, context.get());
}
#endif

Ref<PlatformCAAnimation> GraphicsLayerCARemote::createPlatformCAAnimation(PlatformCAAnimation::AnimationType type, const String& keyPath)
{
    return PlatformCAAnimationRemote::create(type, keyPath);
}

void GraphicsLayerCARemote::moveToContext(RemoteLayerTreeContext& context)
{
    if (RefPtr oldContext = m_context.get())
        oldContext->graphicsLayerWillLeaveContext(*this);

    m_context = context;

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

    bool tryCopyToLayer(ImageBuffer& buffer, bool opaque) final
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
            m_contentsFormat = convertToContentsFormat(clone->pixelFormat());
            m_opaque = opaque;
        }

        RemoteLayerBackingStoreProperties properties(WTFMove(*backendHandle), clone->renderingResourceIdentifier(), opaque);
        m_connection->send(Messages::RemoteLayerTreeDrawingAreaProxy::AsyncSetLayerContents(*m_layerID, WTFMove(properties)), m_drawingArea.toUInt64());
        return true;
    }

    void display(PlatformCALayer& layer) final
    {
        Locker locker { m_surfaceLock };
        if (m_surfaceBackendHandle) {
            downcast<PlatformCALayerRemote>(layer).setOpaque(m_opaque);
            downcast<PlatformCALayerRemote>(layer).setContentsFormat(m_contentsFormat);
            downcast<PlatformCALayerRemote>(layer).setRemoteDelegatedContents({ ImageBufferBackendHandle { *m_surfaceBackendHandle }, { }, std::optional<RenderingResourceIdentifier>(m_surfaceIdentifier) });
        }
    }

    void setDestinationLayerID(WebCore::PlatformLayerIdentifier layerID)
    {
        m_layerID = layerID;
    }

    bool isGraphicsLayerCARemoteAsyncContentsDisplayDelegate() const final { return true; }

private:
    const Ref<IPC::Connection> m_connection;
    DrawingAreaIdentifier m_drawingArea;
    Markable<WebCore::PlatformLayerIdentifier> m_layerID;
    Lock m_surfaceLock;
    std::optional<ImageBufferBackendHandle> m_surfaceBackendHandle WTF_GUARDED_BY_LOCK(m_surfaceLock);
    Markable<WebCore::RenderingResourceIdentifier> m_surfaceIdentifier WTF_GUARDED_BY_LOCK(m_surfaceLock);
    ContentsFormat m_contentsFormat WTF_GUARDED_BY_LOCK(m_surfaceLock) { ContentsFormat::RGBA8 };
    bool m_opaque WTF_GUARDED_BY_LOCK(m_surfaceLock) { false };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::GraphicsLayerCARemoteAsyncContentsDisplayDelegate)
static bool isType(const WebCore::GraphicsLayerAsyncContentsDisplayDelegate& delegate) { return delegate.isGraphicsLayerCARemoteAsyncContentsDisplayDelegate(); }
SPECIALIZE_TYPE_TRAITS_END()

namespace WebKit {

RefPtr<WebCore::GraphicsLayerAsyncContentsDisplayDelegate> GraphicsLayerCARemote::createAsyncContentsDisplayDelegate(GraphicsLayerAsyncContentsDisplayDelegate* existing)
{
    RefPtr context = m_context.get();
    if (!context || !context->drawingAreaIdentifier() || !WebProcess::singleton().parentProcessConnection())
        return nullptr;

    RefPtr delegate = dynamicDowncast<GraphicsLayerCARemoteAsyncContentsDisplayDelegate>(existing);
    if (!delegate) {
        ASSERT(!existing);
        delegate = adoptRef(new GraphicsLayerCARemoteAsyncContentsDisplayDelegate(*WebProcess::singleton().parentProcessConnection(), *context->drawingAreaIdentifier()));
    }

    auto layerID = setContentsToAsyncDisplayDelegate(delegate, ContentsLayerPurpose::Canvas);

    delegate->setDestinationLayerID(layerID);
    return delegate;
}

bool GraphicsLayerCARemote::shouldDirectlyCompositeImageBuffer(ImageBuffer* image) const
{
    return !!dynamicDowncast<ImageBufferBackendHandleSharing>(image->toBackendSharing());
}

void GraphicsLayerCARemote::setLayerContentsToImageBuffer(PlatformCALayer* layer, ImageBuffer* image)
{
    if (!image)
        return;

    image->flushDrawingContextAsync();

    auto* sharing = dynamicDowncast<ImageBufferBackendHandleSharing>(image->toBackendSharing());
    if (!sharing)
        return;

    auto backendHandle = sharing->createBackendHandle(SharedMemory::Protection::ReadOnly);
    ASSERT(backendHandle);

    layer->setAcceleratesDrawing(true);
#if HAVE(SUPPORT_HDR_DISPLAY)
    layer->setTonemappingEnabled(true);
#endif
    downcast<PlatformCALayerRemote>(layer)->setRemoteDelegatedContents({ ImageBufferBackendHandle { *backendHandle }, { }, std::nullopt  });
}

GraphicsLayer::LayerMode GraphicsLayerCARemote::layerMode() const
{
    return GraphicsLayer::LayerMode::LayerHostingContextId;
}

} // namespace WebKit
