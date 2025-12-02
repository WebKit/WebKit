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

#include "config.h"
#include "RemoteImageBufferSet.h"

#include "ImageBufferBackendHandleSharing.h"
#include "Logging.h"
#include "RemoteGraphicsContext.h"
#include "RemoteImageBufferGraphicsContext.h"
#include "RemoteImageBufferSetMessages.h"
#include "RemoteRenderingBackend.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/NullImageBufferBackend.h>

#if ENABLE(GPU_PROCESS)

#define MESSAGE_CHECK(assertion, message) MESSAGE_CHECK_WITH_MESSAGE_BASE(assertion, &m_renderingBackend->streamConnection(), message)

namespace WebKit {

Ref<RemoteImageBufferSet> RemoteImageBufferSet::create(ImageBufferSetIdentifier identifier, RemoteGraphicsContextIdentifier contextIdentifier, RemoteRenderingBackend& renderingBackend)
{
    auto instance = adoptRef(*new RemoteImageBufferSet(identifier, contextIdentifier, renderingBackend));
    instance->startListeningForIPC();
    return instance;
}

RemoteImageBufferSet::RemoteImageBufferSet(ImageBufferSetIdentifier identifier, RemoteGraphicsContextIdentifier contextIdentifier, RemoteRenderingBackend& renderingBackend)
    : ImageBufferSet(identifier)
    , m_contextIdentifier(contextIdentifier)
    , m_renderingBackend(renderingBackend)
{
}

RemoteImageBufferSet::~RemoteImageBufferSet()
{
    RefPtr frontBuffer = m_frontBuffer;
    // Volatile image buffers do not have contexts.
    if (!frontBuffer || frontBuffer->volatilityState() == WebCore::VolatilityState::Volatile)
        return;
    if (!frontBuffer->hasBackend())
        return;
    // Unwind the context's state stack before destruction, since calls to restore may not have
    // been flushed yet, or the web process may have terminated.
    auto& context = frontBuffer->context();
    context.unwindStateStack();
}

void RemoteImageBufferSet::startListeningForIPC()
{
    m_renderingBackend->streamConnection().startReceivingMessages(*this, Messages::RemoteImageBufferSet::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteImageBufferSet::stopListeningForIPC()
{
    m_renderingBackend->streamConnection().stopReceivingMessages(Messages::RemoteImageBufferSet::messageReceiverName(), m_identifier.toUInt64());
}

IPC::StreamConnectionWorkQueue& RemoteImageBufferSet::workQueue() const
{
    return m_renderingBackend->workQueue();
}

void RemoteImageBufferSet::updateConfiguration(const RemoteImageBufferSetConfiguration& configuration)
{
    m_configuration = configuration;
    clearBuffers();
}

void RemoteImageBufferSet::endPrepareForDisplay(RenderingUpdateID renderingUpdateID, CompletionHandler<void(ImageBufferSetPrepareBufferForDisplayOutputData, RenderingUpdateID)>&& completionHandler)
{
    m_context.reset();

    RefPtr frontBuffer = m_frontBuffer;
    if (frontBuffer)
        frontBuffer->flushDrawingContext();

    auto bufferIdentifier = [](RefPtr<WebCore::ImageBuffer> buffer) -> std::optional<WebCore::RenderingResourceIdentifier> {
        if (!buffer)
            return std::nullopt;
        return buffer->renderingResourceIdentifier();
    };

    ImageBufferSetPrepareBufferForDisplayOutputData outputData;
    if (frontBuffer) {
        auto* sharing = frontBuffer->toBackendSharing();
        outputData.backendHandle = downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();
    }

    outputData.bufferCacheIdentifiers = BufferIdentifierSet { bufferIdentifier(frontBuffer), bufferIdentifier(m_backBuffer), bufferIdentifier(m_secondaryBackBuffer) };
    completionHandler(WTFMove(outputData), renderingUpdateID);
}

// This is the GPU Process version of RemoteLayerBackingStore::prepareBuffers().
void RemoteImageBufferSet::ensureBufferForDisplay(ImageBufferSetPrepareBufferForDisplayInputData& inputData, SwapBuffersDisplayRequirement& displayRequirement, bool isSync)
{
    assertIsCurrent(workQueue());
    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "GPU Process: ::ensureFrontBufferForDisplay " << " - front "
        << m_frontBuffer << " (in-use " << (m_frontBuffer && protectedFrontBuffer()->isInUse()) << ") "
        << m_backBuffer << " (in-use " << (m_backBuffer && protectedBackBuffer()->isInUse()) << ") "
        << m_secondaryBackBuffer << " (in-use " << (m_secondaryBackBuffer && protectedSecondaryBackBuffer()->isInUse()) << ") ");

    displayRequirement = swapBuffersForDisplay(inputData.hasEmptyDirtyRegion, inputData.supportsPartialRepaint && !isSmallLayerBacking({ m_configuration.logicalSize, m_configuration.resolutionScale, m_configuration.colorSpace, m_configuration.bufferFormat, m_configuration.renderingPurpose }));
    if (displayRequirement == SwapBuffersDisplayRequirement::NeedsFullDisplay) {
        auto layerBounds = WebCore::IntRect { { }, expandedIntSize(m_configuration.logicalSize) };
        MESSAGE_CHECK(isSync || inputData.dirtyRegion.contains(layerBounds), "Can't asynchronously require full display for a buffer set");
        inputData.dirtyRegion = layerBounds;
    }

    if (!m_frontBuffer) {
        WebCore::ImageBufferCreationContext creationContext;
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
        if (m_configuration.includeDisplayList == WebCore::IncludeDynamicContentScalingDisplayList::Yes)
            creationContext.dynamicContentScalingResourceCache = ensureDynamicContentScalingResourceCache();
#endif
        m_frontBuffer = m_renderingBackend->allocateImageBuffer(m_configuration.logicalSize, m_configuration.renderingMode, m_configuration.renderingPurpose, m_configuration.resolutionScale, m_configuration.colorSpace, m_configuration.bufferFormat, WTFMove(creationContext));
        m_frontBufferIsCleared = true;
    }

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "GPU Process: ensureFrontBufferForDisplay - swapped to ["
        << m_frontBuffer << ", " << m_backBuffer << ", " << m_secondaryBackBuffer << "]");

    if (displayRequirement != SwapBuffersDisplayRequirement::NeedsNoDisplay) {
        RefPtr imageBuffer = m_frontBuffer;
        if (!imageBuffer) {
            imageBuffer = WebCore::ImageBuffer::create<WebCore::NullImageBufferBackend>({ 0, 0 }, 1, WebCore::DestinationColorSpace::SRGB(), { WebCore::PixelFormat::BGRA8 }, WebCore::RenderingPurpose::Unspecified, { });
            RELEASE_ASSERT(imageBuffer);
        }
        m_context = RemoteImageBufferGraphicsContext::create(*imageBuffer, m_contextIdentifier, m_renderingBackend);
    }
}

void RemoteImageBufferSet::prepareBufferForDisplay(const WebCore::Region& dirtyRegion, bool requiresClearedPixels)
{
    PaintRectList paintingRects = computePaintingRects(dirtyRegion, m_configuration.resolutionScale);
    WebCore::FloatRect layerBounds { { }, m_configuration.logicalSize };

    ImageBufferSet::prepareBufferForDisplay(layerBounds, dirtyRegion, paintingRects, requiresClearedPixels);
}

bool RemoteImageBufferSet::makeBuffersVolatile(OptionSet<BufferInSetType> requestedBuffers, OptionSet<BufferInSetType>& volatileBuffers, bool forcePurge)
{
    bool allSucceeded = true;

    auto makeVolatile = [&](WebCore::ImageBuffer& imageBuffer) {
        if (forcePurge) {
            imageBuffer.setVolatileAndPurgeForTesting();
            return true;
        }
        imageBuffer.releaseGraphicsContext();
        return imageBuffer.setVolatile();
    };

    auto makeBufferTypeVolatile = [&](BufferInSetType type, RefPtr<WebCore::ImageBuffer> imageBuffer) {
        if (requestedBuffers.contains(type) && imageBuffer) {
            if (makeVolatile(*imageBuffer))
                volatileBuffers.add(type);
            else
                allSucceeded = false;
        }
    };

    makeBufferTypeVolatile(BufferInSetType::Front, m_frontBuffer);
    makeBufferTypeVolatile(BufferInSetType::Back, m_backBuffer);
    makeBufferTypeVolatile(BufferInSetType::SecondaryBack, m_secondaryBackBuffer);

    return allSucceeded;
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
void RemoteImageBufferSet::dynamicContentScalingDisplayList(CompletionHandler<void(std::optional<WebCore::DynamicContentScalingDisplayList>&&)>&& completionHandler)
{
    std::optional<WebCore::DynamicContentScalingDisplayList> displayList;
    if (m_frontBuffer)
        displayList = m_frontBuffer->dynamicContentScalingDisplayList();
    completionHandler({ WTFMove(displayList) });
}

DynamicContentScalingResourceCache RemoteImageBufferSet::ensureDynamicContentScalingResourceCache()
{
    if (!m_dynamicContentScalingResourceCache)
        m_dynamicContentScalingResourceCache = DynamicContentScalingResourceCache::create();
    return m_dynamicContentScalingResourceCache;
}
#endif

} // namespace WebKit

#undef MESSAGE_CHECK

#endif
