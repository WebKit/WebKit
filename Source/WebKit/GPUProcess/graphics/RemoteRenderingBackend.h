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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "Connection.h"
#include "ImageBufferBackendHandle.h"
#include "MarkSurfacesAsVolatileRequestIdentifier.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteRenderingBackendCreationParameters.h"
#include "RemoteResourceCache.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#include "RenderingBackendIdentifier.h"
#include "RenderingUpdateID.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include "ScopedRenderingResourcesRequest.h"
#include "ShapeDetectionIdentifier.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/HashMap.h>

namespace WTF {
enum class Critical : bool;
enum class Synchronous : bool;
}

namespace WebCore {
class DestinationColorSpace;
class FloatSize;
class MediaPlayer;
class NativeImage;
enum class RenderingMode : bool;

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
struct BufferIdentifierSet;
struct PrepareBackingStoreBuffersInputData;
struct PrepareBackingStoreBuffersOutputData;
struct RemoteRenderingBackendCreationParameters;
enum class SwapBuffersDisplayRequirement : uint8_t;

namespace ShapeDetection {
class ObjectHeap;
}

class RemoteRenderingBackend : private IPC::MessageSender, public IPC::StreamMessageReceiver {
public:
    static Ref<RemoteRenderingBackend> create(GPUConnectionToWebProcess&, RemoteRenderingBackendCreationParameters&&, Ref<IPC::StreamServerConnection>&&);
    virtual ~RemoteRenderingBackend();
    void stopListeningForIPC();

    RemoteResourceCache& remoteResourceCache() { return m_remoteResourceCache; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void didCreateImageBuffer(Ref<WebCore::ImageBuffer>);

    // Runs Function in RemoteRenderingBackend task queue.
    void dispatch(Function<void()>&&);

    IPC::StreamServerConnection& streamConnection() const { return m_streamConnection.get(); }

    void lowMemoryHandler(WTF::Critical, WTF::Synchronous);

#if HAVE(IOSURFACE)
    WebCore::IOSurfacePool& ioSurfacePool() const { return m_ioSurfacePool; }
#endif

    const WebCore::ProcessIdentity& resourceOwner() const { return m_resourceOwner; }

    GPUConnectionToWebProcess& gpuConnectionToWebProcess() { return m_gpuConnectionToWebProcess.get(); }

    void setSharedMemoryForGetPixelBuffer(RefPtr<SharedMemory> memory) { m_getPixelBufferSharedMemory = WTFMove(memory); }
    RefPtr<SharedMemory> sharedMemoryForGetPixelBuffer() const { return m_getPixelBufferSharedMemory; }

    IPC::StreamConnectionWorkQueue& workQueue() const { return m_workQueue; }

    RefPtr<WebCore::ImageBuffer> imageBuffer(WebCore::RenderingResourceIdentifier);
    RefPtr<WebCore::ImageBuffer> takeImageBuffer(WebCore::RenderingResourceIdentifier);

    void terminateWebProcess(ASCIILiteral message);
private:
    RemoteRenderingBackend(GPUConnectionToWebProcess&, RemoteRenderingBackendCreationParameters&&, Ref<IPC::StreamServerConnection>&&);
    void startListeningForIPC();
    void workQueueInitialize();
    void workQueueUninitialize();

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // Messages to be received.
    void createImageBuffer(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::PixelFormat, WebCore::RenderingResourceIdentifier);
    void releaseImageBuffer(WebCore::RenderingResourceIdentifier);
    void moveToSerializedBuffer(WebCore::RenderingResourceIdentifier);
    void moveToImageBuffer(WebCore::RenderingResourceIdentifier);
    void destroyGetPixelBufferSharedMemory();
    void cacheNativeImage(ShareableBitmap::Handle&&, WebCore::RenderingResourceIdentifier);
    void cacheDecomposedGlyphs(Ref<WebCore::DecomposedGlyphs>&&);
    void cacheGradient(Ref<WebCore::Gradient>&&);
    void cacheFilter(Ref<WebCore::Filter>&&);
    void cacheFont(const WebCore::Font::Attributes&, WebCore::FontPlatformData::Attributes, std::optional<WebCore::RenderingResourceIdentifier>);
    void cacheFontCustomPlatformData(Ref<WebCore::FontCustomPlatformData>&&);
    void releaseAllDrawingResources();
    void releaseAllImageResources();
    void releaseRenderingResource(WebCore::RenderingResourceIdentifier);
    void finalizeRenderingUpdate(RenderingUpdateID);
    void markSurfacesVolatile(MarkSurfacesAsVolatileRequestIdentifier, const Vector<WebCore::RenderingResourceIdentifier>&);
#if PLATFORM(COCOA)
    void prepareBuffersForDisplay(Vector<PrepareBackingStoreBuffersInputData> swapBuffersInput, CompletionHandler<void(Vector<PrepareBackingStoreBuffersOutputData>&&)>&&);
#endif
    
#if PLATFORM(COCOA)
    void prepareLayerBuffersForDisplay(const PrepareBackingStoreBuffersInputData&, PrepareBackingStoreBuffersOutputData&);
#endif

    void createRemoteBarcodeDetector(ShapeDetectionIdentifier, const WebCore::ShapeDetection::BarcodeDetectorOptions&);
    void releaseRemoteBarcodeDetector(ShapeDetectionIdentifier);
    void getRemoteBarcodeDetectorSupportedFormats(CompletionHandler<void(Vector<WebCore::ShapeDetection::BarcodeFormat>&&)>&&);
    void createRemoteFaceDetector(ShapeDetectionIdentifier, const WebCore::ShapeDetection::FaceDetectorOptions&);
    void releaseRemoteFaceDetector(ShapeDetectionIdentifier);
    void createRemoteTextDetector(ShapeDetectionIdentifier);
    void releaseRemoteTextDetector(ShapeDetectionIdentifier);

    Ref<IPC::StreamConnectionWorkQueue> m_workQueue;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    RemoteResourceCache m_remoteResourceCache;
    Ref<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    WebCore::ProcessIdentity m_resourceOwner;
    RenderingBackendIdentifier m_renderingBackendIdentifier;
    RefPtr<SharedMemory> m_getPixelBufferSharedMemory;
#if HAVE(IOSURFACE)
    Ref<WebCore::IOSurfacePool> m_ioSurfacePool;
#endif

    HashMap<WebCore::RenderingResourceIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteDisplayListRecorder>> m_remoteDisplayLists WTF_GUARDED_BY_CAPABILITY(workQueue());
    HashMap<WebCore::RenderingResourceIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteImageBuffer>> m_remoteImageBuffers WTF_GUARDED_BY_CAPABILITY(workQueue());
    Ref<ShapeDetection::ObjectHeap> m_shapeDetectionObjectHeap;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
