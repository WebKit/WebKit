/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "RemoteLayerTreeContext.h"
#include "RemoteLayerTreeDrawingAreaProxyMessages.h"
#include <WebCore/GraphicsLayerContentsDisplayDelegate.h>
#include <WebCore/Model.h>
#include <WebCore/PlatformScreen.h>

namespace WebKit {
using namespace WebCore;

GraphicsLayerCARemote::GraphicsLayerCARemote(Type layerType, GraphicsLayerClient& client, RemoteLayerTreeContext& context)
    : GraphicsLayerCA(layerType, client)
    , m_context(&context)
{
    context.graphicsLayerDidEnterContext(*this);
}

GraphicsLayerCARemote::~GraphicsLayerCARemote()
{
    if (m_context)
        m_context->graphicsLayerWillLeaveContext(*this);
}

bool GraphicsLayerCARemote::filtersCanBeComposited(const FilterOperations& filters)
{
    return PlatformCALayerRemote::filtersCanBeComposited(filters);
}

Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayer(PlatformCALayer::LayerType layerType, PlatformCALayerClient* owner)
{
    auto result = PlatformCALayerRemote::create(layerType, owner, *m_context);

    if (result->canHaveBackingStore())
        result->setWantsDeepColorBackingStore(screenSupportsExtendedColor());

    return WTFMove(result);
}

Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayer(PlatformLayer* platformLayer, PlatformCALayerClient* owner)
{
    return PlatformCALayerRemote::create(platformLayer, owner, *m_context);
}

#if ENABLE(MODEL_ELEMENT)
Ref<PlatformCALayer> GraphicsLayerCARemote::createPlatformCALayer(Ref<WebCore::Model> model, PlatformCALayerClient* owner)
{
    return PlatformCALayerRemote::create(model, owner, *m_context);
}
#endif

Ref<PlatformCAAnimation> GraphicsLayerCARemote::createPlatformCAAnimation(PlatformCAAnimation::AnimationType type, const String& keyPath)
{
    return PlatformCAAnimationRemote::create(type, keyPath);
}

void GraphicsLayerCARemote::moveToContext(RemoteLayerTreeContext& context)
{
    if (m_context)
        m_context->graphicsLayerWillLeaveContext(*this);

    m_context = &context;

    context.graphicsLayerDidEnterContext(*this);
}

Color GraphicsLayerCARemote::pageTiledBackingBorderColor() const
{
    return SRGBA<uint8_t> { 28, 74, 120, 128 }; // remote tile cache layer: navy blue
}

class GraphicsLayerCARemoteAsyncContentsDisplayDelegate : public GraphicsLayerAsyncContentsDisplayDelegate {
public:
    GraphicsLayerCARemoteAsyncContentsDisplayDelegate(IPC::Connection& connection, DrawingAreaIdentifier identifier, WebCore::GraphicsLayer::PlatformLayerID layerID)
        : m_connection(connection)
        , m_drawingArea(identifier)
        , m_layerID(layerID)
    { }

    bool tryCopyToLayer(ImageBuffer& buffer) final
    {
        auto clone = buffer.clone();
        if (!clone)
            return false;
        auto* backend = clone->ensureBackendCreated();
        if (!backend)
            return false;

        clone->flushDrawingContext();

        auto* sharing = dynamicDowncast<ImageBufferBackendHandleSharing>(backend->toBackendSharing());
        if (!sharing)
            return false;

        auto backendHandle = sharing->createBackendHandle(SharedMemory::Protection::ReadOnly);
        m_connection->send(Messages::RemoteLayerTreeDrawingAreaProxy::AsyncSetLayerContents(m_layerID, backendHandle), m_drawingArea.toUInt64());
        return true;
    }

private:
    Ref<IPC::Connection> m_connection;
    DrawingAreaIdentifier m_drawingArea;
    WebCore::GraphicsLayer::PlatformLayerID m_layerID;
};

RefPtr<WebCore::GraphicsLayerAsyncContentsDisplayDelegate> GraphicsLayerCARemote::createAsyncContentsDisplayDelegate()
{
    if (!m_context || !m_context->drawingAreaIdentifier() || !WebProcess::singleton().parentProcessConnection())
        return nullptr;

    return adoptRef(new GraphicsLayerCARemoteAsyncContentsDisplayDelegate(*WebProcess::singleton().parentProcessConnection(), m_context->drawingAreaIdentifier(), primaryLayerID()));
}


} // namespace WebKit
