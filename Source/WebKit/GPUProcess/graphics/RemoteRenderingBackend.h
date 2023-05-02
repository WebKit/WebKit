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
#include "DisplayListRecorderFlushIdentifier.h"
#include "ImageBufferBackendHandle.h"
#include "MarkSurfacesAsVolatileRequestIdentifier.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "QualifiedRenderingResourceIdentifier.h"
#include "RemoteGPU.h"
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
#include "WebGPUIdentifier.h"
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/ProcessIdentity.h>
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
    static Ref<RemoteRenderingBackend> create(GPUConnectionToWebProcess&, RemoteRenderingBackendCreationParameters&&, IPC::StreamServerConnection::Handle&&);
    virtual ~RemoteRenderingBackend();
    void stopListeningForIPC();

    RemoteResourceCache& remoteResourceCache() { return m_remoteResourceCache; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    // Messages to be sent.
    void didCreateImageBufferBackend(ImageBufferBackendHandle, QualifiedRenderingResourceIdentifier, RemoteDisplayListRecorder&);

    // Runs Function in RemoteRenderingBackend task queue.
    void dispatch(Function<void()>&&);

    IPC::StreamServerConnection& streamConnection() const { return m_streamConnection.get(); }
#if ENABLE(VIDEO)
    void performWithMediaPlayerOnMainThread(WebCore::MediaPlayerIdentifier, Function<void(WebCore::MediaPlayer&)>&&);
#endif

    void lowMemoryHandler(WTF::Critical, WTF::Synchronous);

#if HAVE(IOSURFACE)
    WebCore::IOSurfacePool& ioSurfacePool() const { return m_ioSurfacePool; }
#endif

    const WebCore::ProcessIdentity& resourceOwner() const { return m_resourceOwner; }

    GPUConnectionToWebProcess& gpuConnectionToWebProcess() { return m_gpuConnectionToWebProcess.get(); }

private:
    RemoteRenderingBackend(GPUConnectionToWebProcess&, RemoteRenderingBackendCreationParameters&&, IPC::StreamServerConnection::Handle&&);
    void startListeningForIPC();

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // Messages to be received.
    void createImageBuffer(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::PixelFormat, WebCore::RenderingResourceIdentifier);
    void moveToSerializedBuffer(WebCore::RenderingResourceIdentifier, RemoteSerializedImageBufferWriteReference&&);
    void moveToImageBuffer(RemoteSerializedImageBufferWriteReference&&, WebCore::RenderingResourceIdentifier);
    void getPixelBufferForImageBuffer(WebCore::RenderingResourceIdentifier, WebCore::PixelBufferFormat&&, WebCore::IntRect&& srcRect, CompletionHandler<void()>&&);
    void getPixelBufferForImageBufferWithNewMemory(WebCore::RenderingResourceIdentifier, SharedMemory::Handle&&, WebCore::PixelBufferFormat&& destinationFormat, WebCore::IntRect&& srcRect, CompletionHandler<void()>&&);
    void destroyGetPixelBufferSharedMemory();
    void putPixelBufferForImageBuffer(WebCore::RenderingResourceIdentifier, Ref<WebCore::PixelBuffer>&&, WebCore::IntRect&& srcRect, WebCore::IntPoint&& destPoint, WebCore::AlphaPremultiplication destFormat);
    void getShareableBitmapForImageBuffer(WebCore::RenderingResourceIdentifier, WebCore::PreserveResolution, CompletionHandler<void(ShareableBitmap::Handle&&)>&&);
    void getFilteredImageForImageBuffer(WebCore::RenderingResourceIdentifier, Ref<WebCore::Filter>, CompletionHandler<void(ShareableBitmap::Handle&&)>&&);
    void cacheNativeImage(ShareableBitmap::Handle&&, WebCore::RenderingResourceIdentifier);
    void cacheDecomposedGlyphs(Ref<WebCore::DecomposedGlyphs>&&);
    void cacheGradient(Ref<WebCore::Gradient>&&, WebCore::RenderingResourceIdentifier);
    void cacheFont(const WebCore::Font::Attributes&, WebCore::FontPlatformData::Attributes, std::optional<WebCore::RenderingResourceIdentifier>);
    void cacheFontCustomPlatformData(Ref<WebCore::FontCustomPlatformData>&&);
    void releaseAllResources();
    void releaseAllImageResources();
    void releaseRenderingResource(WebCore::RenderingResourceIdentifier);
    void finalizeRenderingUpdate(RenderingUpdateID);
    void markSurfacesVolatile(MarkSurfacesAsVolatileRequestIdentifier, const Vector<WebCore::RenderingResourceIdentifier>&);
#if PLATFORM(COCOA)
    void prepareBuffersForDisplay(Vector<PrepareBackingStoreBuffersInputData> swapBuffersInput, CompletionHandler<void(const Vector<PrepareBackingStoreBuffersOutputData>&)>&&);
#endif
    
    // Received messages translated to use QualifiedRenderingResourceIdentifier.
    void createImageBufferWithQualifiedIdentifier(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::PixelFormat, QualifiedRenderingResourceIdentifier);
    void getShareableBitmapForImageBufferWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier, WebCore::PreserveResolution, CompletionHandler<void(ShareableBitmap::Handle&&)>&&);
    void cacheNativeImageWithQualifiedIdentifier(ShareableBitmap::Handle&&, QualifiedRenderingResourceIdentifier);
    void cacheDecomposedGlyphsWithQualifiedIdentifier(Ref<WebCore::DecomposedGlyphs>&&, QualifiedRenderingResourceIdentifier);
    void cacheGradientWithQualifiedIdentifier(Ref<WebCore::Gradient>&&, QualifiedRenderingResourceIdentifier);
    void releaseRenderingResourceWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier);
    void cacheFontWithQualifiedIdentifier(Ref<WebCore::Font>&&, QualifiedRenderingResourceIdentifier);
    void cacheFontCustomPlatformDataWithQualifiedIdentifier(Ref<WebCore::FontCustomPlatformData>&&, QualifiedRenderingResourceIdentifier);

#if PLATFORM(COCOA)
    void prepareLayerBuffersForDisplay(const PrepareBackingStoreBuffersInputData&, PrepareBackingStoreBuffersOutputData&);
#endif

    void createRemoteGPU(WebGPUIdentifier, IPC::StreamServerConnection::Handle&&);
    void releaseRemoteGPU(WebGPUIdentifier);

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

    Lock m_remoteDisplayListsLock;
    bool m_canRegisterRemoteDisplayLists WTF_GUARDED_BY_LOCK(m_remoteDisplayListsLock) { false };
    HashMap<QualifiedRenderingResourceIdentifier, Ref<RemoteDisplayListRecorder>> m_remoteDisplayLists WTF_GUARDED_BY_CAPABILITY(m_remoteDisplayListsLock);

    using RemoteGPUMap = HashMap<WebGPUIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteGPU>>;
    RemoteGPUMap m_remoteGPUMap;

    Ref<ShapeDetection::ObjectHeap> m_shapeDetectionObjectHeap;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
