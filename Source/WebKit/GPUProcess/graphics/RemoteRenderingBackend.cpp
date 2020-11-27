/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RemoteRenderingBackend.h"

#if ENABLE(GPU_PROCESS)

#include "DisplayListReaderHandle.h"
#include "GPUConnectionToWebProcess.h"
#include "PlatformRemoteImageBuffer.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/SystemTracing.h>

namespace WebKit {
using namespace WebCore;

std::unique_ptr<RemoteRenderingBackend> RemoteRenderingBackend::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RenderingBackendIdentifier renderingBackendIdentifier)
{
    return std::unique_ptr<RemoteRenderingBackend>(new RemoteRenderingBackend(gpuConnectionToWebProcess, renderingBackendIdentifier));
}

RemoteRenderingBackend::RemoteRenderingBackend(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RenderingBackendIdentifier renderingBackendIdentifier)
    : m_gpuConnectionToWebProcess(makeWeakPtr(gpuConnectionToWebProcess))
    , m_renderingBackendIdentifier(renderingBackendIdentifier)
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        gpuConnectionToWebProcess->messageReceiverMap().addMessageReceiver(Messages::RemoteRenderingBackend::messageReceiverName(), renderingBackendIdentifier.toUInt64(), *this);
}

RemoteRenderingBackend::~RemoteRenderingBackend()
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        gpuConnectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteRenderingBackend::messageReceiverName(), m_renderingBackendIdentifier.toUInt64());
}

GPUConnectionToWebProcess* RemoteRenderingBackend::gpuConnectionToWebProcess() const
{
    return m_gpuConnectionToWebProcess.get();
}

IPC::Connection* RemoteRenderingBackend::messageSenderConnection() const
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        return &gpuConnectionToWebProcess->connection();
    return nullptr;
}

uint64_t RemoteRenderingBackend::messageSenderDestinationID() const
{
    return m_renderingBackendIdentifier.toUInt64();
}

bool RemoteRenderingBackend::applyMediaItem(DisplayList::ItemHandle item, GraphicsContext& context)
{
    if (!item.is<DisplayList::PaintFrameForMedia>())
        return false;

    auto& mediaItem = item.get<DisplayList::PaintFrameForMedia>();
    auto process = gpuConnectionToWebProcess();
    if (!process)
        return false;

    auto playerProxy = process->remoteMediaPlayerManagerProxy().getProxy(mediaItem.identifier());
    if (!playerProxy)
        return false;

    auto player = playerProxy->mediaPlayer();
    if (!player)
        return false;

    context.paintFrameForMedia(*player, mediaItem.destination());
    return true;
}

void RemoteRenderingBackend::imageBufferBackendWasCreated(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace colorSpace, PixelFormat pixelFormat, ImageBufferBackendHandle handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemoteRenderingBackendProxy::ImageBufferBackendWasCreated(logicalSize, backendSize, resolutionScale, colorSpace, pixelFormat, WTFMove(handle), renderingResourceIdentifier), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::didFlush(DisplayList::FlushIdentifier flushIdentifier, RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemoteRenderingBackendProxy::DidFlush(flushIdentifier, renderingResourceIdentifier), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::createImageBuffer(const FloatSize& logicalSize, RenderingMode renderingMode, float resolutionScale, ColorSpace colorSpace, PixelFormat pixelFormat, RenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(renderingMode == RenderingMode::Accelerated || renderingMode == RenderingMode::Unaccelerated);

    RefPtr<ImageBuffer> imageBuffer;

    if (renderingMode == RenderingMode::Accelerated)
        imageBuffer = AcceleratedRemoteImageBuffer::create(logicalSize, resolutionScale, colorSpace, pixelFormat, *this, renderingResourceIdentifier);

    if (!imageBuffer)
        imageBuffer = UnacceleratedRemoteImageBuffer::create(logicalSize, resolutionScale, colorSpace, pixelFormat, *this, renderingResourceIdentifier);

    if (!imageBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_remoteResourceCache.cacheImageBuffer(makeRef(*imageBuffer));
}

void RemoteRenderingBackend::applyDisplayListsFromHandle(ImageBuffer& destination, DisplayListReaderHandle& handle, size_t initialOffset)
{
    auto handleProtector = makeRef(handle);

    size_t offset = initialOffset;
    size_t sizeToRead = 0;

    do {
        sizeToRead = handle.unreadBytes();
    } while (!sizeToRead);

    while (sizeToRead) {
        auto displayList = handle.displayListForReading(offset, sizeToRead, *this);
        if (UNLIKELY(!displayList)) {
            // FIXME: Add a message check to terminate the web process.
            ASSERT_NOT_REACHED();
            return;
        }

        if (destination.isAccelerated())
            static_cast<AcceleratedRemoteImageBuffer&>(destination).submitDisplayList(*displayList);
        else
            static_cast<UnacceleratedRemoteImageBuffer&>(destination).submitDisplayList(*displayList);

        CheckedSize checkedOffset = offset;
        checkedOffset += sizeToRead;
        if (UNLIKELY(checkedOffset.hasOverflowed())) {
            // FIXME: Add a message check to terminate the web process.
            ASSERT_NOT_REACHED();
            return;
        }

        offset = checkedOffset.unsafeGet();

        if (UNLIKELY(offset > handle.sharedMemory().size())) {
            // FIXME: Add a message check to terminate the web process.
            ASSERT_NOT_REACHED();
            return;
        }

        sizeToRead = handle.advance(sizeToRead);
    }
}

void RemoteRenderingBackend::wakeUpAndApplyDisplayList(DisplayList::ItemBufferIdentifier initialIdentifier, uint64_t initialOffset, RenderingResourceIdentifier destinationBufferIdentifier)
{
    TraceScope tracingScope(WakeUpAndApplyDisplayListStart, WakeUpAndApplyDisplayListEnd);
    auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(destinationBufferIdentifier);
    if (UNLIKELY(!imageBuffer)) {
        // FIXME: Add a message check to terminate the web process.
        ASSERT_NOT_REACHED();
        return;
    }

    auto initialHandle = m_sharedDisplayListHandles.get(initialIdentifier);
    if (UNLIKELY(!initialHandle)) {
        // FIXME: Add a message check to terminate the web process.
        ASSERT_NOT_REACHED();
        return;
    }

    applyDisplayListsFromHandle(*imageBuffer, *initialHandle, initialOffset);

    while (m_nextItemBufferToRead) {
        auto nextHandle = m_sharedDisplayListHandles.get(m_nextItemBufferToRead);
        if (!nextHandle) {
            // If the handle identifier is currently unknown, wait until the GPU process receives an
            // IPC message with a shared memory handle to the next item buffer.
            break;
        }
        // Otherwise, continue reading the next display list item buffer from the start.
        m_nextItemBufferToRead = { };
        applyDisplayListsFromHandle(*imageBuffer, *nextHandle, SharedDisplayListHandle::headerSize());
    }
}

void RemoteRenderingBackend::setNextItemBufferToRead(DisplayList::ItemBufferIdentifier identifier)
{
    if (UNLIKELY(m_nextItemBufferToRead)) {
        // FIXME: Add a message check to terminate the web process.
        ASSERT_NOT_REACHED();
        return;
    }
    m_nextItemBufferToRead = identifier;
}

void RemoteRenderingBackend::getImageData(AlphaPremultiplication outputFormat, IntRect srcRect, RenderingResourceIdentifier renderingResourceIdentifier, CompletionHandler<void(IPC::ImageDataReference&&)>&& completionHandler)
{
    RefPtr<ImageData> imageData;
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(renderingResourceIdentifier))
        imageData = imageBuffer->getImageData(outputFormat, srcRect);
    completionHandler(IPC::ImageDataReference(WTFMove(imageData)));
}

void RemoteRenderingBackend::cacheNativeImage(Ref<NativeImage>&& image)
{
    m_remoteResourceCache.cacheNativeImage(WTFMove(image));
}

void RemoteRenderingBackend::releaseRemoteResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    m_remoteResourceCache.releaseRemoteResource(renderingResourceIdentifier);
}

void RemoteRenderingBackend::didCreateSharedDisplayListHandle(DisplayList::ItemBufferIdentifier identifier, const SharedMemory::IPCHandle& handle, RenderingResourceIdentifier destinationBufferIdentifier)
{
    if (UNLIKELY(m_sharedDisplayListHandles.contains(identifier))) {
        // FIXME: Add a message check to terminate the web process.
        ASSERT_NOT_REACHED();
        return;
    }

    if (auto sharedMemory = SharedMemory::map(handle.handle, SharedMemory::Protection::ReadWrite))
        m_sharedDisplayListHandles.set(identifier, DisplayListReaderHandle::create(identifier, sharedMemory.releaseNonNull()));

    if (m_nextItemBufferToRead == identifier) {
        m_nextItemBufferToRead = { };
        wakeUpAndApplyDisplayList(identifier, SharedDisplayListHandle::headerSize(), destinationBufferIdentifier);
    }
}

Optional<DisplayList::ItemHandle> WARN_UNUSED_RETURN RemoteRenderingBackend::decodeItem(const uint8_t* data, size_t length, DisplayList::ItemType type, uint8_t* handleLocation)
{
    switch (type) {
    case DisplayList::ItemType::ClipOutToPath:
        return decodeAndCreate<DisplayList::ClipOutToPath>(data, length, handleLocation);
    case DisplayList::ItemType::ClipPath:
        return decodeAndCreate<DisplayList::ClipPath>(data, length, handleLocation);
    case DisplayList::ItemType::ClipToDrawingCommands:
        return decodeAndCreate<DisplayList::ClipToDrawingCommands>(data, length, handleLocation);
    case DisplayList::ItemType::DrawFocusRingPath:
        return decodeAndCreate<DisplayList::DrawFocusRingPath>(data, length, handleLocation);
    case DisplayList::ItemType::DrawFocusRingRects:
        return decodeAndCreate<DisplayList::DrawFocusRingRects>(data, length, handleLocation);
    case DisplayList::ItemType::DrawGlyphs:
        return decodeAndCreate<DisplayList::DrawGlyphs>(data, length, handleLocation);
    case DisplayList::ItemType::DrawLinesForText:
        return decodeAndCreate<DisplayList::DrawLinesForText>(data, length, handleLocation);
    case DisplayList::ItemType::DrawPath:
        return decodeAndCreate<DisplayList::DrawPath>(data, length, handleLocation);
    case DisplayList::ItemType::FillCompositedRect:
        return decodeAndCreate<DisplayList::FillCompositedRect>(data, length, handleLocation);
    case DisplayList::ItemType::FillPath:
        return decodeAndCreate<DisplayList::FillPath>(data, length, handleLocation);
    case DisplayList::ItemType::FillRectWithColor:
        return decodeAndCreate<DisplayList::FillRectWithColor>(data, length, handleLocation);
    case DisplayList::ItemType::FillRectWithGradient:
        return decodeAndCreate<DisplayList::FillRectWithGradient>(data, length, handleLocation);
    case DisplayList::ItemType::FillRectWithRoundedHole:
        return decodeAndCreate<DisplayList::FillRectWithRoundedHole>(data, length, handleLocation);
    case DisplayList::ItemType::FillRoundedRect:
        return decodeAndCreate<DisplayList::FillRoundedRect>(data, length, handleLocation);
    case DisplayList::ItemType::PutImageData:
        return decodeAndCreate<DisplayList::PutImageData>(data, length, handleLocation);
    case DisplayList::ItemType::SetLineDash:
        return decodeAndCreate<DisplayList::SetLineDash>(data, length, handleLocation);
    case DisplayList::ItemType::SetState:
        return decodeAndCreate<DisplayList::SetState>(data, length, handleLocation);
    case DisplayList::ItemType::StrokePath:
        return decodeAndCreate<DisplayList::StrokePath>(data, length, handleLocation);
    case DisplayList::ItemType::ApplyDeviceScaleFactor:
#if USE(CG)
    case DisplayList::ItemType::ApplyFillPattern:
    case DisplayList::ItemType::ApplyStrokePattern:
#endif
    case DisplayList::ItemType::BeginTransparencyLayer:
    case DisplayList::ItemType::ClearRect:
    case DisplayList::ItemType::ClearShadow:
    case DisplayList::ItemType::Clip:
    case DisplayList::ItemType::ClipOut:
    case DisplayList::ItemType::ClipToImageBuffer:
    case DisplayList::ItemType::ConcatenateCTM:
    case DisplayList::ItemType::DrawDotsForDocumentMarker:
    case DisplayList::ItemType::DrawEllipse:
    case DisplayList::ItemType::DrawImageBuffer:
    case DisplayList::ItemType::DrawNativeImage:
    case DisplayList::ItemType::DrawPattern:
    case DisplayList::ItemType::DrawLine:
    case DisplayList::ItemType::DrawRect:
    case DisplayList::ItemType::EndTransparencyLayer:
    case DisplayList::ItemType::FillEllipse:
#if ENABLE(INLINE_PATH_DATA)
    case DisplayList::ItemType::FillInlinePath:
#endif
    case DisplayList::ItemType::FillRect:
    case DisplayList::ItemType::FlushContext:
    case DisplayList::ItemType::MetaCommandChangeDestinationImageBuffer:
    case DisplayList::ItemType::MetaCommandChangeItemBuffer:
    case DisplayList::ItemType::MetaCommandEnd:
    case DisplayList::ItemType::PaintFrameForMedia:
    case DisplayList::ItemType::Restore:
    case DisplayList::ItemType::Rotate:
    case DisplayList::ItemType::Save:
    case DisplayList::ItemType::Scale:
    case DisplayList::ItemType::SetCTM:
    case DisplayList::ItemType::SetInlineFillColor:
    case DisplayList::ItemType::SetInlineFillGradient:
    case DisplayList::ItemType::SetInlineStrokeColor:
    case DisplayList::ItemType::SetLineCap:
    case DisplayList::ItemType::SetLineJoin:
    case DisplayList::ItemType::SetMiterLimit:
    case DisplayList::ItemType::SetStrokeThickness:
    case DisplayList::ItemType::StrokeEllipse:
#if ENABLE(INLINE_PATH_DATA)
    case DisplayList::ItemType::StrokeInlinePath:
#endif
    case DisplayList::ItemType::StrokeRect:
    case DisplayList::ItemType::StrokeLine:
    case DisplayList::ItemType::Translate: {
        ASSERT_NOT_REACHED();
        break;
    }
    }
    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
