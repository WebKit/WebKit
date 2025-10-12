/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "RemoteImageBuffer.h"

#if ENABLE(GPU_PROCESS)

#include "IPCSemaphore.h"
#include "ImageBufferBackendHandleSharing.h"
#include "Logging.h"
#include "RemoteImageBufferGraphicsContext.h"
#include "RemoteImageBufferMessages.h"
#include "RemoteImageBufferProxyMessages.h"
#include "RemoteRenderingBackend.h"
#include "RemoteSharedResourceCache.h"
#include "StreamConnectionWorkQueue.h"
#include <WebCore/GraphicsContext.h>
#include <wtf/StdLibExtras.h>

#define MESSAGE_CHECK(assertion, message) MESSAGE_CHECK_WITH_MESSAGE_BASE(assertion, m_renderingBackend->streamConnection(), message)

namespace WebKit {

Ref<RemoteImageBuffer> RemoteImageBuffer::create(Ref<WebCore::ImageBuffer>&& imageBuffer, WebCore::RenderingResourceIdentifier identifier, RemoteGraphicsContextIdentifier contextIdentifier, RemoteRenderingBackend& renderingBackend)
{
    auto instance = adoptRef(*new RemoteImageBuffer(WTFMove(imageBuffer), identifier, contextIdentifier, renderingBackend));
    instance->startListeningForIPC();
    return instance;
}

RemoteImageBuffer::RemoteImageBuffer(Ref<WebCore::ImageBuffer>&& imageBuffer, WebCore::RenderingResourceIdentifier identifier, RemoteGraphicsContextIdentifier contextIdentifier, RemoteRenderingBackend& renderingBackend)
    : m_imageBuffer(WTFMove(imageBuffer))
    , m_identifier(identifier)
    , m_renderingBackend(renderingBackend)
    , m_context(RemoteImageBufferGraphicsContext::create(m_imageBuffer, contextIdentifier, m_renderingBackend))
{
    m_renderingBackend->sharedResourceCache().didCreateImageBuffer(m_imageBuffer->renderingPurpose(), m_imageBuffer->renderingMode());

    // If the ImageBuffer was an error buffer, this will fail and nullopt will be sent, signaling
    // allocation failure.
    auto* sharing = m_imageBuffer->toBackendSharing();
    auto handle = sharing ? downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle() : std::nullopt;
    m_renderingBackend->streamConnection().send(Messages::RemoteImageBufferProxy::DidCreateBackend(WTFMove(handle)), m_identifier);
}

RemoteImageBuffer::~RemoteImageBuffer()
{
    m_renderingBackend->sharedResourceCache().didReleaseImageBuffer(m_imageBuffer->renderingPurpose(), m_imageBuffer->renderingMode());
    // Volatile image buffers do not have contexts.
    if (m_imageBuffer->volatilityState() == WebCore::VolatilityState::Volatile)
        return;
    if (!m_imageBuffer->hasBackend())
        return;
    // Unwind the context's state stack before destruction, since calls to restore may not have
    // been flushed yet, or the web process may have terminated.
    auto& context = m_imageBuffer->context();
    while (context.stackSize())
        context.restore();
}

void RemoteImageBuffer::startListeningForIPC()
{
    m_renderingBackend->streamConnection().startReceivingMessages(*this, Messages::RemoteImageBuffer::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteImageBuffer::stopListeningForIPC()
{
    m_context.reset();
    m_renderingBackend->streamConnection().stopReceivingMessages(Messages::RemoteImageBuffer::messageReceiverName(), m_identifier.toUInt64());
}

Ref<WebCore::ImageBuffer> RemoteImageBuffer::sinkIntoImageBuffer(Ref<RemoteImageBuffer>&& remote)
{
    Ref localRemote = WTFMove(remote);
    RELEASE_ASSERT(localRemote->hasOneRef());
    Ref imageBuffer = localRemote->m_imageBuffer;
    return imageBuffer;
}

void RemoteImageBuffer::getPixelBuffer(WebCore::PixelBufferFormat destinationFormat, WebCore::IntPoint srcPoint, WebCore::IntSize srcSize, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    auto memory = m_renderingBackend->sharedMemoryForGetPixelBuffer();
    MESSAGE_CHECK(memory, "No shared memory for getPixelBufferForImageBuffer");
    MESSAGE_CHECK(WebCore::PixelBuffer::supportedPixelFormat(destinationFormat.pixelFormat), "Pixel format not supported");
    WebCore::IntRect srcRect(srcPoint, srcSize);
    if (auto pixelBuffer = m_imageBuffer->getPixelBuffer(destinationFormat, srcRect)) {
        MESSAGE_CHECK(pixelBuffer->bytes().size() <= memory->size(), "Shmem for return of getPixelBuffer is too small");
        memcpySpan(memory->mutableSpan(), pixelBuffer->bytes());
    } else
        zeroSpan(memory->mutableSpan());
    completionHandler();
}

void RemoteImageBuffer::getPixelBufferWithNewMemory(WebCore::SharedMemory::Handle&& handle, WebCore::PixelBufferFormat destinationFormat, WebCore::IntPoint srcPoint, WebCore::IntSize srcSize, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    m_renderingBackend->setSharedMemoryForGetPixelBuffer(nullptr);
    auto sharedMemory = WebCore::SharedMemory::map(WTFMove(handle), WebCore::SharedMemory::Protection::ReadWrite);
    MESSAGE_CHECK(sharedMemory, "Shared memory could not be mapped.");
    m_renderingBackend->setSharedMemoryForGetPixelBuffer(WTFMove(sharedMemory));
    getPixelBuffer(WTFMove(destinationFormat), WTFMove(srcPoint), WTFMove(srcSize), WTFMove(completionHandler));
}

void RemoteImageBuffer::putPixelBuffer(const WebCore::PixelBufferSourceView& pixelBuffer, WebCore::IntPoint srcPoint, WebCore::IntSize srcSize, WebCore::IntPoint destPoint, WebCore::AlphaPremultiplication destFormat)
{
    assertIsCurrent(workQueue());
    WebCore::IntRect srcRect(srcPoint, srcSize);
    m_imageBuffer->putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat);
}

void RemoteImageBuffer::copyNativeImage(RenderingResourceIdentifier imageIdentifier)
{
    assertIsCurrent(workQueue());
    RefPtr image = m_imageBuffer->copyNativeImage();
    // FIXME: Handle OOM.
    MESSAGE_CHECK(image, "OOM");
    bool success = m_renderingBackend->remoteResourceCache().cacheNativeImage(imageIdentifier, image.releaseNonNull());
    MESSAGE_CHECK(success, "NativeImage already exists");
}

void RemoteImageBuffer::filteredNativeImage(Ref<WebCore::Filter> filter, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    std::optional<WebCore::ShareableBitmap::Handle> handle = [&]() -> std::optional<WebCore::ShareableBitmap::Handle> {
        Ref imageBuffer = m_imageBuffer;
        auto image = imageBuffer->filteredNativeImage(filter);
        if (!image)
            return std::nullopt;
        auto imageSize = image->size();
        auto bitmap = WebCore::ShareableBitmap::create({ imageSize, imageBuffer->colorSpace() });
        if (!bitmap)
            return std::nullopt;
        auto handle = bitmap->createHandle();
        if (m_renderingBackend->sharedResourceCache().resourceOwner())
            handle->setOwnershipOfMemory(m_renderingBackend->sharedResourceCache().resourceOwner(), WebCore::MemoryLedger::Graphics);
        auto context = bitmap->createGraphicsContext();
        if (!context)
            return std::nullopt;
        context->drawNativeImage(*image, WebCore::FloatRect { { }, imageSize }, WebCore::FloatRect { { }, imageSize });
        return handle;
    }();
    completionHandler(WTFMove(handle));
}

void RemoteImageBuffer::convertToLuminanceMask()
{
    assertIsCurrent(workQueue());
    m_imageBuffer->convertToLuminanceMask();
}

void RemoteImageBuffer::transformToColorSpace(const WebCore::DestinationColorSpace& colorSpace)
{
    assertIsCurrent(workQueue());
    m_imageBuffer->transformToColorSpace(colorSpace);
}

void RemoteImageBuffer::setFlushSignal(IPC::Signal&& signal)
{
    m_flushSignal = WTFMove(signal);
}

void RemoteImageBuffer::flushContext()
{
    RELEASE_ASSERT(m_flushSignal);
    assertIsCurrent(workQueue());
    m_imageBuffer->flushDrawingContext();
    m_flushSignal->signal();
}

void RemoteImageBuffer::flushContextSync(CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    m_imageBuffer->flushDrawingContext();
    completionHandler();
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
void RemoteImageBuffer::dynamicContentScalingDisplayList(CompletionHandler<void(std::optional<WebCore::DynamicContentScalingDisplayList>&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    auto displayList = m_imageBuffer->dynamicContentScalingDisplayList();
    completionHandler({ WTFMove(displayList) });
}
#endif

IPC::StreamConnectionWorkQueue& RemoteImageBuffer::workQueue() const
{
    return m_renderingBackend->workQueue();
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
