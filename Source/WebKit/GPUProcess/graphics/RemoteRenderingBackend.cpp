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
#include "GPUProcess.h"
#include "GPUProcessProxyMessages.h"
#include "ImageBufferShareableBitmapBackend.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "RemoteBarcodeDetector.h"
#include "RemoteBarcodeDetectorMessages.h"
#include "RemoteDisplayListRecorder.h"
#include "RemoteDisplayListRecorderMessages.h"
#include "RemoteFaceDetector.h"
#include "RemoteFaceDetectorMessages.h"
#include "RemoteImageBuffer.h"
#include "RemoteImageBufferProxyMessages.h"
#include "RemoteImageBufferSet.h"
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
#include "WebPageProxy.h"
#include <WebCore/HTMLCanvasElement.h>
#include <WebCore/NullImageBufferBackend.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/RunLoop.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>

#if HAVE(IOSURFACE)
#include "ImageBufferRemoteIOSurfaceBackend.h"
#include "ImageBufferShareableMappedIOSurfaceBackend.h"
#include "ImageBufferShareableMappedIOSurfaceBitmapBackend.h"
#include <WebCore/IOSurfacePool.h>
#endif

#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
#import <WebCore/BarcodeDetectorImplementation.h>
#import <WebCore/FaceDetectorImplementation.h>
#import <WebCore/TextDetectorImplementation.h>
#endif

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
#import "DynamicContentScalingBifurcatedImageBuffer.h"
#import "DynamicContentScalingImageBufferBackend.h"
#endif

#define MESSAGE_CHECK(assertion, message) do { \
    if (UNLIKELY(!(assertion))) { \
        terminateWebProcess(message); \
        return; \
    } \
} while (0)

namespace WebKit {
using namespace WebCore;

bool isSmallLayerBacking(const ImageBufferParameters& parameters)
{
    const unsigned maxSmallLayerBackingArea = 64u * 64u; // 4096 == 16kb backing store which equals 1 page on AS.
    auto checkedArea = ImageBuffer::calculateBackendSize(parameters.logicalSize, parameters.resolutionScale).area<RecordOverflow>();
    return (parameters.purpose == RenderingPurpose::LayerBacking || parameters.purpose == RenderingPurpose::BitmapOnlyLayerBacking)
        && !checkedArea.hasOverflowed() && checkedArea <= maxSmallLayerBackingArea
        && (parameters.pixelFormat == PixelFormat::BGRA8 || parameters.pixelFormat == PixelFormat::BGRX8);
}

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
    m_remoteImageBufferSets.clear();
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

void RemoteRenderingBackend::createDisplayListRecorder(RefPtr<ImageBuffer> imageBuffer, RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    if (!imageBuffer) {
        auto errorImage = ImageBuffer::create<NullImageBufferBackend>({ 0, 0 }, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, RenderingPurpose::Unspecified, { }, identifier);
        m_remoteDisplayLists.add(identifier, RemoteDisplayListRecorder::create(*errorImage.get(), identifier, *this));
        return;
    }
    m_remoteDisplayLists.add(identifier, RemoteDisplayListRecorder::create(*imageBuffer.get(), identifier, *this));
}

void RemoteRenderingBackend::releaseDisplayListRecorder(RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    m_remoteDisplayLists.take(identifier);
}

void RemoteRenderingBackend::didFailCreateImageBuffer(RenderingResourceIdentifier imageBufferIdentifier)
{
    // On failure to create a remote image buffer we still create a null display list recorder.
    // Commands to draw to the failed image might have already be issued and we must process
    // them.
    auto errorImage = ImageBuffer::create<NullImageBufferBackend>({ 0, 0 }, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, RenderingPurpose::Unspecified, { }, imageBufferIdentifier);
    RELEASE_ASSERT(errorImage);
    m_remoteDisplayLists.add(imageBufferIdentifier, RemoteDisplayListRecorder::create(*errorImage, imageBufferIdentifier, *this));
    m_remoteImageBuffers.add(imageBufferIdentifier, RemoteImageBuffer::create(errorImage.releaseNonNull(), *this));
    send(Messages::RemoteImageBufferProxy::DidCreateBackend(std::nullopt), imageBufferIdentifier);
}

void RemoteRenderingBackend::didCreateImageBuffer(Ref<ImageBuffer> imageBuffer)
{
    auto imageBufferIdentifier = imageBuffer->renderingResourceIdentifier();
    auto* sharing = imageBuffer->toBackendSharing();
    auto handle = downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();
    m_remoteDisplayLists.add(imageBufferIdentifier, RemoteDisplayListRecorder::create(imageBuffer.get(), imageBufferIdentifier, *this));
    m_remoteImageBuffers.add(imageBufferIdentifier, RemoteImageBuffer::create(WTFMove(imageBuffer), *this));
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
    imageBuffer->transferToNewContext(creationContext);
    didCreateImageBuffer(imageBuffer.releaseNonNull());
}

template<typename ImageBufferType>
static RefPtr<ImageBuffer> allocateImageBufferInternal(const FloatSize& logicalSize, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, ImageBufferCreationContext& creationContext, RenderingResourceIdentifier imageBufferIdentifier)
{
    RefPtr<ImageBuffer> imageBuffer;

#if HAVE(IOSURFACE)
    if (renderingMode == RenderingMode::Accelerated) {
        if (isSmallLayerBacking({ logicalSize, resolutionScale, colorSpace, pixelFormat, purpose }))
            imageBuffer = ImageBuffer::create<ImageBufferShareableMappedIOSurfaceBitmapBackend, ImageBufferType>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext, imageBufferIdentifier);
        if (!imageBuffer)
            imageBuffer = ImageBuffer::create<ImageBufferShareableMappedIOSurfaceBackend, ImageBufferType>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext, imageBufferIdentifier);
    }
#endif
    if (!imageBuffer)
        imageBuffer = ImageBuffer::create<ImageBufferShareableBitmapBackend, ImageBufferType>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, creationContext, imageBufferIdentifier);

    return imageBuffer;
}

RefPtr<ImageBuffer> RemoteRenderingBackend::allocateImageBuffer(const FloatSize& logicalSize, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, const ImageBufferCreationContext& creationContext, RenderingResourceIdentifier imageBufferIdentifier)
{
    assertIsCurrent(workQueue());

    ASSERT(!creationContext.resourceOwner);
#if HAVE(IOSURFACE)
    ASSERT(!creationContext.surfacePool);
#endif
    ImageBufferCreationContext adjustedCreationContext = creationContext;
    adjustedCreationContext.resourceOwner = m_resourceOwner;
#if HAVE(IOSURFACE)
    adjustedCreationContext.surfacePool = &ioSurfacePool();
#endif

    RefPtr<ImageBuffer> imageBuffer;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    if (m_gpuConnectionToWebProcess->isDynamicContentScalingEnabled() && (purpose == RenderingPurpose::LayerBacking || purpose == RenderingPurpose::DOM))
        imageBuffer = allocateImageBufferInternal<DynamicContentScalingBifurcatedImageBuffer>(logicalSize, renderingMode, purpose, resolutionScale, colorSpace, pixelFormat, adjustedCreationContext, imageBufferIdentifier);
#endif

    if (!imageBuffer)
        imageBuffer = allocateImageBufferInternal<ImageBuffer>(logicalSize, renderingMode, purpose, resolutionScale, colorSpace, pixelFormat, adjustedCreationContext, imageBufferIdentifier);

    return imageBuffer;
}


void RemoteRenderingBackend::createImageBuffer(const FloatSize& logicalSize, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, RenderingResourceIdentifier imageBufferIdentifier)
{
    assertIsCurrent(workQueue());
    RefPtr<ImageBuffer> imageBuffer = allocateImageBuffer(logicalSize, renderingMode, purpose, resolutionScale, colorSpace, pixelFormat, { }, imageBufferIdentifier);

    if (imageBuffer)
        didCreateImageBuffer(imageBuffer.releaseNonNull());
    else {
        RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend=%" PRIu64 "] RemoteRenderingBackend::createImageBuffer - failed to allocate image buffer %" PRIu64, m_renderingBackendIdentifier.toUInt64(), imageBufferIdentifier.toUInt64());
        didFailCreateImageBuffer(imageBufferIdentifier);
    }
}

void RemoteRenderingBackend::releaseImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    assertIsCurrent(workQueue());
    m_remoteDisplayLists.take(renderingResourceIdentifier);
    bool success = m_remoteImageBuffers.take(renderingResourceIdentifier).get();
    MESSAGE_CHECK(success, "Resource is being released before being cached."_s);
}

void RemoteRenderingBackend::createRemoteImageBufferSet(RemoteImageBufferSetIdentifier bufferSetIdentifier, WebCore::RenderingResourceIdentifier displayListIdentifier)
{
    assertIsCurrent(workQueue());
    m_remoteImageBufferSets.add(bufferSetIdentifier, RemoteImageBufferSet::create(bufferSetIdentifier, displayListIdentifier, *this));
}

void RemoteRenderingBackend::releaseRemoteImageBufferSet(RemoteImageBufferSetIdentifier bufferSetIdentifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteImageBufferSets.take(bufferSetIdentifier).get();
    MESSAGE_CHECK(success, "BufferSet is being released before being created"_s);
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

void RemoteRenderingBackend::cacheFont(const Font::Attributes& fontAttributes, FontPlatformDataAttributes platformData, std::optional<RenderingResourceIdentifier> fontCustomPlatformDataIdentifier)
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

#if USE(GRAPHICS_LAYER_WC)
void RemoteRenderingBackend::flush(IPC::Semaphore&& semaphore)
{
    semaphore.signal();
}
#endif

#if PLATFORM(COCOA)
void RemoteRenderingBackend::prepareImageBufferSetsForDisplay(Vector<ImageBufferSetPrepareBufferForDisplayInputData> swapBuffersInput)
{
    assertIsCurrent(workQueue());

    for (unsigned i = 0; i < swapBuffersInput.size(); ++i) {
        RefPtr<RemoteImageBufferSet> remoteImageBufferSet = m_remoteImageBufferSets.get(swapBuffersInput[i].remoteBufferSet);
        MESSAGE_CHECK(remoteImageBufferSet, "BufferSet is being updated before being created"_s);
        SwapBuffersDisplayRequirement displayRequirement = SwapBuffersDisplayRequirement::NeedsNormalDisplay;
        remoteImageBufferSet->ensureBufferForDisplay(swapBuffersInput[i], displayRequirement);
        MESSAGE_CHECK(displayRequirement != SwapBuffersDisplayRequirement::NeedsFullDisplay, "Can't asynchronously require full display for a buffer set"_s);

        if (displayRequirement != SwapBuffersDisplayRequirement::NeedsNoDisplay)
            remoteImageBufferSet->prepareBufferForDisplay(swapBuffersInput[i].dirtyRegion, swapBuffersInput[i].requiresClearedPixels);
    }
}

void RemoteRenderingBackend::prepareImageBufferSetsForDisplaySync(Vector<ImageBufferSetPrepareBufferForDisplayInputData> swapBuffersInput, CompletionHandler<void(Vector<SwapBuffersDisplayRequirement>&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());

    Vector<SwapBuffersDisplayRequirement> outputData;
    outputData.resizeToFit(swapBuffersInput.size());

    for (unsigned i = 0; i < swapBuffersInput.size(); ++i) {
        RefPtr<RemoteImageBufferSet> remoteImageBufferSet = m_remoteImageBufferSets.get(swapBuffersInput[i].remoteBufferSet);
        MESSAGE_CHECK(remoteImageBufferSet, "BufferSet is being updated before being created"_s);
        remoteImageBufferSet->ensureBufferForDisplay(swapBuffersInput[i], outputData[i]);
    }

    completionHandler(WTFMove(outputData));

    // Defer preparing all the front buffers (which triggers pixel copy
    // operations) until after we've sent the completion handler (and any
    // buffer backend created messages) to unblock the WebProcess as soon
    // as possible.
    for (unsigned i = 0; i < swapBuffersInput.size(); ++i) {
        RefPtr<RemoteImageBufferSet> remoteImageBufferSet = m_remoteImageBufferSets.get(swapBuffersInput[i].remoteBufferSet);
        MESSAGE_CHECK(remoteImageBufferSet, "BufferSet is being updated before being created"_s);

        if (outputData[i] != SwapBuffersDisplayRequirement::NeedsNoDisplay)
            remoteImageBufferSet->prepareBufferForDisplay(swapBuffersInput[i].dirtyRegion, swapBuffersInput[i].requiresClearedPixels);
    }
}
#endif

void RemoteRenderingBackend::markSurfacesVolatile(MarkSurfacesAsVolatileRequestIdentifier requestIdentifier, const Vector<std::pair<RemoteImageBufferSetIdentifier, OptionSet<BufferInSetType>>>& identifiers, bool forcePurge)
{
    assertIsCurrent(workQueue());
    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "GPU Process: RemoteRenderingBackend::markSurfacesVolatile " << identifiers);

    Vector<std::pair<RemoteImageBufferSetIdentifier, OptionSet<BufferInSetType>>> markedBufferSets;
    bool allSucceeded = true;

    for (auto identifier : identifiers) {
        RefPtr<RemoteImageBufferSet> remoteImageBufferSet = m_remoteImageBufferSets.get(identifier.first);

        MESSAGE_CHECK(remoteImageBufferSet, "BufferSet is being marked volatile before being created"_s);

        OptionSet<BufferInSetType> volatileBuffers;
        if (!remoteImageBufferSet->makeBuffersVolatile(identifier.second, volatileBuffers, forcePurge))
            allSucceeded = false;

        if (!volatileBuffers.isEmpty())
            markedBufferSets.append(std::make_pair(identifier.first, volatileBuffers));
    }

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "GPU Process: markSurfacesVolatile - surfaces marked volatile " << markedBufferSets);
    send(Messages::RemoteRenderingBackendProxy::DidMarkLayersAsVolatile(requestIdentifier, WTFMove(markedBufferSets), allSucceeded), m_renderingBackendIdentifier);
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
