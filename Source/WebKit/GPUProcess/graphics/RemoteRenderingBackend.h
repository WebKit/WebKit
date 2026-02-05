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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "BufferIdentifierSet.h"
#include "Connection.h"
#include "IPCEvent.h"
#include "ImageBufferBackendHandle.h"
#include "ImageBufferSetIdentifier.h"
#include "MarkSurfacesAsVolatileRequestIdentifier.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteDisplayListIdentifier.h"
#include "RemoteDisplayListRecorderIdentifier.h"
#include "RemoteGradientIdentifier.h"
#include "RemoteGraphicsContextIdentifier.h"
#include "RemotePathImplIdentifier.h"
#include "RemoteRenderingBackendIdentifier.h"
#include "RemoteResourceCache.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#include "RemoteSharedResourceCache.h"
#include "RemoteSnapshotIdentifier.h"
#include "RemoteSnapshotRecorderIdentifier.h"
#include "RenderingUpdateID.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include "ScopedRenderingResourcesRequest.h"
#include "ShapeDetectionIdentifier.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include <WebCore/Font.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PixelFormat.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WTF {
enum class Critical : bool;
enum class Synchronous : bool;
}

namespace WebCore {
class DestinationColorSpace;
class Filter;
class FloatSize;
class Gradient;
class MediaPlayer;
class NativeImage;
enum class RenderingMode : uint8_t;
struct FontCustomPlatformSerializedData;

namespace ShapeDetection {
struct BarcodeDetectorOptions;
enum class BarcodeFormat : uint8_t;
struct FaceDetectorOptions;
}

}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteDisplayListRecorder;
class RemoteImageBuffer;
class RemoteImageBufferSet;
class RemoteSnapshotRecorder;
struct BufferIdentifierSet;
struct ImageBufferSetPrepareBufferForDisplayInputData;
struct ImageBufferSetPrepareBufferForDisplayOutputData;
struct SharedPreferencesForWebProcess;
enum class SwapBuffersDisplayRequirement : uint8_t;

namespace ShapeDetection {
class ObjectHeap;
}

class RemoteRenderingBackend : public CanMakeWeakPtr<RemoteRenderingBackend>, public IPC::StreamServerConnection::Client {
    WTF_MAKE_TZONE_ALLOCATED(RemoteRenderingBackend);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteRenderingBackend);

public:
    static Ref<RemoteRenderingBackend> create(GPUConnectionToWebProcess&, RemoteRenderingBackendIdentifier, Ref<IPC::StreamServerConnection>&&);
    virtual ~RemoteRenderingBackend();
    void stopListeningForIPC();

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

    RemoteResourceCache& remoteResourceCache() { return m_remoteResourceCache; }
    RemoteSharedResourceCache& sharedResourceCache() { return m_sharedResourceCache; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    // Runs Function in RemoteRenderingBackend task queue.
    void dispatch(Function<void()>&&);

    IPC::StreamServerConnection& streamConnection() const { return m_streamConnection.get(); }

    GPUConnectionToWebProcess& gpuConnectionToWebProcess() { return m_gpuConnectionToWebProcess.get(); }

    void setSharedMemoryForGetPixelBuffer(RefPtr<WebCore::SharedMemory> memory) { m_getPixelBufferSharedMemory = WTF::move(memory); }
    RefPtr<WebCore::SharedMemory> sharedMemoryForGetPixelBuffer() const { return m_getPixelBufferSharedMemory; }

    IPC::StreamConnectionWorkQueue& workQueue() const { return m_workQueue; }

    RefPtr<WebCore::ImageBuffer> imageBuffer(WebCore::RenderingResourceIdentifier);

    RefPtr<WebCore::ImageBuffer> allocateImageBuffer(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::ImageBufferFormat, WebCore::ImageBufferCreationContext);

    RemoteRenderingBackendIdentifier identifier() { return m_renderingBackendIdentifier; }
private:
    friend class RemoteImageBufferSet;
    RemoteRenderingBackend(GPUConnectionToWebProcess&, RemoteRenderingBackendIdentifier, Ref<IPC::StreamServerConnection>&&);
    void startListeningForIPC();
    void workQueueInitialize();
    void workQueueUninitialize();
    template<typename T>
    IPC::Error send(T&& message) const
    {
        return m_streamConnection->send(std::forward<T>(message), m_renderingBackendIdentifier);
    }

    // IPC::StreamServerConnection::Client overrides.
    void didReceiveInvalidMessage(IPC::StreamServerConnection&, IPC::MessageName, const Vector<uint32_t>&) final;

    // Messages to be received.
    void createImageBuffer(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::ImageBufferFormat, WebCore::RenderingResourceIdentifier, RemoteGraphicsContextIdentifier);
    void releaseImageBuffer(WebCore::RenderingResourceIdentifier);
    void moveToSerializedBuffer(WebCore::RenderingResourceIdentifier, RemoteSerializedImageBufferIdentifier);
    void moveToImageBuffer(RemoteSerializedImageBufferIdentifier, WebCore::RenderingResourceIdentifier, RemoteGraphicsContextIdentifier);
    void createDisplayListRecorder(RemoteDisplayListRecorderIdentifier);
    void sinkDisplayListRecorderIntoDisplayList(RemoteDisplayListRecorderIdentifier, RemoteDisplayListIdentifier);
    void releaseDisplayList(RemoteDisplayListIdentifier);
    void destroyGetPixelBufferSharedMemory();
    void nativeImageBitmap(WebCore::RenderingResourceIdentifier imageIdentifier, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>)>&&);
    void cacheNativeImage(WebCore::ShareableBitmap::Handle&&, WebCore::RenderingResourceIdentifier);
    void cacheNativeImageFromSharedNativeImage(WebCore::RenderingResourceIdentifier);
    void releaseNativeImage(WebCore::RenderingResourceIdentifier);
    void cachePathImpl(Ref<WebCore::PathImpl>&&, RemotePathImplIdentifier);
    void releasePathImpl(RemotePathImplIdentifier);
    void cacheGradient(Ref<WebCore::Gradient>&&, RemoteGradientIdentifier);
    void releaseGradient(RemoteGradientIdentifier);
    void cacheFilter(Ref<WebCore::Filter>&&);
    void releaseFilter(WebCore::RenderingResourceIdentifier);
    void cacheFont(const WebCore::Font::Attributes&, WebCore::FontPlatformDataAttributes, std::optional<WebCore::RenderingResourceIdentifier>);
    void releaseFont(WebCore::RenderingResourceIdentifier);
    void cacheFontCustomPlatformData(WebCore::FontCustomPlatformSerializedData&&);
    void releaseFontCustomPlatformData(WebCore::RenderingResourceIdentifier);
    void releaseMemory();
    void finalizeRenderingUpdate(RenderingUpdateID);
    void markSurfacesVolatile(MarkSurfacesAsVolatileRequestIdentifier, const Vector<std::pair<ImageBufferSetIdentifier, OptionSet<BufferInSetType>>>&, bool forcePurge);
    void createImageBufferSet(WebKit::ImageBufferSetIdentifier, RemoteGraphicsContextIdentifier);
    void releaseImageBufferSet(WebKit::ImageBufferSetIdentifier);

#if USE(GRAPHICS_LAYER_WC)
    void flush(IPC::Semaphore&&);
#endif

#if PLATFORM(COCOA)
    void prepareImageBufferSetsForDisplay(Vector<ImageBufferSetPrepareBufferForDisplayInputData> swapBuffersInput);
    void prepareImageBufferSetsForDisplaySync(Vector<ImageBufferSetPrepareBufferForDisplayInputData> swapBuffersInput, CompletionHandler<void(Vector<SwapBuffersDisplayRequirement>&&)>&&);
#endif

    void createSnapshotRecorder(RemoteSnapshotRecorderIdentifier, RemoteSnapshotIdentifier);
    void sinkSnapshotRecorderIntoSnapshotFrame(RemoteSnapshotRecorderIdentifier, WebCore::FrameIdentifier, CompletionHandler<void(bool)>&&);

    void createBarcodeDetector(ShapeDetectionIdentifier, const WebCore::ShapeDetection::BarcodeDetectorOptions&);
    void releaseBarcodeDetector(ShapeDetectionIdentifier);
    void supportedBarcodeDetectorBarcodeFormats(CompletionHandler<void(Vector<WebCore::ShapeDetection::BarcodeFormat>&&)>&&);
    void createFaceDetector(ShapeDetectionIdentifier, const WebCore::ShapeDetection::FaceDetectorOptions&);
    void releaseFaceDetector(ShapeDetectionIdentifier);
    void createTextDetector(ShapeDetectionIdentifier);
    void releaseTextDetector(ShapeDetectionIdentifier);

    bool shouldUseLockdownFontParser() const;

    void getImageBufferResourceLimitsForTesting(CompletionHandler<void(WebCore::ImageBufferResourceLimits)>&&);

    const Ref<IPC::StreamConnectionWorkQueue> m_workQueue;
    const Ref<IPC::StreamServerConnection> m_streamConnection;
    const Ref<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    const Ref<RemoteSharedResourceCache> m_sharedResourceCache;
    RemoteResourceCache m_remoteResourceCache;
    WebCore::ProcessIdentity m_resourceOwner;
    RemoteRenderingBackendIdentifier m_renderingBackendIdentifier;
    RefPtr<WebCore::SharedMemory> m_getPixelBufferSharedMemory;

    HashMap<WebCore::RenderingResourceIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteImageBuffer>> m_remoteImageBuffers WTF_GUARDED_BY_CAPABILITY(workQueue());

    HashMap<ImageBufferSetIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteImageBufferSet>> m_remoteImageBufferSets WTF_GUARDED_BY_CAPABILITY(workQueue());
    const Ref<ShapeDetection::ObjectHeap> m_shapeDetectionObjectHeap;
    HashMap<RemoteDisplayListRecorderIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteDisplayListRecorder>> m_remoteDisplayListRecorders WTF_GUARDED_BY_CAPABILITY(workQueue());
    HashMap<RemoteSnapshotRecorderIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteSnapshotRecorder>> m_remoteSnapshotRecorders WTF_GUARDED_BY_CAPABILITY(workQueue());
};

bool isSmallLayerBacking(const WebCore::ImageBufferParameters&);

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
