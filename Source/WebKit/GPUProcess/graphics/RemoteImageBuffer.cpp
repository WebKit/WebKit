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

#include "GPUConnectionToWebProcess.h"
#include "IPCSemaphore.h"
#include "Logging.h"
#include "RemoteImageBufferMessages.h"
#include "RemoteRenderingBackend.h"
#include "RemoteSharedResourceCache.h"
#include "StreamConnectionWorkQueue.h"
#include <WebCore/GraphicsContext.h>
#include <wtf/StdLibExtras.h>

#define MESSAGE_CHECK(assertion, message) MESSAGE_CHECK_WITH_MESSAGE_BASE(assertion, &m_backend->gpuConnectionToWebProcess().connection(), message)

namespace WebKit {

Ref<RemoteImageBuffer> RemoteImageBuffer::create(Ref<WebCore::ImageBuffer> imageBuffer, RemoteRenderingBackend& backend)
{
    auto instance = adoptRef(*new RemoteImageBuffer(WTFMove(imageBuffer), backend));
    instance->startListeningForIPC();
    return instance;
}

RemoteImageBuffer::RemoteImageBuffer(Ref<WebCore::ImageBuffer> imageBuffer, RemoteRenderingBackend& backend)
    : m_backend(&backend)
    , m_imageBuffer(WTFMove(imageBuffer))
{
    m_backend->protectedSharedResourceCache()->didCreateImageBuffer(m_imageBuffer->renderingPurpose(), m_imageBuffer->renderingMode());
}

RemoteImageBuffer::~RemoteImageBuffer()
{
    if (m_backend)
        m_backend->protectedSharedResourceCache()->didReleaseImageBuffer(m_imageBuffer->renderingPurpose(), m_imageBuffer->renderingMode());
    // Volatile image buffers do not have contexts.
    Ref imageBuffer = m_imageBuffer;
    if (imageBuffer->volatilityState() == WebCore::VolatilityState::Volatile)
        return;
    if (!imageBuffer->hasBackend())
        return;
    // Unwind the context's state stack before destruction, since calls to restore may not have
    // been flushed yet, or the web process may have terminated.
    auto& context = imageBuffer->context();
    while (context.stackSize())
        context.restore();
}

void RemoteImageBuffer::startListeningForIPC()
{
    m_backend->protectedStreamConnection()->startReceivingMessages(*this, Messages::RemoteImageBuffer::messageReceiverName(), identifier().toUInt64());
}

void RemoteImageBuffer::stopListeningForIPC()
{
    if (auto backend = std::exchange(m_backend, { })) {
        backend->protectedSharedResourceCache()->didReleaseImageBuffer(m_imageBuffer->renderingPurpose(), m_imageBuffer->renderingMode());
        backend->protectedStreamConnection()->stopReceivingMessages(Messages::RemoteImageBuffer::messageReceiverName(), identifier().toUInt64());
    }
}

void RemoteImageBuffer::getPixelBuffer(WebCore::PixelBufferFormat destinationFormat, WebCore::IntPoint srcPoint, WebCore::IntSize srcSize, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    auto memory = m_backend->sharedMemoryForGetPixelBuffer();
    MESSAGE_CHECK(memory, "No shared memory for getPixelBufferForImageBuffer");
    MESSAGE_CHECK(WebCore::PixelBuffer::supportedPixelFormat(destinationFormat.pixelFormat), "Pixel format not supported");
    WebCore::IntRect srcRect(srcPoint, srcSize);
    if (auto pixelBuffer = imageBuffer()->getPixelBuffer(destinationFormat, srcRect)) {
        MESSAGE_CHECK(pixelBuffer->bytes().size() <= memory->size(), "Shmem for return of getPixelBuffer is too small");
        memcpySpan(memory->mutableSpan(), pixelBuffer->bytes());
    } else
        zeroSpan(memory->mutableSpan());
    completionHandler();
}

void RemoteImageBuffer::getPixelBufferWithNewMemory(WebCore::SharedMemory::Handle&& handle, WebCore::PixelBufferFormat destinationFormat, WebCore::IntPoint srcPoint, WebCore::IntSize srcSize, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    m_backend->setSharedMemoryForGetPixelBuffer(nullptr);
    auto sharedMemory = WebCore::SharedMemory::map(WTFMove(handle), WebCore::SharedMemory::Protection::ReadWrite);
    MESSAGE_CHECK(sharedMemory, "Shared memory could not be mapped.");
    m_backend->setSharedMemoryForGetPixelBuffer(WTFMove(sharedMemory));
    getPixelBuffer(WTFMove(destinationFormat), WTFMove(srcPoint), WTFMove(srcSize), WTFMove(completionHandler));
}

void RemoteImageBuffer::putPixelBuffer(Ref<WebCore::PixelBuffer> pixelBuffer, WebCore::IntPoint srcPoint, WebCore::IntSize srcSize, WebCore::IntPoint destPoint, WebCore::AlphaPremultiplication destFormat)
{
    assertIsCurrent(workQueue());
    WebCore::IntRect srcRect(srcPoint, srcSize);
    imageBuffer()->putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat);
}

void RemoteImageBuffer::getShareableBitmap(WebCore::PreserveResolution preserveResolution, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    std::optional<WebCore::ShareableBitmap::Handle> handle = [&]() -> std::optional<WebCore::ShareableBitmap::Handle> {
        Ref<WebCore::ImageBuffer> imageBuffer = m_imageBuffer;
        auto backendSize = imageBuffer->backendSize();
        auto logicalSize = imageBuffer->logicalSize();
        auto resultSize = preserveResolution == WebCore::PreserveResolution::Yes ? backendSize : imageBuffer->truncatedLogicalSize();
        if (resultSize.isEmpty())
            return std::nullopt;
        auto bitmap = WebCore::ShareableBitmap::create({ resultSize, imageBuffer->colorSpace() });
        if (!bitmap)
            return std::nullopt;
        auto handle = bitmap->createHandle();
        RefPtr backend = m_backend;
        if (backend->sharedResourceCache().resourceOwner())
            handle->setOwnershipOfMemory(backend->sharedResourceCache().resourceOwner(), WebCore::MemoryLedger::Graphics);
        auto context = bitmap->createGraphicsContext();
        if (!context)
            return std::nullopt;
        context->drawImageBuffer(imageBuffer.get(), WebCore::FloatRect { { }, resultSize }, WebCore::FloatRect { { }, logicalSize }, { WebCore::CompositeOperator::Copy });
        return handle;
    }();
    completionHandler(WTFMove(handle));
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
        if (m_backend->sharedResourceCache().resourceOwner())
            handle->setOwnershipOfMemory(m_backend->sharedResourceCache().resourceOwner(), WebCore::MemoryLedger::Graphics);
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
    imageBuffer()->convertToLuminanceMask();
}

void RemoteImageBuffer::transformToColorSpace(const WebCore::DestinationColorSpace& colorSpace)
{
    assertIsCurrent(workQueue());
    imageBuffer()->transformToColorSpace(colorSpace);
}

void RemoteImageBuffer::flushContext()
{
    assertIsCurrent(workQueue());
    imageBuffer()->flushDrawingContext();
}

void RemoteImageBuffer::flushContextSync(CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    imageBuffer()->flushDrawingContext();
    completionHandler();
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
void RemoteImageBuffer::dynamicContentScalingDisplayList(CompletionHandler<void(std::optional<WebCore::DynamicContentScalingDisplayList>&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    auto displayList = imageBuffer()->dynamicContentScalingDisplayList();
    completionHandler({ WTFMove(displayList) });
}
#endif

IPC::StreamConnectionWorkQueue& RemoteImageBuffer::workQueue() const
{
    return m_backend->workQueue();
}

#undef MESSAGE_CHECK

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
