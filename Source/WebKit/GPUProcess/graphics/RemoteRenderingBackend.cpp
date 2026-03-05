/*
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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
#include "PrepareBackingStoreBuffersData.h"
#include "RemoteBarcodeDetector.h"
#include "RemoteBarcodeDetectorMessages.h"
#include "RemoteDisplayListRecorder.h"
#include "RemoteFaceDetector.h"
#include "RemoteFaceDetectorMessages.h"
#include "RemoteGraphicsContext.h"
#include "RemoteImageBuffer.h"
#include "RemoteImageBufferProxyMessages.h"
#include "RemoteImageBufferSet.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "RemoteSharedResourceCache.h"
#include "RemoteSnapshot.h"
#include "RemoteSnapshotRecorder.h"
#include "RemoteTextDetector.h"
#include "RemoteTextDetectorMessages.h"
#include "ShapeDetectionObjectHeap.h"
#include "WebPageProxy.h"
#include <WebCore/Filter.h>
#include <WebCore/FontCustomPlatformData.h>
#include <WebCore/Gradient.h>
#include <WebCore/HTMLCanvasElement.h>
#include <WebCore/ImageBufferDisplayListBackend.h>
#include <WebCore/NullImageBufferBackend.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/RunLoop.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>

#if USE(CG)
#include <WebCore/ImageBufferCGPDFDocumentBackend.h>
#endif

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

#define MESSAGE_CHECK(assertion, message) MESSAGE_CHECK_WITH_MESSAGE_BASE(assertion, m_streamConnection, message);

namespace WebKit {
using namespace WebCore;

bool isSmallLayerBacking(const ImageBufferParameters& parameters)
{
    const unsigned maxSmallLayerBackingArea = 64u * 64u; // 4096 == 16kb backing store which equals 1 page on AS.
    auto checkedArea = ImageBuffer::calculateBackendSize(parameters.logicalSize, parameters.resolutionScale).area<RecordOverflow>();
    return (parameters.purpose == RenderingPurpose::LayerBacking)
        && !checkedArea.hasOverflowed() && checkedArea <= maxSmallLayerBackingArea
        && (parameters.bufferFormat.pixelFormat == PixelFormat::BGRA8 || parameters.bufferFormat.pixelFormat == PixelFormat::BGRX8);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteRenderingBackend);

Ref<RemoteRenderingBackend> RemoteRenderingBackend::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackendIdentifier identifier, Ref<IPC::StreamServerConnection>&& streamConnection)
{
    auto instance = adoptRef(*new RemoteRenderingBackend(gpuConnectionToWebProcess, identifier, WTF::move(streamConnection)));
    instance->startListeningForIPC();
    return instance;
}

RemoteRenderingBackend::RemoteRenderingBackend(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackendIdentifier identifier, Ref<IPC::StreamServerConnection>&& streamConnection)
    : m_workQueue(IPC::StreamConnectionWorkQueue::create("RemoteRenderingBackend work queue"_s))
    , m_streamConnection(WTF::move(streamConnection))
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_sharedResourceCache(gpuConnectionToWebProcess.sharedResourceCache())
    , m_renderingBackendIdentifier(identifier)
    , m_shapeDetectionObjectHeap(ShapeDetection::ObjectHeap::create())
{
    ASSERT(RunLoop::isMain());
    weakPtrFactory().prepareForUseOnlyOnNonMainThread();
}

RemoteRenderingBackend::~RemoteRenderingBackend() = default;

void RemoteRenderingBackend::startListeningForIPC()
{
    dispatch([protectedThis = Ref { *this }] {
        protectedThis->workQueueInitialize();
    });
}

void RemoteRenderingBackend::stopListeningForIPC()
{
    m_workQueue->stopAndWaitForCompletion([protectedThis = Ref { *this }] {
        protectedThis->workQueueUninitialize();
    });
}

std::optional<SharedPreferencesForWebProcess> RemoteRenderingBackend::sharedPreferencesForWebProcess() const
{
    return m_gpuConnectionToWebProcess->sharedPreferencesForWebProcess();
}

void RemoteRenderingBackend::workQueueInitialize()
{
    assertIsCurrent(workQueue());
    m_streamConnection->open(*this, m_workQueue.get());
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteRenderingBackend::messageReceiverName(), m_renderingBackendIdentifier.toUInt64());
    send(Messages::RemoteRenderingBackendProxy::DidInitialize(workQueue().wakeUpSemaphore(), m_streamConnection->clientWaitSemaphore()));
}

void RemoteRenderingBackend::workQueueUninitialize()
{
    assertIsCurrent(workQueue());
    m_remoteImageBuffers.clear();
    m_remoteImageBufferSets.clear();
    // Make sure we destroy the ResourceCache on the WorkQueue since it gets populated on the WorkQueue.
    m_remoteResourceCache.releaseAllResources();

    Ref streamConnection = m_streamConnection;
    streamConnection->stopReceivingMessages(Messages::RemoteRenderingBackend::messageReceiverName(), m_renderingBackendIdentifier.toUInt64());
    streamConnection->invalidate();
}

void RemoteRenderingBackend::didReceiveInvalidMessage(IPC::StreamServerConnection&, IPC::MessageName messageName, const Vector<uint32_t>&)
{
    RELEASE_LOG_FAULT_WITH_PAYLOAD(IPC, makeString("Received an invalid message '"_s, description(messageName), "' from WebContent process "_s, m_gpuConnectionToWebProcess->webProcessIdentifier().toUInt64(), ", requesting for it to be terminated."_s).utf8().data());
    callOnMainRunLoop([gpuConnectionToWebProcess = m_gpuConnectionToWebProcess] {
        gpuConnectionToWebProcess->terminateWebProcess();
    });
}

void RemoteRenderingBackend::dispatch(Function<void()>&& task)
{
    m_workQueue->dispatch(WTF::move(task));
}

void RemoteRenderingBackend::moveToSerializedBuffer(RenderingResourceIdentifier identifier, RemoteSerializedImageBufferIdentifier serializedIdentifier)
{
    assertIsCurrent(workQueue());
    // This transfers ownership of the RemoteImageBuffer contents to the transfer heap.
    RefPtr remoteImageBuffer = m_remoteImageBuffers.take(identifier).get();
    MESSAGE_CHECK(remoteImageBuffer, "Missing ImageBuffer");
    Ref imageBuffer = RemoteImageBuffer::sinkIntoImageBuffer(remoteImageBuffer.releaseNonNull());
    MESSAGE_CHECK(imageBuffer->hasOneRef(), "ImageBuffer in use");
    bool success = m_sharedResourceCache->addSerializedImageBuffer(serializedIdentifier, WTF::move(imageBuffer));
    MESSAGE_CHECK(success, "Duplicate SerializedImageBuffer");
}

static void adjustImageBufferCreationContext(RemoteSharedResourceCache& sharedResourceCache, ImageBufferCreationContext& creationContext)
{
#if HAVE(IOSURFACE)
    creationContext.surfacePool = sharedResourceCache.ioSurfacePool();
#endif
    creationContext.resourceOwner = sharedResourceCache.resourceOwner();
}

void RemoteRenderingBackend::moveToImageBuffer(RemoteSerializedImageBufferIdentifier identifier, RenderingResourceIdentifier imageBufferIdentifier, RemoteGraphicsContextIdentifier contextIdentifier)
{
    assertIsCurrent(workQueue());
    RefPtr imageBuffer = m_sharedResourceCache->takeSerializedImageBuffer(identifier);
    MESSAGE_CHECK(imageBuffer, "Missing SerializedImageBuffer");

    ImageBufferCreationContext creationContext;
    adjustImageBufferCreationContext(m_sharedResourceCache, creationContext);
    imageBuffer->transferToNewContext(creationContext);
    auto result = m_remoteImageBuffers.add(imageBufferIdentifier, RemoteImageBuffer::create(imageBuffer.releaseNonNull(), imageBufferIdentifier, contextIdentifier, *this));
    MESSAGE_CHECK(result.isNewEntry, "Duplicate ImageBuffer");
}

void RemoteRenderingBackend::createSnapshotRecorder(RemoteSnapshotRecorderIdentifier identifier, RemoteSnapshotIdentifier snapshotIdentifier)
{
    assertIsCurrent(workQueue());
    // FIXME: using global identifiers (snapshotIdentifier) is not secure. Do not follow this pattern.
    Ref snapshot = GPUProcess::singleton().getOrCreateSnapshot(snapshotIdentifier);
    auto result = m_remoteSnapshotRecorders.add(identifier, RemoteSnapshotRecorder::create(identifier, snapshot, *this));
    MESSAGE_CHECK(result.isNewEntry, "Recorder already created");
}

void RemoteRenderingBackend::sinkSnapshotRecorderIntoSnapshotFrame(RemoteSnapshotRecorderIdentifier identifier, FrameIdentifier frameIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    RefPtr recorder = m_remoteSnapshotRecorders.take(identifier).get();
    MESSAGE_CHECK(recorder, "Recorder sunk into snapshot before being cached");
    Ref snapshot = recorder->snapshot();
    // FIXME: using global identifiers (frameIdentifier) is not secure. Do not follow this pattern.
    bool success = snapshot->setFrame(frameIdentifier, recorder->takeDisplayList(), workQueue());
    MESSAGE_CHECK(success, "Frame already present");

    // Note:
    // Success completion handlers are used to ensure that getOrCreateSnapshot does not vivify already released snapshot identifier into a leaked object. Caller is expected to wait
    // until completion of *all* handlers, failing or not, before consuming the snapshot, otherwise leaks occur.
    completionHandler(true);
}

template<typename ImageBufferType>
static RefPtr<ImageBuffer> allocateImageBufferInternal(const FloatSize& logicalSize, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferFormat bufferFormat, ImageBufferCreationContext& creationContext)
{
    RefPtr<ImageBuffer> imageBuffer;

    switch (renderingMode) {
    case RenderingMode::Accelerated:
#if HAVE(IOSURFACE)
        if (isSmallLayerBacking({ logicalSize, resolutionScale, colorSpace, bufferFormat, purpose }))
            imageBuffer = ImageBuffer::create<ImageBufferShareableMappedIOSurfaceBitmapBackend, ImageBufferType>(logicalSize, resolutionScale, colorSpace, bufferFormat, purpose, creationContext);
        if (!imageBuffer)
            imageBuffer = ImageBuffer::create<ImageBufferShareableMappedIOSurfaceBackend, ImageBufferType>(logicalSize, resolutionScale, colorSpace, bufferFormat, purpose, creationContext);
#endif
        [[fallthrough]];

    case RenderingMode::Unaccelerated:
        if (!imageBuffer)
            imageBuffer = ImageBuffer::create<ImageBufferShareableBitmapBackend, ImageBufferType>(logicalSize, resolutionScale, colorSpace, bufferFormat, purpose, creationContext);
        break;

    case RenderingMode::PDFDocument:
#if USE(CG)
        imageBuffer = ImageBuffer::create<ImageBufferCGPDFDocumentBackend, ImageBufferType>(logicalSize, resolutionScale, colorSpace, bufferFormat, purpose, creationContext);
#endif
        break;

    case RenderingMode::DisplayList:
        if (auto backend = ImageBufferDisplayListBackend::create(logicalSize, resolutionScale, colorSpace, bufferFormat.pixelFormat, purpose, ControlFactory::create()))
            imageBuffer = ImageBuffer::create<ImageBufferDisplayListBackend, ImageBufferType>(logicalSize, creationContext, WTF::move(backend));
        break;
    }

    return imageBuffer;
}

static void adjustImageBufferRenderingMode(const RemoteSharedResourceCache& sharedResourceCache, RenderingPurpose purpose, RenderingMode& renderingMode)
{
    if (renderingMode == RenderingMode::Accelerated && sharedResourceCache.reachedAcceleratedImageBufferLimit(purpose))
        renderingMode = RenderingMode::Unaccelerated;
}

RefPtr<ImageBuffer> RemoteRenderingBackend::allocateImageBuffer(const FloatSize& logicalSize, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferFormat bufferFormat, ImageBufferCreationContext creationContext)
{
    assertIsCurrent(workQueue());
    if (purpose == RenderingPurpose::Canvas && m_sharedResourceCache->reachedImageBufferForCanvasLimit())
        return nullptr;

    adjustImageBufferCreationContext(m_sharedResourceCache, creationContext);
    adjustImageBufferRenderingMode(m_sharedResourceCache, purpose, renderingMode);

    RefPtr<ImageBuffer> imageBuffer;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    if (m_gpuConnectionToWebProcess->isDynamicContentScalingEnabled() && creationContext.dynamicContentScalingResourceCache)
        imageBuffer = allocateImageBufferInternal<DynamicContentScalingBifurcatedImageBuffer>(logicalSize, renderingMode, purpose, resolutionScale, colorSpace, bufferFormat, creationContext);
#endif

    if (!imageBuffer)
        imageBuffer = allocateImageBufferInternal<ImageBuffer>(logicalSize, renderingMode, purpose, resolutionScale, colorSpace, bufferFormat, creationContext);

    return imageBuffer;
}


void RemoteRenderingBackend::createImageBuffer(const FloatSize& logicalSize, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferFormat pixelFormat, RenderingResourceIdentifier identifier, RemoteGraphicsContextIdentifier contextIdentifier)
{
    assertIsCurrent(workQueue());
    RefPtr<ImageBuffer> imageBuffer = allocateImageBuffer(logicalSize, renderingMode, purpose, resolutionScale, colorSpace, pixelFormat, { });
    if (!imageBuffer) {
        RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend=%" PRIu64 "] RemoteRenderingBackend::createImageBuffer - failed to allocate image buffer %" PRIu64, m_renderingBackendIdentifier.toUInt64(), identifier.toUInt64());
        // On failure to create a remote image buffer we still create a null display list recorder.
        // Commands to draw to the failed image might have already be issued and we must process
        // them.
        imageBuffer = ImageBuffer::create<NullImageBufferBackend>({ 0, 0 }, 1, DestinationColorSpace::SRGB(), { PixelFormat::BGRA8 }, RenderingPurpose::Unspecified, { });
        RELEASE_ASSERT(imageBuffer);
    }
    auto result = m_remoteImageBuffers.add(identifier, RemoteImageBuffer::create(imageBuffer.releaseNonNull(), identifier, contextIdentifier, *this));
    MESSAGE_CHECK(result.isNewEntry, "Duplicate ImageBuffers");
}

void RemoteRenderingBackend::releaseImageBuffer(RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteImageBuffers.take(identifier).get();
    MESSAGE_CHECK(success, "Missing ImageBuffer");
}

void RemoteRenderingBackend::createImageBufferSet(ImageBufferSetIdentifier identifier, RemoteGraphicsContextIdentifier contextIdentifier)
{
    assertIsCurrent(workQueue());
    auto result = m_remoteImageBufferSets.add(identifier, RemoteImageBufferSet::create(identifier, contextIdentifier, *this));
    MESSAGE_CHECK(result.isNewEntry, "Duplicate ImageBufferSet");
}

void RemoteRenderingBackend::releaseImageBufferSet(ImageBufferSetIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteImageBufferSets.take(identifier).get();
    MESSAGE_CHECK(success, "Missing ImageBufferSet");
}

void RemoteRenderingBackend::destroyGetPixelBufferSharedMemory()
{
    m_getPixelBufferSharedMemory = nullptr;
}

void RemoteRenderingBackend::nativeImageBitmap(RenderingResourceIdentifier imageIdentifier, CompletionHandler<void(std::optional<ShareableBitmap::Handle>)>&& completionHandler)
{
    std::optional<ShareableBitmap::Handle> result;
    [&] {
        RefPtr image = m_remoteResourceCache.cachedNativeImage(imageIdentifier);
        MESSAGE_CHECK(image, "NativeImage not cached.");
        RefPtr<ShareableBitmap> bitmap;
#if USE(CG)
        bitmap = ShareableBitmap::createFromImagePixels(*image);
#endif
        if (!bitmap)
            bitmap = ShareableBitmap::createFromImageDraw(*image, DestinationColorSpace { image->colorSpace() });
        if (!bitmap)
            return;
        auto handle = bitmap->createHandle();
        if (!handle)
            return;
        if (sharedResourceCache().resourceOwner())
            handle->setOwnershipOfMemory(sharedResourceCache().resourceOwner(), WebCore::MemoryLedger::Graphics);
        auto platformImage = bitmap->createPlatformImage();
        if (!platformImage) {
            ASSERT_NOT_REACHED();
            return;
        }
        image->replacePlatformImage(WTF::move(platformImage));
        result = WTF::move(handle);
    }();
    ASSERT(result);
    completionHandler(WTF::move(result));
}

void RemoteRenderingBackend::cacheNativeImage(ShareableBitmap::Handle&& handle, RenderingResourceIdentifier imageIdentifier)
{
    ASSERT(!RunLoop::isMain());

    auto bitmap = ShareableBitmap::create(WTF::move(handle));
    if (!bitmap)
        return;

    auto image = NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes));
    if (!image)
        return;

    bool success = m_remoteResourceCache.cacheNativeImage(imageIdentifier, image.releaseNonNull());
    MESSAGE_CHECK(success, "NativeImage already cached.");
}

void RemoteRenderingBackend::cacheNativeImageFromSharedNativeImage(WebCore::RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    RefPtr image = m_sharedResourceCache->takeNativeImage(identifier);
    MESSAGE_CHECK(image, "Shared NativeImage not found.");
    bool success = m_remoteResourceCache.cacheNativeImage(identifier, image.releaseNonNull());
    MESSAGE_CHECK(success, "NativeImage already cached.");
}

void RemoteRenderingBackend::releaseNativeImage(RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseNativeImage(identifier);
    MESSAGE_CHECK(success, "NativeImage released before being cached.");
}

void RemoteRenderingBackend::cacheFont(const Font::Attributes& fontAttributes, FontPlatformDataAttributes platformData, std::optional<RenderingResourceIdentifier> fontCustomPlatformDataIdentifier)
{
    ASSERT(!RunLoop::isMain());

    RefPtr<FontCustomPlatformData> customPlatformData = nullptr;
    if (fontCustomPlatformDataIdentifier) {
        customPlatformData = m_remoteResourceCache.cachedFontCustomPlatformData(*fontCustomPlatformDataIdentifier);
        MESSAGE_CHECK(customPlatformData, "CacheFont without caching custom data");
    }

    FontPlatformData platform = FontPlatformData::create(platformData, customPlatformData.get());

    Ref<Font> font = Font::create(platform, fontAttributes.origin, fontAttributes.isInterstitial, fontAttributes.visibility, fontAttributes.isTextOrientationFallback, fontAttributes.renderingResourceIdentifier);

    m_remoteResourceCache.cacheFont(WTF::move(font));
}

void RemoteRenderingBackend::releaseFont(WebCore::RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseFont(identifier);
    MESSAGE_CHECK(success, "Font released before being cached.");
}

void RemoteRenderingBackend::cacheFontCustomPlatformData(WebCore::FontCustomPlatformSerializedData&& fontCustomPlatformSerializedData)
{
    ASSERT(!RunLoop::isMain());

    auto customPlatformData = FontCustomPlatformData::tryMakeFromSerializationData(WTF::move(fontCustomPlatformSerializedData), shouldUseLockdownFontParser());
    MESSAGE_CHECK(customPlatformData.has_value(), "cacheFontCustomPlatformData couldn't deserialize FontCustomPlatformData");

    m_remoteResourceCache.cacheFontCustomPlatformData(WTF::move(customPlatformData.value()));
}

void RemoteRenderingBackend::releaseFontCustomPlatformData(WebCore::RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseFontCustomPlatformData(identifier);
    MESSAGE_CHECK(success, "FontCustomPlatformData released before being cached.");
}

void RemoteRenderingBackend::cachePathImpl(Ref<WebCore::PathImpl>&& path, RemotePathImplIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.cachePathImpl(identifier, WTF::move(path));
    MESSAGE_CHECK(success, "Path already cached.");
}

void RemoteRenderingBackend::releasePathImpl(RemotePathImplIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releasePathImpl(identifier);
    MESSAGE_CHECK(success, "Path released before being cached.");
}

void RemoteRenderingBackend::cacheGradient(Ref<Gradient>&& gradient, RemoteGradientIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.cacheGradient(identifier, WTF::move(gradient));
    MESSAGE_CHECK(success, "Gradient already cached.");
}

void RemoteRenderingBackend::releaseGradient(RemoteGradientIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseGradient(identifier);
    MESSAGE_CHECK(success, "Gradient released before being cached.");
}


void RemoteRenderingBackend::cacheFilter(Ref<Filter>&& filter)
{
    ASSERT(!RunLoop::isMain());
    if (filter->hasValidRenderingResourceIdentifier())
        m_remoteResourceCache.cacheFilter(WTF::move(filter));
    else
        LOG_WITH_STREAM(DisplayLists, stream << "Received a Filter without a valid resource identifier");
}

void RemoteRenderingBackend::releaseFilter(RenderingResourceIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseFilter(identifier);
    MESSAGE_CHECK(success, "Filter released before being cached.");
}

void RemoteRenderingBackend::createDisplayListRecorder(RemoteDisplayListRecorderIdentifier identifier)
{
    assertIsCurrent(workQueue());
    auto result = m_remoteDisplayListRecorders.add(identifier, RemoteDisplayListRecorder::create(identifier, *this));
    MESSAGE_CHECK(result.isNewEntry, "Recorder already created");
}

void RemoteRenderingBackend::sinkDisplayListRecorderIntoDisplayList(RemoteDisplayListRecorderIdentifier identifier, RemoteDisplayListIdentifier displayListIdentifier)
{
    assertIsCurrent(workQueue());
    RefPtr recorder = m_remoteDisplayListRecorders.take(identifier).get();
    MESSAGE_CHECK(recorder, "Recorder sunk into display list before being cached");
    Ref displayList = recorder->takeDisplayList();
    bool success = m_remoteResourceCache.cacheDisplayList(displayListIdentifier, WTF::move(displayList));
    MESSAGE_CHECK(success, "Display list already created");
}

void RemoteRenderingBackend::releaseDisplayList(RemoteDisplayListIdentifier identifier)
{
    assertIsCurrent(workQueue());
    bool success = m_remoteResourceCache.releaseDisplayList(identifier);
    MESSAGE_CHECK(success, "Display list released before being cached");
}

void RemoteRenderingBackend::releaseMemory()
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.releaseMemory();
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
        MESSAGE_CHECK(remoteImageBufferSet, "BufferSet is being updated before being created");
        SwapBuffersDisplayRequirement displayRequirement = SwapBuffersDisplayRequirement::NeedsNormalDisplay;
        remoteImageBufferSet->ensureBufferForDisplay(swapBuffersInput[i], displayRequirement, false);

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
        MESSAGE_CHECK(remoteImageBufferSet, "BufferSet is being updated before being created");
        remoteImageBufferSet->ensureBufferForDisplay(swapBuffersInput[i], outputData[i], true);
    }

    completionHandler(WTF::move(outputData));

    // Defer preparing all the front buffers (which triggers pixel copy
    // operations) until after we've sent the completion handler (and any
    // buffer backend created messages) to unblock the WebProcess as soon
    // as possible.
    for (unsigned i = 0; i < swapBuffersInput.size(); ++i) {
        RefPtr<RemoteImageBufferSet> remoteImageBufferSet = m_remoteImageBufferSets.get(swapBuffersInput[i].remoteBufferSet);
        MESSAGE_CHECK(remoteImageBufferSet, "BufferSet is being updated before being created");

        if (outputData[i] != SwapBuffersDisplayRequirement::NeedsNoDisplay)
            remoteImageBufferSet->prepareBufferForDisplay(swapBuffersInput[i].dirtyRegion, swapBuffersInput[i].requiresClearedPixels);
    }
}
#endif

void RemoteRenderingBackend::markSurfacesVolatile(MarkSurfacesAsVolatileRequestIdentifier requestIdentifier, const Vector<std::pair<ImageBufferSetIdentifier, OptionSet<BufferInSetType>>>& identifiers, bool forcePurge)
{
    assertIsCurrent(workQueue());
    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "GPU Process: RemoteRenderingBackend::markSurfacesVolatile " << identifiers);

    Vector<std::pair<ImageBufferSetIdentifier, OptionSet<BufferInSetType>>> markedBufferSets;
    bool allSucceeded = true;

    for (auto identifier : identifiers) {
        RefPtr<RemoteImageBufferSet> remoteImageBufferSet = m_remoteImageBufferSets.get(identifier.first);

        MESSAGE_CHECK(remoteImageBufferSet, "BufferSet is being marked volatile before being created");

        OptionSet<BufferInSetType> volatileBuffers;
        if (!remoteImageBufferSet->makeBuffersVolatile(identifier.second, volatileBuffers, forcePurge))
            allSucceeded = false;

        if (!volatileBuffers.isEmpty())
            markedBufferSets.append(std::make_pair(identifier.first, volatileBuffers));
    }

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "GPU Process: markSurfacesVolatile - surfaces marked volatile " << markedBufferSets);
    send(Messages::RemoteRenderingBackendProxy::DidMarkLayersAsVolatile(requestIdentifier, WTF::move(markedBufferSets), allSucceeded));
}

void RemoteRenderingBackend::finalizeRenderingUpdate(RenderingUpdateID renderingUpdateID)
{
    send(Messages::RemoteRenderingBackendProxy::DidFinalizeRenderingUpdate(renderingUpdateID));
}

void RemoteRenderingBackend::createBarcodeDetector(ShapeDetectionIdentifier identifier, const WebCore::ShapeDetection::BarcodeDetectorOptions& barcodeDetectorOptions)
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    auto inner = WebCore::ShapeDetection::BarcodeDetectorImpl::create(barcodeDetectorOptions);
    auto remoteBarcodeDetector = RemoteBarcodeDetector::create(WTF::move(inner), *this, identifier);
    m_shapeDetectionObjectHeap->addObject(identifier, remoteBarcodeDetector);
    m_streamConnection->startReceivingMessages(remoteBarcodeDetector, Messages::RemoteBarcodeDetector::messageReceiverName(), identifier.toUInt64());
#else
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(barcodeDetectorOptions);
#endif
}

void RemoteRenderingBackend::releaseBarcodeDetector(ShapeDetectionIdentifier identifier)
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    m_streamConnection->stopReceivingMessages(Messages::RemoteBarcodeDetector::messageReceiverName(), identifier.toUInt64());
    m_shapeDetectionObjectHeap->removeObject(identifier);
#else
    UNUSED_PARAM(identifier);
#endif
}

void RemoteRenderingBackend::supportedBarcodeDetectorBarcodeFormats(CompletionHandler<void(Vector<WebCore::ShapeDetection::BarcodeFormat>&&)>&& completionHandler)
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    WebCore::ShapeDetection::BarcodeDetectorImpl::getSupportedFormats(WTF::move(completionHandler));
#else
    completionHandler({ });
#endif
}

void RemoteRenderingBackend::createFaceDetector(ShapeDetectionIdentifier identifier, const WebCore::ShapeDetection::FaceDetectorOptions& faceDetectorOptions)
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    auto inner = WebCore::ShapeDetection::FaceDetectorImpl::create(faceDetectorOptions);
    auto remoteFaceDetector = RemoteFaceDetector::create(WTF::move(inner), *this, identifier);
    m_shapeDetectionObjectHeap->addObject(identifier, remoteFaceDetector);
    m_streamConnection->startReceivingMessages(remoteFaceDetector, Messages::RemoteFaceDetector::messageReceiverName(), identifier.toUInt64());
#else
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(faceDetectorOptions);
#endif
}

void RemoteRenderingBackend::releaseFaceDetector(ShapeDetectionIdentifier identifier)
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    m_streamConnection->stopReceivingMessages(Messages::RemoteFaceDetector::messageReceiverName(), identifier.toUInt64());
    m_shapeDetectionObjectHeap->removeObject(identifier);
#else
    UNUSED_PARAM(identifier);
#endif
}

void RemoteRenderingBackend::createTextDetector(ShapeDetectionIdentifier identifier)
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    auto inner = WebCore::ShapeDetection::TextDetectorImpl::create();
    auto remoteTextDetector = RemoteTextDetector::create(WTF::move(inner), *this, identifier);
    m_shapeDetectionObjectHeap->addObject(identifier, remoteTextDetector);
    m_streamConnection->startReceivingMessages(remoteTextDetector, Messages::RemoteTextDetector::messageReceiverName(), identifier.toUInt64());
#else
    UNUSED_PARAM(identifier);
#endif
}

void RemoteRenderingBackend::releaseTextDetector(ShapeDetectionIdentifier identifier)
{
#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    m_streamConnection->stopReceivingMessages(Messages::RemoteTextDetector::messageReceiverName(), identifier.toUInt64());
    m_shapeDetectionObjectHeap->removeObject(identifier);
#else
    UNUSED_PARAM(identifier);
#endif
}

RefPtr<ImageBuffer> RemoteRenderingBackend::imageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    assertIsCurrent(workQueue());
    RefPtr<RemoteImageBuffer> remoteImageBuffer = m_remoteImageBuffers.get(renderingResourceIdentifier);
    if (!remoteImageBuffer.get())
        return nullptr;
    return remoteImageBuffer->imageBuffer();
}

#if PLATFORM(COCOA)
bool RemoteRenderingBackend::shouldUseLockdownFontParser() const
{
#if HAVE(CTFONTMANAGER_CREATEMEMORYSAFEFONTDESCRIPTORFROMDATA)
    return (m_gpuConnectionToWebProcess->isLockdownSafeFontParserEnabled() && m_gpuConnectionToWebProcess->isLockdownModeEnabled()) || (m_gpuConnectionToWebProcess->isForceLockdownSafeFontParserEnabled());
#else
    return false;
#endif
}
#elif USE(CAIRO) || USE(SKIA)
bool RemoteRenderingBackend::shouldUseLockdownFontParser() const
{
    return false;
}
#endif

void RemoteRenderingBackend::getImageBufferResourceLimitsForTesting(CompletionHandler<void(WebCore::ImageBufferResourceLimits)>&& callback)
{
    callback(m_sharedResourceCache->getResourceLimitsForTesting());
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
