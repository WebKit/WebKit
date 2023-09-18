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
#include <WebCore/RenderingResourceIdentifier.h>
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

#define MESSAGE_CHECK(assertion, message) do { \
    if (UNLIKELY(!(assertion))) { \
        terminateWebProcess(message); \
        return; \
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
    m_remoteImageBuffers.clear();
    // Make sure we destroy the ResourceCache on the WorkQueue since it gets populated on the WorkQueue.
    m_remoteResourceCache.releaseAllResources();
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

void RemoteRenderingBackend::didCreateImageBuffer(Ref<ImageBuffer> imageBuffer)
{
    assertIsCurrent(workQueue());
    auto imageBufferIdentifier = imageBuffer->renderingResourceIdentifier();
    auto remoteDisplayList = RemoteDisplayListRecorder::create(imageBuffer.get(), imageBufferIdentifier, *this);
    m_remoteDisplayLists.add(imageBufferIdentifier, WTFMove(remoteDisplayList));
    auto* sharing = imageBuffer->backend()->toBackendSharing();
    auto handle = downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();
    auto remoteImageBuffer = RemoteImageBuffer::create(WTFMove(imageBuffer), *this);
    m_remoteImageBuffers.add(imageBufferIdentifier, WTFMove(remoteImageBuffer));
    send(Messages::RemoteImageBufferProxy::DidCreateBackend(WTFMove(*handle)), imageBufferIdentifier);
}

void RemoteRenderingBackend::moveToSerializedBuffer(RenderingResourceIdentifier imageBufferIdentifier)
{
    assertIsCurrent(workQueue());
    // Destroy the DisplayListRecorder which plays back to this image buffer.
    m_remoteDisplayLists.take(imageBufferIdentifier);
    // This transfers ownership of the RemoteImageBuffer contents to the transfer heap.
    auto imageBuffer = takeImageBuffer(imageBufferIdentifier);
    if (!imageBuffer) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
    m_gpuConnectionToWebProcess->serializedImageBufferHeap().add({ imageBufferIdentifier, 0 }, WTFMove(imageBuffer));
}

void RemoteRenderingBackend::moveToImageBuffer(RenderingResourceIdentifier imageBufferIdentifier)
{
    assertIsCurrent(workQueue());
    auto imageBuffer = m_gpuConnectionToWebProcess->serializedImageBufferHeap().take({ { imageBufferIdentifier, 0 }, 0 }, IPC::Timeout::infinity());
    if (!imageBuffer) {
        ASSERT_IS_TESTING_IPC();
        return;
    }

    ASSERT(imageBufferIdentifier == imageBuffer->renderingResourceIdentifier());

    ImageBufferCreationContext creationContext;
#if HAVE(IOSURFACE)
    creationContext.surfacePool = &ioSurfacePool();
#endif
    creationContext.resourceOwner = m_resourceOwner;
    imageBuffer->backend()->transferToNewContext(creationContext);
    didCreateImageBuffer(imageBuffer.releaseNonNull());
}

void RemoteRenderingBackend::createImageBuffer(const FloatSize& logicalSize, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, RenderingResourceIdentifier imageBufferIdentifier)
{
    assertIsCurrent(workQueue());
    RefPtr<ImageBuffer> imageBuffer;
    ImageBufferCreationContext creationContext;
#if HAVE(IOSURFACE)
    creationContext.surfacePool = &ioSurfacePool();
#endif
    creationContext.resourceOwner = m_resourceOwner;

    if (renderingMode == RenderingMode::Accelerated) {
#if PLATFORM(COCOA)
        if (isSmallLayerBacking({ logicalSize, resolutionScale, colorSpace, pixelFormat, purpose }))
            imageBuffer = ImageBuffer::create<ImageBufferShareableMappedIOSurfaceBitmapBackend>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext, imageBufferIdentifier);
#endif
        if (!imageBuffer)
            imageBuffer = ImageBuffer::create<AcceleratedImageBufferShareableMappedBackend>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext, imageBufferIdentifier);
    }

    if (!imageBuffer)
        imageBuffer = ImageBuffer::create<UnacceleratedImageBufferShareableBackend>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext, imageBufferIdentifier);

    if (!imageBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }
    didCreateImageBuffer(imageBuffer.releaseNonNull());
}

void RemoteRenderingBackend::releaseImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    assertIsCurrent(workQueue());
    m_remoteDisplayLists.take(renderingResourceIdentifier);
    bool success = m_remoteImageBuffers.take(renderingResourceIdentifier).get();
    MESSAGE_CHECK(success, "Resource is being released before being cached."_s);
}

void RemoteRenderingBackend::destroyGetPixelBufferSharedMemory()
{
    m_getPixelBufferSharedMemory = nullptr;
}


void RemoteRenderingBackend::cacheNativeImage(ShareableBitmap::Handle&& handle, RenderingResourceIdentifier nativeImageIdentifier)
{
    ASSERT(!RunLoop::isMain());

    auto bitmap = ShareableBitmap::create(WTFMove(handle));
    if (!bitmap)
        return;

    auto image = NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes), nativeImageIdentifier);
    if (!image)
        return;

    m_remoteResourceCache.cacheNativeImage(image.releaseNonNull());
}

void RemoteRenderingBackend::cacheFont(const Font::Attributes& fontAttributes, FontPlatformData::Attributes platformData, std::optional<RenderingResourceIdentifier> fontCustomPlatformDataIdentifier)
{
    ASSERT(!RunLoop::isMain());

    RefPtr<FontCustomPlatformData> customPlatformData = nullptr;
    if (fontCustomPlatformDataIdentifier) {
        customPlatformData = m_remoteResourceCache.cachedFontCustomPlatformData(*fontCustomPlatformDataIdentifier);
        MESSAGE_CHECK(customPlatformData, "CacheFont without caching custom data"_s);
    }

    FontPlatformData platform = FontPlatformData::create(platformData, customPlatformData.get());

    Ref<Font> font = Font::create(platform, fontAttributes.origin, fontAttributes.isInterstitial, fontAttributes.visibility, fontAttributes.isTextOrientationFallback, fontAttributes.renderingResourceIdentifier);

    m_remoteResourceCache.cacheFont(WTFMove(font));
}

void RemoteRenderingBackend::cacheFontCustomPlatformData(Ref<FontCustomPlatformData>&& customPlatformData)
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.cacheFontCustomPlatformData(WTFMove(customPlatformData));
}

void RemoteRenderingBackend::cacheDecomposedGlyphs(Ref<DecomposedGlyphs>&& decomposedGlyphs)
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.cacheDecomposedGlyphs(WTFMove(decomposedGlyphs));
}

void RemoteRenderingBackend::cacheGradient(Ref<Gradient>&& gradient)
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.cacheGradient(WTFMove(gradient));
}

void RemoteRenderingBackend::cacheFilter(Ref<Filter>&& filter)
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.cacheFilter(WTFMove(filter));
}

void RemoteRenderingBackend::releaseAllDrawingResources()
{

    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.releaseAllDrawingResources();
}

void RemoteRenderingBackend::releaseAllImageResources()
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.releaseAllImageResources();
}

void RemoteRenderingBackend::releaseRenderingResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseRenderingResource(renderingResourceIdentifier);
    MESSAGE_CHECK(success, "Resource is being released before being cached."_s);
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
    auto fetchBuffer = [&](std::optional<RenderingResourceIdentifier> identifier) -> RefPtr<ImageBuffer> {
        return identifier ? imageBuffer(*identifier) : nullptr;
    };

    auto bufferIdentifier = [](RefPtr<WebCore::ImageBuffer> buffer) -> std::optional<RenderingResourceIdentifier> {
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
        if (auto target = imageBuffer(identifier)) {
            if (makeVolatile(*target))
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
    auto remoteBarcodeDetector = RemoteBarcodeDetector::create(WTFMove(inner), m_shapeDetectionObjectHeap, *this, identifier, gpuConnectionToWebProcess().webProcessIdentifier());
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
    auto remoteFaceDetector = RemoteFaceDetector::create(WTFMove(inner), m_shapeDetectionObjectHeap, *this, identifier, gpuConnectionToWebProcess().webProcessIdentifier());
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
    auto remoteTextDetector = RemoteTextDetector::create(WTFMove(inner), m_shapeDetectionObjectHeap, *this, identifier, gpuConnectionToWebProcess().webProcessIdentifier());
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

RefPtr<ImageBuffer> RemoteRenderingBackend::imageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    assertIsCurrent(workQueue());
    RefPtr<RemoteImageBuffer> remoteImageBuffer = m_remoteImageBuffers.get(renderingResourceIdentifier);
    if (!remoteImageBuffer.get())
        return nullptr;
    return remoteImageBuffer->imageBuffer();
}

RefPtr<ImageBuffer> RemoteRenderingBackend::takeImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    assertIsCurrent(workQueue());
    auto remoteImageBufferReceiveQueue = m_remoteImageBuffers.take(renderingResourceIdentifier);
    if (!remoteImageBufferReceiveQueue.get())
        return nullptr;
    RefPtr remoteImageBuffer = remoteImageBufferReceiveQueue.get();
    remoteImageBufferReceiveQueue.reset();
    ASSERT(remoteImageBuffer->hasOneRef());
    return remoteImageBuffer->imageBuffer();
}

void RemoteRenderingBackend::terminateWebProcess(ASCIILiteral message)
{
#if ENABLE(IPC_TESTING_API)
    bool shouldTerminate = !m_gpuConnectionToWebProcess->connection().ignoreInvalidMessageForTesting();
#else
    bool shouldTerminate = true;
#endif
    if (shouldTerminate) {
        RELEASE_LOG_FAULT(IPC, "Requesting termination of web process %" PRIu64 " for reason: %" PUBLIC_LOG_STRING, m_gpuConnectionToWebProcess->webProcessIdentifier().toUInt64(), message.characters());
        m_gpuConnectionToWebProcess->terminateWebProcess();
    }
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
