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
#include "RemoteImageBufferSetMessages.h"
#include "RemoteRenderingBackend.h"

#if PLATFORM(COCOA)
#include <WebCore/PlatformCALayer.h>
#endif

#if ENABLE(GPU_PROCESS)

#define MESSAGE_CHECK(assertion, backend, message) do { \
    if (UNLIKELY(!(assertion))) { \
        backend.terminateWebProcess(message); \
        return; \
    } \
} while (0)

namespace WebKit {

Ref<RemoteImageBufferSet> RemoteImageBufferSet::create(RemoteImageBufferSetIdentifier identifier, RenderingResourceIdentifier displayListIdentifier, RemoteRenderingBackend& backend)
{
    auto instance = adoptRef(*new RemoteImageBufferSet(identifier, displayListIdentifier, backend));
    instance->startListeningForIPC();
    return instance;
}

RemoteImageBufferSet::RemoteImageBufferSet(RemoteImageBufferSetIdentifier identifier, RenderingResourceIdentifier displayListIdentifier, RemoteRenderingBackend& backend)
    : m_backend(&backend)
    , m_identifier(identifier)
    , m_displayListIdentifier(displayListIdentifier)
{
}

RemoteImageBufferSet::~RemoteImageBufferSet() = default;

void RemoteImageBufferSet::startListeningForIPC()
{
    m_backend->streamConnection().startReceivingMessages(*this, Messages::RemoteImageBufferSet::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteImageBufferSet::stopListeningForIPC()
{
    if (auto backend = std::exchange(m_backend, { }))
        backend->streamConnection().stopReceivingMessages(Messages::RemoteImageBufferSet::messageReceiverName(), m_identifier.toUInt64());
}

IPC::StreamConnectionWorkQueue& RemoteImageBufferSet::workQueue() const
{
    return m_backend->workQueue();
}

void RemoteImageBufferSet::updateConfiguration(const FloatSize& logicalSize, RenderingMode renderingMode, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat)
{
    m_logicalSize = logicalSize;
    m_renderingMode = renderingMode;
    m_resolutionScale = resolutionScale;
    m_colorSpace = colorSpace;
    m_pixelFormat = pixelFormat;
    m_frontBuffer = m_backBuffer = m_secondaryBackBuffer = nullptr;

}

void RemoteImageBufferSet::setFlushSignal(IPC::Signal&& signal)
{
    m_flushSignal = WTFMove(signal);
}

void RemoteImageBufferSet::flush()
{
    m_backend->releaseDisplayListRecorder(m_displayListIdentifier);
    ASSERT(m_flushSignal);
    if (m_frontBuffer)
        m_frontBuffer->flushDrawingContext();
    m_flushSignal->signal();
}

// This is the GPU Process version of RemoteLayerBackingStore::prepareBuffers().
void RemoteImageBufferSet::ensureBufferForDisplay(ImageBufferSetPrepareBufferForDisplayInputData& inputData, ImageBufferSetPrepareBufferForDisplayOutputData& outputData)
{
    assertIsCurrent(workQueue());
    auto bufferIdentifier = [](RefPtr<WebCore::ImageBuffer> buffer) -> std::optional<RenderingResourceIdentifier> {
        if (!buffer)
            return std::nullopt;
        return buffer->renderingResourceIdentifier();
    };

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "GPU Process: ::ensureFrontBufferForDisplay " << " - front "
        << m_frontBuffer << " (in-use " << (m_frontBuffer && m_frontBuffer->isInUse()) << ") "
        << m_backBuffer << " (in-use " << (m_backBuffer && m_backBuffer->isInUse()) << ") "
        << m_secondaryBackBuffer << " (in-use " << (m_secondaryBackBuffer && m_secondaryBackBuffer->isInUse()) << ") ");

    m_previousFrontBuffer = m_frontBuffer;

    bool needsFullDisplay = false;
    if (m_frontBuffer) {
        auto previousState = m_frontBuffer->setNonVolatile();
        if (previousState == SetNonVolatileResult::Empty) {
            needsFullDisplay = true;
            inputData.dirtyRegion = IntRect { { }, expandedIntSize(m_logicalSize) };
        }
    }

    if (!m_frontBuffer || !inputData.supportsPartialRepaint || isSmallLayerBacking(m_frontBuffer->parameters()))
        m_previouslyPaintedRect = std::nullopt;

    if (!m_backBuffer || m_backBuffer->isInUse()) {
        std::swap(m_backBuffer, m_secondaryBackBuffer);

        // When pulling the secondary back buffer out of hibernation (to become
        // the new front buffer), if it is somehow still in use (e.g. we got
        // three swaps ahead of the render server), just give up and discard it.
        if (m_backBuffer && m_backBuffer->isInUse())
            m_backBuffer = nullptr;

        m_previouslyPaintedRect = std::nullopt;
    }

    if (m_frontBuffer && !needsFullDisplay && inputData.hasEmptyDirtyRegion) {
        // No swap necessary.
        outputData.displayRequirement = SwapBuffersDisplayRequirement::NeedsNoDisplay;
    } else {
        outputData.displayRequirement = needsFullDisplay ? SwapBuffersDisplayRequirement::NeedsFullDisplay : SwapBuffersDisplayRequirement::NeedsNormalDisplay;

        std::swap(m_frontBuffer, m_backBuffer);
    }

    if (m_frontBuffer) {
        auto previousState = m_frontBuffer->setNonVolatile();
        if (previousState == SetNonVolatileResult::Empty)
            m_previouslyPaintedRect = std::nullopt;
    } else {
        m_frontBuffer = m_backend->allocateImageBuffer(m_logicalSize, m_renderingMode, WebCore::RenderingPurpose::LayerBacking, m_resolutionScale, m_colorSpace, m_pixelFormat, RenderingResourceIdentifier::generate());
        m_frontBufferIsCleared = true;
    }

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "GPU Process: ensureFrontBufferForDisplay - swapped to ["
        << m_frontBuffer << ", " << m_backBuffer << ", " << m_secondaryBackBuffer << "]");

    if (outputData.displayRequirement != SwapBuffersDisplayRequirement::NeedsNoDisplay) {
        m_backend->createDisplayListRecorder(m_frontBuffer, m_displayListIdentifier);
        if (m_frontBuffer) {
            auto* sharing = m_frontBuffer->toBackendSharing();
            outputData.backendHandle = downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();
        }
    }

    outputData.bufferCacheIdentifiers = BufferIdentifierSet { bufferIdentifier(m_frontBuffer), bufferIdentifier(m_backBuffer), bufferIdentifier(m_secondaryBackBuffer) };
}

void RemoteImageBufferSet::prepareBufferForDisplay(const WebCore::Region& dirtyRegion, bool requiresClearedPixels)
{
    if (!m_frontBuffer) {
        m_previouslyPaintedRect = std::nullopt;
        return;
    }

    FloatRect bufferBounds { { }, m_logicalSize };

    GraphicsContext& context = m_frontBuffer->context();
    context.resetClip();

    FloatRect copyRect;
    if (m_previousFrontBuffer && m_frontBuffer != m_previousFrontBuffer) {
        IntRect enclosingCopyRect { m_previouslyPaintedRect ? *m_previouslyPaintedRect : enclosingIntRect(bufferBounds) };
        if (!dirtyRegion.contains(enclosingCopyRect)) {
            Region copyRegion(enclosingCopyRect);
            copyRegion.subtract(dirtyRegion);
            copyRect = intersection(copyRegion.bounds(), bufferBounds);
            if (!copyRect.isEmpty())
                m_frontBuffer->context().drawImageBuffer(*m_previousFrontBuffer, copyRect, copyRect, { CompositeOperator::Copy });
        }
    }

    auto dirtyRects = dirtyRegion.rects();
#if PLATFORM(COCOA)
    IntRect dirtyBounds = dirtyRegion.bounds();
    if (dirtyRects.size() > PlatformCALayer::webLayerMaxRectsToPaint || dirtyRegion.totalArea() > PlatformCALayer::webLayerWastedSpaceThreshold * dirtyBounds.width() * dirtyBounds.height()) {
        dirtyRects.clear();
        dirtyRects.append(dirtyBounds);
    }
#endif

    Vector<FloatRect, 5> paintingRects;
    for (const auto& rect : dirtyRects) {
        FloatRect scaledRect(rect);
        scaledRect.scale(m_resolutionScale);
        scaledRect = enclosingIntRect(scaledRect);
        scaledRect.scale(1 / m_resolutionScale);
        paintingRects.append(scaledRect);

        // If the copy-forward touched pixels that are about to be painted, then they
        // won't be 'clear' any more.
        if (copyRect.intersects(scaledRect))
            m_frontBufferIsCleared = false;
    }

    if (paintingRects.size() == 1)
        context.clip(paintingRects[0]);
    else {
        Path clipPath;
        for (auto rect : paintingRects)
            clipPath.addRect(rect);
        context.clipPath(clipPath);
    }

    if (requiresClearedPixels && !m_frontBufferIsCleared)
        context.clearRect(bufferBounds);

    m_previouslyPaintedRect = dirtyRegion.bounds();
    m_previousFrontBuffer = nullptr;
    m_frontBufferIsCleared = false;
}

bool RemoteImageBufferSet::makeBuffersVolatile(OptionSet<BufferInSetType> requestedBuffers, OptionSet<BufferInSetType>& volatileBuffers)
{
    bool allSucceeded = true;

    auto makeVolatile = [](ImageBuffer& imageBuffer) {
        imageBuffer.releaseGraphicsContext();
        return imageBuffer.setVolatile();
    };

    auto makeBufferTypeVolatile = [&](BufferInSetType type, RefPtr<ImageBuffer> imageBuffer) {
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
#endif

} // namespace WebKit

#undef MESSAGE_CHECK

#endif
