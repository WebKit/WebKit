/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#include "BufferIdentifierSet.h"
#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "PlatformImageBufferShareableBackend.h"
#include "QualifiedRenderingResourceIdentifier.h"
#include "RemoteBarcodeDetector.h"
#include "RemoteBarcodeDetectorMessages.h"
#include "RemoteDisplayListRecorder.h"
#include "RemoteDisplayListRecorderMessages.h"
#include "RemoteFaceDetector.h"
#include "RemoteFaceDetectorMessages.h"
#include "RemoteImageBuffer.h"
#include "RemoteImageBufferProxyMessages.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteRenderingBackendCreationParameters.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "RemoteTextDetector.h"
#include "RemoteTextDetectorMessages.h"
#include "ShapeDetectionObjectHeap.h"
#include "SwapBuffersDisplayRequirement.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/HTMLCanvasElement.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/RunLoop.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>

#if HAVE(IOSURFACE)
#include <WebCore/IOSurfacePool.h>
#endif

#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
#import <WebCore/BarcodeDetectorImplementation.h>
#import <WebCore/FaceDetectorImplementation.h>
#import <WebCore/TextDetectorImplementation.h>
#endif

#if PLATFORM(COCOA)
#include "ImageBufferShareableMappedIOSurfaceBitmapBackend.h"
#endif

#if ENABLE(IPC_TESTING_API)
#define WEB_PROCESS_TERMINATE_CONDITION !m_gpuConnectionToWebProcess->connection().ignoreInvalidMessageForTesting()
#else
#define WEB_PROCESS_TERMINATE_CONDITION true
#endif

#define TERMINATE_WEB_PROCESS_WITH_MESSAGE(message) \
    if (WEB_PROCESS_TERMINATE_CONDITION) { \
        RELEASE_LOG_FAULT(IPC, "Requesting termination of web process %" PRIu64 " for reason: %" PUBLIC_LOG_STRING, m_gpuConnectionToWebProcess->webProcessIdentifier().toUInt64(), #message); \
        m_gpuConnectionToWebProcess->terminateWebProcess(); \
    }

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

#if PLATFORM(COCOA)
static bool isSmallLayerBacking(const ImageBufferBackendParameters& parameters)
{
    const unsigned maxSmallLayerBackingArea = 64u * 64u; // 4096 == 16kb backing store which equals 1 page on AS.
    return parameters.purpose == RenderingPurpose::LayerBacking
        && ImageBufferBackend::calculateBackendSize(parameters).area() <= maxSmallLayerBackingArea
        && (parameters.pixelFormat == PixelFormat::BGRA8 || parameters.pixelFormat == PixelFormat::BGRX8);
}
#endif

Ref<RemoteRenderingBackend> RemoteRenderingBackend::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackendCreationParameters&& creationParameters, Ref<IPC::StreamServerConnection>&& streamConnection)
{
    auto instance = adoptRef(*new RemoteRenderingBackend(gpuConnectionToWebProcess, WTFMove(creationParameters), WTFMove(streamConnection)));
    instance->startListeningForIPC();
    return instance;
}

RemoteRenderingBackend::RemoteRenderingBackend(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackendCreationParameters&& creationParameters, Ref<IPC::StreamServerConnection>&& streamConnection)
    : m_workQueue(IPC::StreamConnectionWorkQueue::create("RemoteRenderingBackend work queue"))
    , m_streamConnection(WTFMove(streamConnection))
    , m_remoteResourceCache(gpuConnectionToWebProcess.webProcessIdentifier())
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_resourceOwner(gpuConnectionToWebProcess.webProcessIdentity())
    , m_renderingBackendIdentifier(creationParameters.identifier)
#if HAVE(IOSURFACE)
    , m_ioSurfacePool(IOSurfacePool::create())
#endif
    , m_shapeDetectionObjectHeap(ShapeDetection::ObjectHeap::create())
{
    ASSERT(RunLoop::isMain());
}

RemoteRenderingBackend::~RemoteRenderingBackend() = default;

void RemoteRenderingBackend::startListeningForIPC()
{
    dispatch([this] {
        workQueueInitialize();
    });
}

void RemoteRenderingBackend::stopListeningForIPC()
{
    workQueue().stopAndWaitForCompletion([this] {
        workQueueUninitialize();
    });
}

void RemoteRenderingBackend::workQueueInitialize()
{
    assertIsCurrent(workQueue());
    m_streamConnection->open(m_workQueue.get());
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteRenderingBackend::messageReceiverName(), m_renderingBackendIdentifier.toUInt64());
    send(Messages::RemoteRenderingBackendProxy::DidInitialize(workQueue().wakeUpSemaphore(), m_streamConnection->clientWaitSemaphore()), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::workQueueUninitialize()
{
    assertIsCurrent(workQueue());
    m_remoteDisplayLists.clear();
    // Make sure we destroy the ResourceCache on the WorkQueue since it gets populated on the WorkQueue.
    m_remoteResourceCache = { m_gpuConnectionToWebProcess->webProcessIdentifier() };
    m_streamConnection->stopReceivingMessages(Messages::RemoteRenderingBackend::messageReceiverName(), m_renderingBackendIdentifier.toUInt64());
    m_streamConnection->invalidate();
}

void RemoteRenderingBackend::dispatch(Function<void()>&& task)
{
    workQueue().dispatch(WTFMove(task));
}

IPC::Connection* RemoteRenderingBackend::messageSenderConnection() const
{
    return &m_streamConnection->connection();
}

uint64_t RemoteRenderingBackend::messageSenderDestinationID() const
{
    return m_renderingBackendIdentifier.toUInt64();
}

void RemoteRenderingBackend::didCreateImageBuffer(Ref<RemoteImageBuffer> imageBuffer, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    assertIsCurrent(workQueue());
    MESSAGE_CHECK(renderingResourceIdentifier.processIdentifier() == m_gpuConnectionToWebProcess->webProcessIdentifier(), "Sending didCreateImageBufferBackend() message to the wrong web process.");
    auto remoteDisplayList = RemoteDisplayListRecorder::create(imageBuffer.get(), renderingResourceIdentifier, renderingResourceIdentifier.processIdentifier(), *this);
    m_remoteDisplayLists.add(renderingResourceIdentifier, WTFMove(remoteDisplayList));
    auto* sharing = imageBuffer->backend()->toBackendSharing();
    auto handle = downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();
    m_remoteResourceCache.cacheImageBuffer(WTFMove(imageBuffer), renderingResourceIdentifier);
    send(Messages::RemoteImageBufferProxy::DidCreateBackend(WTFMove(handle)), renderingResourceIdentifier.object());
}

void RemoteRenderingBackend::moveToSerializedBuffer(WebCore::RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    QualifiedRenderingResourceIdentifier qualifiedIdentifier { identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() };
    // Destroy the DisplayListRecorder which plays back to this image buffer.
    m_remoteDisplayLists.take(qualifiedIdentifier);
    // This transfers ownership of the RemoteImageBuffer to the transfer heap.
    auto imageBuffer = m_remoteResourceCache.takeImageBuffer(qualifiedIdentifier);
    if (!imageBuffer) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
    m_gpuConnectionToWebProcess->serializedImageBufferHeap().add({ identifier, 0 }, WTFMove(imageBuffer));
}

void RemoteRenderingBackend::moveToImageBuffer(WebCore::RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    auto imageBuffer = m_gpuConnectionToWebProcess->serializedImageBufferHeap().take({ { identifier, 0 }, 0 }, IPC::Timeout::infinity());
    if (!imageBuffer) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
    QualifiedRenderingResourceIdentifier qualifiedIdentifier { identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() };

    WebCore::ImageBufferCreationContext creationContext { nullptr };
#if HAVE(IOSURFACE)
    creationContext.surfacePool = &ioSurfacePool();
#endif
    creationContext.resourceOwner = m_resourceOwner;
    imageBuffer->backend()->transferToNewContext(creationContext);
    didCreateImageBuffer(imageBuffer.releaseNonNull(), qualifiedIdentifier);
}

void RemoteRenderingBackend::createImageBuffer(const FloatSize& logicalSize, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    QualifiedRenderingResourceIdentifier qualifiedIdentifier { identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() };
    RefPtr<RemoteImageBuffer> imageBuffer;
    WebCore::ImageBufferCreationContext creationContext { nullptr };
#if HAVE(IOSURFACE)
    creationContext.surfacePool = &ioSurfacePool();
#endif
    creationContext.resourceOwner = m_resourceOwner;

    if (renderingMode == RenderingMode::Accelerated) {
#if PLATFORM(COCOA)
        if (isSmallLayerBacking({ logicalSize, resolutionScale, colorSpace, pixelFormat, purpose }))
            imageBuffer = ImageBuffer::create<ImageBufferShareableMappedIOSurfaceBitmapBackend, RemoteImageBuffer>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext, identifier);
#endif
        if (!imageBuffer)
            imageBuffer = ImageBuffer::create<AcceleratedImageBufferShareableMappedBackend, RemoteImageBuffer>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext, identifier);
    }

    if (!imageBuffer)
        imageBuffer = ImageBuffer::create<UnacceleratedImageBufferShareableBackend, RemoteImageBuffer>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext, identifier);

    if (!imageBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }
    didCreateImageBuffer(imageBuffer.releaseNonNull(), qualifiedIdentifier);
}

void RemoteRenderingBackend::getPixelBufferForImageBuffer(RenderingResourceIdentifier imageBuffer, PixelBufferFormat&& destinationFormat, IntRect&& srcRect, CompletionHandler<void()>&& completionHandler)
{
    MESSAGE_CHECK(m_getPixelBufferSharedMemory, "No shared memory for getPixelBufferForImageBuffer");
    MESSAGE_CHECK(PixelBuffer::supportedPixelFormat(destinationFormat.pixelFormat), "Pixel format not supported");
    QualifiedRenderingResourceIdentifier qualifiedImageBuffer { imageBuffer, m_gpuConnectionToWebProcess->webProcessIdentifier() };
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(qualifiedImageBuffer)) {
        auto pixelBuffer = imageBuffer->getPixelBuffer(destinationFormat, srcRect);
        if (pixelBuffer) {
            MESSAGE_CHECK(pixelBuffer->sizeInBytes() <= m_getPixelBufferSharedMemory->size(), "Shmem for return of getPixelBuffer is too small");
            memcpy(m_getPixelBufferSharedMemory->data(), pixelBuffer->bytes(), pixelBuffer->sizeInBytes());
        } else
            memset(m_getPixelBufferSharedMemory->data(), 0, m_getPixelBufferSharedMemory->size());
    }
    completionHandler();
}

void RemoteRenderingBackend::getPixelBufferForImageBufferWithNewMemory(RenderingResourceIdentifier imageBuffer, SharedMemory::Handle&& handle, PixelBufferFormat&& destinationFormat, IntRect&& srcRect, CompletionHandler<void()>&& completionHandler)
{
    m_getPixelBufferSharedMemory = nullptr;
    auto sharedMemory = WebKit::SharedMemory::map(WTFMove(handle), WebKit::SharedMemory::Protection::ReadWrite);
    MESSAGE_CHECK(sharedMemory, "Shared memory could not be mapped.");
    m_getPixelBufferSharedMemory = WTFMove(sharedMemory);
    getPixelBufferForImageBuffer(imageBuffer, WTFMove(destinationFormat), WTFMove(srcRect), WTFMove(completionHandler));
}

void RemoteRenderingBackend::destroyGetPixelBufferSharedMemory()
{
    m_getPixelBufferSharedMemory = nullptr;
}

void RemoteRenderingBackend::putPixelBufferForImageBuffer(RenderingResourceIdentifier imageBuffer, Ref<PixelBuffer>&& pixelBuffer, IntRect&& srcRect, IntPoint&& destPoint, AlphaPremultiplication destFormat)
{
    QualifiedRenderingResourceIdentifier qualifiedImageBuffer { imageBuffer, m_gpuConnectionToWebProcess->webProcessIdentifier() };
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(qualifiedImageBuffer)) {
        imageBuffer->putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat);
    }
}

void RemoteRenderingBackend::getShareableBitmapForImageBuffer(RenderingResourceIdentifier identifier, PreserveResolution preserveResolution, CompletionHandler<void(ShareableBitmap::Handle&&)>&& completionHandler)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    getShareableBitmapForImageBufferWithQualifiedIdentifier({ identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() }, preserveResolution, WTFMove(completionHandler));
}

void RemoteRenderingBackend::getShareableBitmapForImageBufferWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier identifier, PreserveResolution preserveResolution, CompletionHandler<void(ShareableBitmap::Handle&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    ShareableBitmap::Handle handle;
    [&]() {
        auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(identifier);
        if (!imageBuffer)
            return;
        auto backendSize = imageBuffer->backendSize();
        auto logicalSize = imageBuffer->logicalSize();
        auto resultSize = preserveResolution == PreserveResolution::Yes ? backendSize : imageBuffer->truncatedLogicalSize();
        auto bitmap = ShareableBitmap::create({ resultSize, imageBuffer->colorSpace() });
        if (!bitmap)
            return;
        auto context = bitmap->createGraphicsContext();
        if (!context)
            return;
        context->drawImageBuffer(*imageBuffer, FloatRect { { }, resultSize }, FloatRect { { }, logicalSize }, { CompositeOperator::Copy });
        if (auto bitmapHandle = bitmap->createHandle())
            handle = WTFMove(*bitmapHandle);
    }();
    completionHandler(WTFMove(handle));
}

void RemoteRenderingBackend::getFilteredImageForImageBuffer(RenderingResourceIdentifier identifier, Ref<Filter> filter, CompletionHandler<void(ShareableBitmap::Handle&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    ShareableBitmap::Handle handle;
    [&]() {
        auto imageBuffer = m_remoteResourceCache.cachedImageBuffer({ identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
        if (!imageBuffer)
            return;
        auto image = imageBuffer->filteredImage(filter);
        if (!image)
            return;
        auto imageSize = image->size();
        auto bitmap = ShareableBitmap::create({ IntSize(imageSize), imageBuffer->colorSpace() });
        if (!bitmap)
            return;
        auto context = bitmap->createGraphicsContext();
        if (!context)
            return;
        context->drawImage(*image, FloatPoint());
        if (auto bitmapHandle = bitmap->createHandle())
            handle = WTFMove(*bitmapHandle);
    }();
    completionHandler(WTFMove(handle));
}

void RemoteRenderingBackend::cacheNativeImage(ShareableBitmap::Handle&& handle, RenderingResourceIdentifier nativeImageResourceIdentifier)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    cacheNativeImageWithQualifiedIdentifier(WTFMove(handle), { nativeImageResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::cacheNativeImageWithQualifiedIdentifier(ShareableBitmap::Handle&& handle, QualifiedRenderingResourceIdentifier nativeImageResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());

    auto bitmap = ShareableBitmap::create(WTFMove(handle));
    if (!bitmap)
        return;

    auto image = NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes), nativeImageResourceIdentifier.object());
    if (!image)
        return;

    m_remoteResourceCache.cacheNativeImage(image.releaseNonNull(), nativeImageResourceIdentifier);
}

void RemoteRenderingBackend::cacheFont(const WebCore::Font::Attributes& fontAttributes, WebCore::FontPlatformData::Attributes platformData, std::optional<WebCore::RenderingResourceIdentifier> renderingResourceIdentifier)
{
    FontCustomPlatformData* customPlatformData = nullptr;
    if (renderingResourceIdentifier) {
        QualifiedRenderingResourceIdentifier ident = { *renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() };
        customPlatformData = m_remoteResourceCache.cachedFontCustomPlatformData(ident);
        MESSAGE_CHECK(customPlatformData, "CacheFont without caching custom data");
    }

    FontPlatformData platform = FontPlatformData::create(platformData, customPlatformData);

    Ref<Font> font = Font::create(platform, fontAttributes.origin, fontAttributes.isInterstitial, fontAttributes.visibility, fontAttributes.isTextOrientationFallback, fontAttributes.renderingResourceIdentifier);

    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    cacheFontWithQualifiedIdentifier(WTFMove(font), { font->renderingResourceIdentifier(), m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::cacheFontWithQualifiedIdentifier(Ref<Font>&& font, QualifiedRenderingResourceIdentifier fontResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.cacheFont(WTFMove(font), fontResourceIdentifier);
}

void RemoteRenderingBackend::cacheFontCustomPlatformData(Ref<FontCustomPlatformData>&& customPlatformData)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    auto renderingResourceIdentifier = customPlatformData->m_renderingResourceIdentifier;
    cacheFontCustomPlatformDataWithQualifiedIdentifier(WTFMove(customPlatformData), { renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::cacheFontCustomPlatformDataWithQualifiedIdentifier(Ref<FontCustomPlatformData>&& customPlatformData, QualifiedRenderingResourceIdentifier fontResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.cacheFontCustomPlatformData(WTFMove(customPlatformData), fontResourceIdentifier);
}

void RemoteRenderingBackend::cacheDecomposedGlyphs(Ref<DecomposedGlyphs>&& decomposedGlyphs)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    auto renderingResourceIdentifier = decomposedGlyphs->renderingResourceIdentifier();
    cacheDecomposedGlyphsWithQualifiedIdentifier(WTFMove(decomposedGlyphs), { renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::cacheDecomposedGlyphsWithQualifiedIdentifier(Ref<DecomposedGlyphs>&& decomposedGlyphs, QualifiedRenderingResourceIdentifier decomposedGlyphsIdentifier)
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.cacheDecomposedGlyphs(WTFMove(decomposedGlyphs), decomposedGlyphsIdentifier);
}

void RemoteRenderingBackend::cacheGradient(Ref<Gradient>&& gradient, RenderingResourceIdentifier renderingResourceIdentifier)
{
    cacheGradientWithQualifiedIdentifier(WTFMove(gradient), { renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::cacheGradientWithQualifiedIdentifier(Ref<Gradient>&& gradient, QualifiedRenderingResourceIdentifier gradientResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.cacheGradient(WTFMove(gradient), gradientResourceIdentifier);
}

void RemoteRenderingBackend::cacheFilter(Ref<Filter>&& filter, RenderingResourceIdentifier renderingResourceIdentifier)
{
    cacheFilterWithQualifiedIdentifier(WTFMove(filter), { renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::cacheFilterWithQualifiedIdentifier(Ref<Filter>&& filter, QualifiedRenderingResourceIdentifier filterResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.cacheFilter(WTFMove(filter), filterResourceIdentifier);
}

void RemoteRenderingBackend::releaseAllResources()
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.releaseAllResources();
}

void RemoteRenderingBackend::releaseAllImageResources()
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.releaseAllImageResources();
}

void RemoteRenderingBackend::releaseRenderingResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    releaseRenderingResourceWithQualifiedIdentifier({ renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::releaseRenderingResourceWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    assertIsCurrent(workQueue());
    m_remoteDisplayLists.take(renderingResourceIdentifier);
    auto success = m_remoteResourceCache.releaseRenderingResource(renderingResourceIdentifier);
    MESSAGE_CHECK(success, "Resource is being released before being cached.");
}

#if PLATFORM(COCOA)
static std::optional<ImageBufferBackendHandle> handleFromBuffer(ImageBuffer& buffer)
{
    auto* backend = buffer.ensureBackendCreated();
    if (!backend)
        return std::nullopt;

    auto* sharing = backend->toBackendSharing();
    if (is<ImageBufferBackendHandleSharing>(sharing))
        return downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();

    return std::nullopt;
}

void RemoteRenderingBackend::prepareBuffersForDisplay(Vector<PrepareBackingStoreBuffersInputData> swapBuffersInput, CompletionHandler<void(Vector<PrepareBackingStoreBuffersOutputData>&&)>&& completionHandler)
{
    Vector<PrepareBackingStoreBuffersOutputData> outputData;
    outputData.resizeToFit(swapBuffersInput.size());

    for (unsigned i = 0; i < swapBuffersInput.size(); ++i)
        prepareLayerBuffersForDisplay(swapBuffersInput[i], outputData[i]);

    completionHandler(WTFMove(outputData));
}

// This is the GPU Process version of RemoteLayerBackingStore::prepareBuffers().
void RemoteRenderingBackend::prepareLayerBuffersForDisplay(const PrepareBackingStoreBuffersInputData& inputData, PrepareBackingStoreBuffersOutputData& outputData)
{
    auto fetchBuffer = [&](std::optional<RenderingResourceIdentifier> identifier) -> ImageBuffer* {
        return identifier ? m_remoteResourceCache.cachedImageBuffer({ *identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() }) : nullptr;
    };

    auto bufferIdentifier = [](ImageBuffer* buffer) -> std::optional<RenderingResourceIdentifier> {
        if (!buffer)
            return std::nullopt;
        return buffer->renderingResourceIdentifier();
    };

    auto frontBuffer = fetchBuffer(inputData.bufferSet.front);
    auto backBuffer = fetchBuffer(inputData.bufferSet.back);
    auto secondaryBackBuffer = fetchBuffer(inputData.bufferSet.secondaryBack);

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "GPU Process: RemoteRenderingBackend::prepareBuffersForDisplay - front "
        << inputData.bufferSet.front << " (in-use " << (frontBuffer && frontBuffer->isInUse()) << ") "
        << inputData.bufferSet.back << " (in-use " << (backBuffer && backBuffer->isInUse()) << ") "
        << inputData.bufferSet.secondaryBack << " (in-use " << (secondaryBackBuffer && secondaryBackBuffer->isInUse()) << ") ");

    bool needsFullDisplay = false;

    if (frontBuffer) {
        auto previousState = frontBuffer->setNonVolatile();
        if (previousState == SetNonVolatileResult::Empty)
            needsFullDisplay = true;
    }

    if (frontBuffer && !needsFullDisplay && inputData.hasEmptyDirtyRegion) {
        // No swap necessary, but we do need to return the front buffer handle.
        outputData.frontBufferHandle = handleFromBuffer(*frontBuffer);
        outputData.bufferSet = BufferIdentifierSet { bufferIdentifier(frontBuffer), bufferIdentifier(backBuffer), bufferIdentifier(secondaryBackBuffer) };
        outputData.displayRequirement = SwapBuffersDisplayRequirement::NeedsNoDisplay;
        return;
    }

    if (!frontBuffer || !inputData.supportsPartialRepaint || isSmallLayerBacking(frontBuffer->parameters()))
        needsFullDisplay = true;

    if (!backBuffer || backBuffer->isInUse()) {
        std::swap(backBuffer, secondaryBackBuffer);

        // When pulling the secondary back buffer out of hibernation (to become
        // the new front buffer), if it is somehow still in use (e.g. we got
        // three swaps ahead of the render server), just give up and discard it.
        if (backBuffer && backBuffer->isInUse())
            backBuffer = nullptr;
    }

    std::swap(frontBuffer, backBuffer);

    outputData.bufferSet = BufferIdentifierSet { bufferIdentifier(frontBuffer), bufferIdentifier(backBuffer), bufferIdentifier(secondaryBackBuffer) };
    if (frontBuffer) {
        auto previousState = frontBuffer->setNonVolatile();
        if (previousState == SetNonVolatileResult::Empty)
            needsFullDisplay = true;

        outputData.frontBufferHandle = handleFromBuffer(*frontBuffer);
    } else
        needsFullDisplay = true;

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "GPU Process: prepareBuffersForDisplay - swapped from ["
        << inputData.bufferSet.front << ", " << inputData.bufferSet.back << ", " << inputData.bufferSet.secondaryBack << "] to ["
        << outputData.bufferSet.front << ", " << outputData.bufferSet.back << ", " << outputData.bufferSet.secondaryBack << "]");

    outputData.displayRequirement = needsFullDisplay ? SwapBuffersDisplayRequirement::NeedsFullDisplay : SwapBuffersDisplayRequirement::NeedsNormalDisplay;
}
#endif

void RemoteRenderingBackend::markSurfacesVolatile(MarkSurfacesAsVolatileRequestIdentifier requestIdentifier, const Vector<RenderingResourceIdentifier>& identifiers)
{
    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "GPU Process: RemoteRenderingBackend::markSurfacesVolatile " << identifiers);

    auto makeVolatile = [](ImageBuffer& imageBuffer) {
        imageBuffer.releaseGraphicsContext();
        return imageBuffer.setVolatile();
    };

    Vector<RenderingResourceIdentifier> markedVolatileBufferIdentifiers;
    for (auto identifier : identifiers) {
        auto imageBuffer = m_remoteResourceCache.cachedImageBuffer({ identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
        if (imageBuffer) {
            if (makeVolatile(*imageBuffer))
                markedVolatileBufferIdentifiers.append(identifier);
        } else
            LOG_WITH_STREAM(RemoteLayerBuffers, stream << " failed to find ImageBuffer for identifier " << identifier);
    }

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "GPU Process: markSurfacesVolatile - surfaces marked volatile " << markedVolatileBufferIdentifiers);

    bool didMarkAllLayerAsVolatile = identifiers.size() == markedVolatileBufferIdentifiers.size();
    send(Messages::RemoteRenderingBackendProxy::DidMarkLayersAsVolatile(requestIdentifier, markedVolatileBufferIdentifiers, didMarkAllLayerAsVolatile), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::finalizeRenderingUpdate(RenderingUpdateID renderingUpdateID)
{
    send(Messages::RemoteRenderingBackendProxy::DidFinalizeRenderingUpdate(renderingUpdateID), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::lowMemoryHandler(Critical, Synchronous)
{
    ASSERT(isMainRunLoop());
#if HAVE(IOSURFACE)
    m_ioSurfacePool->discardAllSurfaces();
#endif
}

void RemoteRenderingBackend::createRemoteBarcodeDetector(ShapeDetectionIdentifier identifier, const WebCore::ShapeDetection::BarcodeDetectorOptions& barcodeDetectorOptions)
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    auto inner = WebCore::ShapeDetection::BarcodeDetectorImpl::create(barcodeDetectorOptions);
    auto remoteBarcodeDetector = RemoteBarcodeDetector::create(WTFMove(inner), m_shapeDetectionObjectHeap, remoteResourceCache(), identifier, gpuConnectionToWebProcess().webProcessIdentifier());
    m_shapeDetectionObjectHeap->addObject(identifier, remoteBarcodeDetector);
    streamConnection().startReceivingMessages(remoteBarcodeDetector, Messages::RemoteBarcodeDetector::messageReceiverName(), identifier.toUInt64());
#else
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(barcodeDetectorOptions);
#endif
}

void RemoteRenderingBackend::releaseRemoteBarcodeDetector(ShapeDetectionIdentifier identifier)
{
    streamConnection().stopReceivingMessages(Messages::RemoteBarcodeDetector::messageReceiverName(), identifier.toUInt64());
    m_shapeDetectionObjectHeap->removeObject(identifier);
}

void RemoteRenderingBackend::getRemoteBarcodeDetectorSupportedFormats(CompletionHandler<void(Vector<WebCore::ShapeDetection::BarcodeFormat>&&)>&& completionHandler)
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    WebCore::ShapeDetection::BarcodeDetectorImpl::getSupportedFormats(WTFMove(completionHandler));
#else
    completionHandler({ });
#endif
}

void RemoteRenderingBackend::createRemoteFaceDetector(ShapeDetectionIdentifier identifier, const WebCore::ShapeDetection::FaceDetectorOptions& faceDetectorOptions)
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    auto inner = WebCore::ShapeDetection::FaceDetectorImpl::create(faceDetectorOptions);
    auto remoteFaceDetector = RemoteFaceDetector::create(WTFMove(inner), m_shapeDetectionObjectHeap, remoteResourceCache(), identifier, gpuConnectionToWebProcess().webProcessIdentifier());
    m_shapeDetectionObjectHeap->addObject(identifier, remoteFaceDetector);
    streamConnection().startReceivingMessages(remoteFaceDetector, Messages::RemoteFaceDetector::messageReceiverName(), identifier.toUInt64());
#else
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(faceDetectorOptions);
#endif
}

void RemoteRenderingBackend::releaseRemoteFaceDetector(ShapeDetectionIdentifier identifier)
{
    streamConnection().stopReceivingMessages(Messages::RemoteFaceDetector::messageReceiverName(), identifier.toUInt64());
    m_shapeDetectionObjectHeap->removeObject(identifier);
}

void RemoteRenderingBackend::createRemoteTextDetector(ShapeDetectionIdentifier identifier)
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    auto inner = WebCore::ShapeDetection::TextDetectorImpl::create();
    auto remoteTextDetector = RemoteTextDetector::create(WTFMove(inner), m_shapeDetectionObjectHeap, remoteResourceCache(), identifier, gpuConnectionToWebProcess().webProcessIdentifier());
    m_shapeDetectionObjectHeap->addObject(identifier, remoteTextDetector);
    streamConnection().startReceivingMessages(remoteTextDetector, Messages::RemoteTextDetector::messageReceiverName(), identifier.toUInt64());
#else
    UNUSED_PARAM(identifier);
#endif
}

void RemoteRenderingBackend::releaseRemoteTextDetector(ShapeDetectionIdentifier identifier)
{
    streamConnection().stopReceivingMessages(Messages::RemoteTextDetector::messageReceiverName(), identifier.toUInt64());
    m_shapeDetectionObjectHeap->removeObject(identifier);
}

} // namespace WebKit

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_WITH_RETURN_VALUE

#endif // ENABLE(GPU_PROCESS)
