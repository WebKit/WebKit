/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteLayerWithRemoteRenderingBackingStore.h"

#import "PlatformCALayerRemote.h"
#import "RemoteImageBufferSetProxy.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteRenderingBackendProxy.h"
#import "SwapBuffersDisplayRequirement.h"
#import <WebCore/PlatformCALayerClient.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerWithRemoteRenderingBackingStore);

using namespace WebCore;

RemoteLayerWithRemoteRenderingBackingStore::RemoteLayerWithRemoteRenderingBackingStore(PlatformCALayerRemote& layer)
    : RemoteLayerBackingStore(layer)
{
    RefPtr collection = backingStoreCollection();
    if (!collection) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_bufferSet = collection->layerTreeContext().ensureRemoteRenderingBackendProxy().createRemoteImageBufferSet();
}

RemoteLayerWithRemoteRenderingBackingStore::~RemoteLayerWithRemoteRenderingBackingStore()
{
    if (m_bufferSet)
        m_bufferSet->close();
}

bool RemoteLayerWithRemoteRenderingBackingStore::hasFrontBuffer() const
{
    return m_contentsBufferHandle || !m_cleared;
}

bool RemoteLayerWithRemoteRenderingBackingStore::frontBufferMayBeVolatile() const
{
    if (!m_bufferSet)
        return false;
    return m_bufferSet->requestedVolatility().contains(BufferInSetType::Front);
}

void RemoteLayerWithRemoteRenderingBackingStore::prepareToDisplay()
{
    m_contentsBufferHandle = std::nullopt;
}

void RemoteLayerWithRemoteRenderingBackingStore::clearBackingStore()
{
    m_contentsBufferHandle = std::nullopt;
    m_cleared = true;
}

std::unique_ptr<ThreadSafeImageBufferSetFlusher> RemoteLayerWithRemoteRenderingBackingStore::createFlusher(ThreadSafeImageBufferSetFlusher::FlushType flushType)
{
    if (!m_bufferSet)
        return { };
    return m_bufferSet->flushFrontBufferAsync(flushType);
}

void RemoteLayerWithRemoteRenderingBackingStore::createContextAndPaintContents()
{
    auto bufferSet = protectedBufferSet();
    if (!bufferSet)
        return;

    if (!bufferSet->hasContext()) {
        // The platform layer delegates display or bufferSet does not have a working connection to GPUP anymore.
        return;
    }

    drawInContext(bufferSet->context());
    m_cleared = false;
}

void RemoteLayerWithRemoteRenderingBackingStore::ensureBackingStore(const Parameters& parameters)
{
    if (m_parameters == parameters)
        return;

    m_parameters = parameters;
    clearBackingStore();
    if (m_bufferSet) {
        auto renderingMode = type() == RemoteLayerBackingStore::Type::IOSurface ? RenderingMode::Accelerated : RenderingMode::Unaccelerated;
        auto renderingPurpose = m_layer->containsBitmapOnly() ? WebCore::RenderingPurpose::BitmapOnlyLayerBacking : WebCore::RenderingPurpose::LayerBacking;
        m_bufferSet->setConfiguration(size(), scale(), colorSpace(), pixelFormat(), renderingMode, renderingPurpose);
    }
}

void RemoteLayerWithRemoteRenderingBackingStore::encodeBufferAndBackendInfos(IPC::Encoder& encoder) const
{
    auto encodeBuffer = [&](const  std::optional<WebCore::RenderingResourceIdentifier>& bufferIdentifier) {
        if (bufferIdentifier) {
            encoder << std::optional { BufferAndBackendInfo { *bufferIdentifier, m_bufferSet->generation() } };
            return;
        }

        encoder << std::optional<BufferAndBackendInfo>();
    };

    encodeBuffer(m_bufferCacheIdentifiers.front);
    encodeBuffer(m_bufferCacheIdentifiers.back);
    encodeBuffer(m_bufferCacheIdentifiers.secondaryBack);
}

std::optional<RemoteImageBufferSetIdentifier> RemoteLayerWithRemoteRenderingBackingStore::bufferSetIdentifier() const
{
    if (!m_bufferSet)
        return std::nullopt;
    return m_bufferSet->identifier();
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
std::optional<ImageBufferBackendHandle> RemoteLayerWithRemoteRenderingBackingStore::displayListHandle() const
{
    return m_bufferSet ? m_bufferSet->dynamicContentScalingDisplayList() : std::nullopt;
}
#endif

void RemoteLayerWithRemoteRenderingBackingStore::dump(WTF::TextStream& ts) const
{
    ts.dumpProperty("buffer set", m_bufferSet);
    ts.dumpProperty("cache identifiers", m_bufferCacheIdentifiers);
    ts.dumpProperty("is opaque", isOpaque());
}

} // namespace WebKit

