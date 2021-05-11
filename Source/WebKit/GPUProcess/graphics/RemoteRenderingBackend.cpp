/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "Logging.h"
#include "PlatformRemoteImageBuffer.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteRenderingBackendCreationParameters.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>
#include <wtf/WorkQueue.h>

#define TERMINATE_WEB_PROCESS_WITH_MESSAGE(message) \
    RELEASE_LOG_FAULT(IPC, "Requesting termination of web process %" PRIu64 " for reason: %" PUBLIC_LOG_STRING, m_gpuConnectionToWebProcess->webProcessIdentifier().toUInt64(), #message); \
    m_gpuConnectionToWebProcess->terminateWebProcess();

#define MESSAGE_CHECK(assertion, message) do { \
    if (UNLIKELY(!(assertion))) { \
        TERMINATE_WEB_PROCESS_WITH_MESSAGE(message); \
        return; \
    } \
} while (0)

#define MESSAGE_CHECK_WITH_RETURN_VALUE(assertion, returnValue, message) do { \
    if (UNLIKELY(!(assertion))) { \
        TERMINATE_WEB_PROCESS_WITH_MESSAGE(message); \
        return (returnValue); \
    } \
} while (0)

namespace WebKit {
using namespace WebCore;

Ref<RemoteRenderingBackend> RemoteRenderingBackend::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackendCreationParameters&& creationParameters)
{
    auto instance = adoptRef(*new RemoteRenderingBackend(gpuConnectionToWebProcess, WTFMove(creationParameters)));
    instance->startListeningForIPC();
    return instance;
}

RemoteRenderingBackend::RemoteRenderingBackend(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackendCreationParameters&& creationParameters)
    : m_workQueue(WorkQueue::create("RemoteRenderingBackend work queue", WorkQueue::Type::Serial, WorkQueue::QOS::UserInteractive))
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_renderingBackendIdentifier(creationParameters.identifier)
    , m_resumeDisplayListSemaphore(WTFMove(creationParameters.resumeDisplayListSemaphore))
{
    ASSERT(RunLoop::isMain());
}

void RemoteRenderingBackend::startListeningForIPC()
{
    m_gpuConnectionToWebProcess->connection().addWorkQueueMessageReceiver(Messages::RemoteRenderingBackend::messageReceiverName(), m_workQueue, this, m_renderingBackendIdentifier.toUInt64());
}

RemoteRenderingBackend::~RemoteRenderingBackend()
{
    // Make sure we destroy the ResourceCache on the WorkQueue since it gets populated on the WorkQueue.
    // Make sure rendering resource request is released after destroying the cache.
    m_workQueue->dispatch([renderingResourcesRequest = WTFMove(m_renderingResourcesRequest), remoteResourceCache = WTFMove(m_remoteResourceCache)] { });
}

void RemoteRenderingBackend::stopListeningForIPC()
{
    ASSERT(RunLoop::isMain());

    // The RemoteRenderingBackend destructor won't be called until disconnect() is called and we unregister ourselves as a WorkQueueMessageReceiver because
    // the IPC::Connection refs its WorkQueueMessageReceivers.
    m_gpuConnectionToWebProcess->connection().removeWorkQueueMessageReceiver(Messages::RemoteRenderingBackend::messageReceiverName(), m_renderingBackendIdentifier.toUInt64());
}

void RemoteRenderingBackend::dispatch(Function<void()>&& task)
{
    m_workQueue->dispatch(WTFMove(task));
}

IPC::Connection* RemoteRenderingBackend::messageSenderConnection() const
{
    return &m_gpuConnectionToWebProcess->connection();
}

uint64_t RemoteRenderingBackend::messageSenderDestinationID() const
{
    return m_renderingBackendIdentifier.toUInt64();
}

bool RemoteRenderingBackend::applyMediaItem(DisplayList::ItemHandle item, GraphicsContext& context)
{
    ASSERT(!RunLoop::isMain());

    if (!item.is<DisplayList::PaintFrameForMedia>())
        return false;

    auto& mediaItem = item.get<DisplayList::PaintFrameForMedia>();
    auto player = m_gpuConnectionToWebProcess->remoteMediaPlayerManagerProxy().mediaPlayer(mediaItem.identifier());
    if (!player)
        return true;

    context.paintFrameForMedia(*player, mediaItem.destination());
    return true;
}

void RemoteRenderingBackend::didCreateImageBufferBackend(ImageBufferBackendHandle handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemoteRenderingBackendProxy::DidCreateImageBufferBackend(WTFMove(handle), renderingResourceIdentifier), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::didFlush(DisplayList::FlushIdentifier flushIdentifier, RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemoteRenderingBackendProxy::DidFlush(flushIdentifier, renderingResourceIdentifier), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::createImageBuffer(const FloatSize& logicalSize, RenderingMode renderingMode, float resolutionScale, DestinationColorSpace colorSpace, PixelFormat pixelFormat, RenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(renderingMode == RenderingMode::Accelerated || renderingMode == RenderingMode::Unaccelerated);

    RefPtr<ImageBuffer> imageBuffer;

    if (renderingMode == RenderingMode::Accelerated) {
        if (auto acceleratedImageBuffer = AcceleratedRemoteImageBuffer::create(logicalSize, resolutionScale, colorSpace, pixelFormat, *this, renderingResourceIdentifier)) {
#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY)
            // Mark the IOSurface as being owned by the WebProcess even though it was constructed by the GPUProcess so that Jetsam knows which process to kill.
            acceleratedImageBuffer->setProcessOwnership(m_gpuConnectionToWebProcess->webProcessIdentityToken());
#endif
            imageBuffer = WTFMove(acceleratedImageBuffer);
        }
    }

    if (!imageBuffer)
        imageBuffer = UnacceleratedRemoteImageBuffer::create(logicalSize, resolutionScale, colorSpace, pixelFormat, *this, renderingResourceIdentifier);

    if (!imageBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_remoteResourceCache.cacheImageBuffer(makeRef(*imageBuffer));
    updateRenderingResourceRequest();

    if (m_pendingWakeupInfo && m_pendingWakeupInfo->shouldPerformWakeup(renderingResourceIdentifier))
        wakeUpAndApplyDisplayList(std::exchange(m_pendingWakeupInfo, WTF::nullopt)->arguments);
}

DisplayList::ReplayResult RemoteRenderingBackend::submit(const DisplayList::DisplayList& displayList, ImageBuffer& destination)
{
    if (displayList.isEmpty())
        return { };

    DisplayList::Replayer::Delegate* replayerDelegate = nullptr;
    if (destination.renderingMode() == RenderingMode::Accelerated)
        replayerDelegate = static_cast<AcceleratedRemoteImageBuffer*>(&destination);
    else
        replayerDelegate = static_cast<UnacceleratedRemoteImageBuffer*>(&destination);

    return WebCore::DisplayList::Replayer {
        destination.context(),
        displayList,
        &remoteResourceCache().imageBuffers(),
        &remoteResourceCache().nativeImages(),
        &remoteResourceCache().fonts(),
        replayerDelegate
    }.replay();
}

RefPtr<ImageBuffer> RemoteRenderingBackend::nextDestinationImageBufferAfterApplyingDisplayLists(ImageBuffer& initialDestination, size_t initialOffset, DisplayListReaderHandle& handle, GPUProcessWakeupReason reason)
{
    auto destination = makeRefPtr(initialDestination);
    auto handleProtector = makeRef(handle);

    auto offset = initialOffset;
    size_t sizeToRead = 0;
    do {
        sizeToRead = handle.unreadBytes();
    } while (!sizeToRead);

    while (destination) {
        auto displayList = handle.displayListForReading(offset, sizeToRead, *this);
        MESSAGE_CHECK_WITH_RETURN_VALUE(displayList, nullptr, "Failed to map display list from shared memory");

        auto result = submit(*displayList, *destination);
        MESSAGE_CHECK_WITH_RETURN_VALUE(result.reasonForStopping != DisplayList::StopReplayReason::InvalidItemOrExtent, nullptr, "Detected invalid display list item or extent");
        MESSAGE_CHECK_WITH_RETURN_VALUE(result.reasonForStopping != DisplayList::StopReplayReason::OutOfMemory, nullptr, "Cound not allocate memory");

        auto advanceResult = handle.advance(result.numberOfBytesRead);
        MESSAGE_CHECK_WITH_RETURN_VALUE(advanceResult, nullptr, "Failed to advance display list reader handle");
        sizeToRead = *advanceResult;

        CheckedSize checkedOffset = offset;
        checkedOffset += result.numberOfBytesRead;
        MESSAGE_CHECK_WITH_RETURN_VALUE(!checkedOffset.hasOverflowed(), nullptr, "Overflowed when advancing shared display list handle offset");

        offset = checkedOffset.unsafeGet();
        MESSAGE_CHECK_WITH_RETURN_VALUE(offset <= handle.sharedMemory().size(), nullptr, "Out-of-bounds offset into shared display list handle");

        if (result.reasonForStopping == DisplayList::StopReplayReason::ChangeDestinationImageBuffer) {
            destination = makeRefPtr(m_remoteResourceCache.cachedImageBuffer(*result.nextDestinationImageBuffer));
            if (!destination) {
                ASSERT(!m_pendingWakeupInfo);
                m_pendingWakeupInfo = {{{ handle.identifier(), offset, *result.nextDestinationImageBuffer, reason }, WTF::nullopt }};
            }
        }

        if (result.reasonForStopping == DisplayList::StopReplayReason::MissingCachedResource) {
            m_pendingWakeupInfo = {{
                { handle.identifier(), offset, destination->renderingResourceIdentifier(), reason },
                result.missingCachedResourceIdentifier
            }};
        }

        if (m_pendingWakeupInfo)
            break;

        if (!sizeToRead) {
            if (reason != GPUProcessWakeupReason::ItemCountHysteresisExceeded)
                break;

            handle.startWaiting();
            m_resumeDisplayListSemaphore.waitFor(30_us);

            auto resumeReadingInfo = handle.stopWaiting();
            if (!resumeReadingInfo)
                break;

            sizeToRead = handle.unreadBytes();
            MESSAGE_CHECK_WITH_RETURN_VALUE(sizeToRead, nullptr, "No unread bytes when resuming display list processing");

            auto newDestinationIdentifier = makeObjectIdentifier<RenderingResourceIdentifierType>(resumeReadingInfo->destination);
            MESSAGE_CHECK_WITH_RETURN_VALUE(newDestinationIdentifier, nullptr, "Invalid image buffer destination when resuming display list processing");

            destination = makeRefPtr(m_remoteResourceCache.cachedImageBuffer(newDestinationIdentifier));
            MESSAGE_CHECK_WITH_RETURN_VALUE(destination, nullptr, "Missing image buffer destination when resuming display list processing");

            offset = resumeReadingInfo->offset;

            if (!destination) {
                ASSERT(!m_pendingWakeupInfo);
                m_pendingWakeupInfo = {{{ handle.identifier(), offset, newDestinationIdentifier, reason }, WTF::nullopt }};
                break;
            }
        }
    }

    return destination;
}

void RemoteRenderingBackend::wakeUpAndApplyDisplayList(const GPUProcessWakeupMessageArguments& arguments)
{
    ASSERT(!RunLoop::isMain());

    TraceScope tracingScope(WakeUpAndApplyDisplayListStart, WakeUpAndApplyDisplayListEnd);
    auto destinationImageBuffer = makeRefPtr(m_remoteResourceCache.cachedImageBuffer(arguments.destinationImageBufferIdentifier));
    MESSAGE_CHECK(destinationImageBuffer, "Missing destination image buffer");

    auto initialHandle = m_sharedDisplayListHandles.get(arguments.itemBufferIdentifier);
    MESSAGE_CHECK(initialHandle, "Missing initial shared display list handle");

    destinationImageBuffer = nextDestinationImageBufferAfterApplyingDisplayLists(*destinationImageBuffer, arguments.offset, *initialHandle, arguments.reason);
    if (!destinationImageBuffer)
        return;

    while (m_pendingWakeupInfo) {
        if (m_pendingWakeupInfo->missingCachedResourceIdentifier)
            break;

        auto nextHandle = m_sharedDisplayListHandles.get(m_pendingWakeupInfo->arguments.itemBufferIdentifier);
        if (!nextHandle) {
            // If the handle identifier is currently unknown, wait until the GPU process receives an
            // IPC message with a shared memory handle to the next item buffer.
            break;
        }

        // Otherwise, continue reading the next display list item buffer from the start.
        auto arguments = std::exchange(m_pendingWakeupInfo, WTF::nullopt)->arguments;
        destinationImageBuffer = nextDestinationImageBufferAfterApplyingDisplayLists(*destinationImageBuffer, arguments.offset, *nextHandle, arguments.reason);
        if (!destinationImageBuffer)
            break;
    }
}

void RemoteRenderingBackend::setNextItemBufferToRead(DisplayList::ItemBufferIdentifier identifier, WebCore::RenderingResourceIdentifier destinationIdentifier)
{
    m_pendingWakeupInfo = {{{ identifier, SharedDisplayListHandle::headerSize(), destinationIdentifier, GPUProcessWakeupReason::Unspecified }, WTF::nullopt }};
}

Optional<SharedMemory::IPCHandle> RemoteRenderingBackend::updateSharedMemoryForGetPixelBufferHelper(size_t byteCount)
{
    MESSAGE_CHECK_WITH_RETURN_VALUE(!m_getPixelBufferSharedMemory || byteCount > m_getPixelBufferSharedMemory->size(), WTF::nullopt, "The existing Shmem for getPixelBuffer() is already big enough to handle the request");

    if (byteCount > 64 * MB) {
        // Just a sanity check. A 4K image is 36MB.
        return WTF::nullopt;
    }

    destroyGetPixelBufferSharedMemory();
    m_getPixelBufferSharedMemory = SharedMemory::allocate(byteCount);
    SharedMemory::Handle handle;
    if (m_getPixelBufferSharedMemory)
        m_getPixelBufferSharedMemory->createHandle(handle, SharedMemory::Protection::ReadOnly);
    return SharedMemory::IPCHandle { WTFMove(handle), m_getPixelBufferSharedMemory ? m_getPixelBufferSharedMemory->size() : 0 };
}

void RemoteRenderingBackend::updateSharedMemoryForGetPixelBuffer(uint32_t byteCount, CompletionHandler<void(const SharedMemory::IPCHandle&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto handle = updateSharedMemoryForGetPixelBufferHelper(byteCount))
        completionHandler(WTFMove(handle.value()));
    else
        completionHandler({ });
}

void RemoteRenderingBackend::semaphoreForGetPixelBuffer(CompletionHandler<void(const IPC::Semaphore&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    completionHandler(m_getPixelBufferSemaphore);
}

void RemoteRenderingBackend::updateSharedMemoryAndSemaphoreForGetPixelBuffer(uint32_t byteCount, CompletionHandler<void(const SharedMemory::IPCHandle&, const IPC::Semaphore&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto handle = updateSharedMemoryForGetPixelBufferHelper(byteCount))
        completionHandler(WTFMove(handle.value()), m_getPixelBufferSemaphore);
    else
        completionHandler({ }, m_getPixelBufferSemaphore);
}

void RemoteRenderingBackend::destroyGetPixelBufferSharedMemory()
{
    m_getPixelBufferSharedMemory = nullptr;
}

void RemoteRenderingBackend::populateGetPixelBufferSharedMemory(Optional<WebCore::PixelBuffer>&& pixelBuffer)
{
    MESSAGE_CHECK(m_getPixelBufferSharedMemory, "We can't run getPixelBuffer without a buffer to write into");

    if (pixelBuffer) {
        MESSAGE_CHECK(pixelBuffer->data().byteLength() <= m_getPixelBufferSharedMemory->size(), "Shmem for return of getPixelBuffer is too small");
        memcpy(m_getPixelBufferSharedMemory->data(), pixelBuffer->data().data(), pixelBuffer->data().byteLength());
    } else
        memset(m_getPixelBufferSharedMemory->data(), 0, m_getPixelBufferSharedMemory->size());

    m_getPixelBufferSemaphore.signal();
}

void RemoteRenderingBackend::getDataURLForImageBuffer(const String& mimeType, Optional<double> quality, WebCore::PreserveResolution preserveResolution, WebCore::RenderingResourceIdentifier renderingResourceIdentifier, CompletionHandler<void(String&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    String urlString;
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(renderingResourceIdentifier))
        urlString = imageBuffer->toDataURL(mimeType, quality, preserveResolution);
    completionHandler(WTFMove(urlString));
}

void RemoteRenderingBackend::getDataForImageBuffer(const String& mimeType, Optional<double> quality, WebCore::RenderingResourceIdentifier renderingResourceIdentifier, CompletionHandler<void(Vector<uint8_t>&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    Vector<uint8_t> data;
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(renderingResourceIdentifier))
        data = imageBuffer->toData(mimeType, quality);
    completionHandler(WTFMove(data));
}

void RemoteRenderingBackend::getBGRADataForImageBuffer(WebCore::RenderingResourceIdentifier renderingResourceIdentifier, CompletionHandler<void(Vector<uint8_t>&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    Vector<uint8_t> data;
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(renderingResourceIdentifier))
        data = imageBuffer->toBGRAData();
    completionHandler(WTFMove(data));
}

void RemoteRenderingBackend::getShareableBitmapForImageBuffer(WebCore::RenderingResourceIdentifier identifier, WebCore::PreserveResolution preserveResolution, CompletionHandler<void(ShareableBitmap::Handle&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    ShareableBitmap::Handle handle;
    [&]() {
        auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(identifier);
        if (!imageBuffer)
            return;
        auto image = imageBuffer->copyNativeImage(WebCore::BackingStoreCopy::DontCopyBackingStore);
        if (!image)
            return;
        auto backendSize = imageBuffer->backendSize();
        auto resultSize = preserveResolution == WebCore::PreserveResolution::Yes ? backendSize : imageBuffer->logicalSize();
        auto bitmap = ShareableBitmap::createShareable(resultSize, { });
        if (!bitmap)
            return;
        auto context = bitmap->createGraphicsContext();
        if (!context)
            return;
        context->drawNativeImage(*image, resultSize, FloatRect { { }, resultSize }, FloatRect { { }, backendSize }, { WebCore::CompositeOperator::Copy });
        bitmap->createHandle(handle);
    }();
    completionHandler(WTFMove(handle));
}

void RemoteRenderingBackend::cacheNativeImage(const ShareableBitmap::Handle& handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());

    auto bitmap = ShareableBitmap::create(handle);
    if (!bitmap)
        return;

    auto image = NativeImage::create(bitmap->createPlatformImage(), renderingResourceIdentifier);
    if (!image)
        return;

    m_remoteResourceCache.cacheNativeImage(makeRef(*image));

    if (m_pendingWakeupInfo && m_pendingWakeupInfo->shouldPerformWakeup(renderingResourceIdentifier))
        wakeUpAndApplyDisplayList(std::exchange(m_pendingWakeupInfo, WTF::nullopt)->arguments);
}

void RemoteRenderingBackend::cacheFont(Ref<Font>&& font)
{
    ASSERT(!RunLoop::isMain());

    auto identifier = font->renderingResourceIdentifier();
    m_remoteResourceCache.cacheFont(WTFMove(font));
    if (m_pendingWakeupInfo && m_pendingWakeupInfo->shouldPerformWakeup(identifier))
        wakeUpAndApplyDisplayList(std::exchange(m_pendingWakeupInfo, WTF::nullopt)->arguments);
}

void RemoteRenderingBackend::deleteAllFonts()
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.deleteAllFonts();
}

void RemoteRenderingBackend::releaseRemoteResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.releaseRemoteResource(renderingResourceIdentifier);
    updateRenderingResourceRequest();
}

void RemoteRenderingBackend::didCreateSharedDisplayListHandle(DisplayList::ItemBufferIdentifier identifier, const SharedMemory::IPCHandle& handle, RenderingResourceIdentifier destinationBufferIdentifier)
{
    ASSERT(!RunLoop::isMain());
    MESSAGE_CHECK(!m_sharedDisplayListHandles.contains(identifier), "Duplicate shared display list handle");

    if (auto sharedMemory = SharedMemory::map(handle.handle, SharedMemory::Protection::ReadWrite)) {
        auto handle = DisplayListReaderHandle::create(identifier, sharedMemory.releaseNonNull());
        MESSAGE_CHECK(handle, "There must be enough space to create the handle.");
        m_sharedDisplayListHandles.set(identifier, handle);
    }

    if (m_pendingWakeupInfo && m_pendingWakeupInfo->shouldPerformWakeup(identifier))
        wakeUpAndApplyDisplayList(std::exchange(m_pendingWakeupInfo, WTF::nullopt)->arguments);
}

Optional<DisplayList::ItemHandle> WARN_UNUSED_RETURN RemoteRenderingBackend::decodeItem(const uint8_t* data, size_t length, DisplayList::ItemType type, uint8_t* handleLocation)
{
    /* This needs to match (1) isInlineItem() in DisplayListItemType.cpp, (2) RemoteImageBufferProxy::encodeItem(),
     * and (3) all the "static constexpr bool isInlineItem"s inside the individual item classes.
     * See the comment at the top of DisplayListItems.h for why. */

    switch (type) {
    case DisplayList::ItemType::ClipOutToPath:
        return decodeAndCreate<DisplayList::ClipOutToPath>(data, length, handleLocation);
    case DisplayList::ItemType::ClipPath:
        return decodeAndCreate<DisplayList::ClipPath>(data, length, handleLocation);
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
    case DisplayList::ItemType::PutPixelBuffer:
        return decodeAndCreate<DisplayList::PutPixelBuffer>(data, length, handleLocation);
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
    case DisplayList::ItemType::BeginClipToDrawingCommands:
    case DisplayList::ItemType::EndClipToDrawingCommands:
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
#if ENABLE(VIDEO)
    case DisplayList::ItemType::PaintFrameForMedia:
#endif
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
    case DisplayList::ItemType::Translate:
    case DisplayList::ItemType::GetPixelBuffer:
        ASSERT_NOT_REACHED();
        return WTF::nullopt;
    }
    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

void RemoteRenderingBackend::updateRenderingResourceRequest()
{
    bool hasActiveDrawables = !m_remoteResourceCache.imageBuffers().isEmpty() || !m_remoteResourceCache.nativeImages().isEmpty();
    bool hasActiveRequest = m_renderingResourcesRequest.isRequested();
    if (hasActiveDrawables && !hasActiveRequest)
        m_renderingResourcesRequest = ScopedRenderingResourcesRequest::acquire();
    else if (!hasActiveDrawables && hasActiveRequest)
        m_renderingResourcesRequest = { };
}

bool RemoteRenderingBackend::allowsExitUnderMemoryPressure() const
{
    return m_remoteResourceCache.imageBuffers().isEmpty() && m_remoteResourceCache.nativeImages().isEmpty();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
