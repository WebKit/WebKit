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

#include "BufferIdentifierSet.h"
#include "Connection.h"
#include "IPCEvent.h"
#include "ImageBufferBackendHandle.h"
#include "MarkSurfacesAsVolatileRequestIdentifier.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteImageBufferSetIdentifier.h"
#include "RemoteResourceCache.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#include "RemoteSharedResourceCache.h"
#include "RenderingBackendIdentifier.h"
#include "RenderingUpdateID.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include "ScopedRenderingResourcesRequest.h"
#include "ShapeDetectionIdentifier.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include <WebCore/ImageBufferPixelFormat.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>

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
class RemoteImageBufferSet;
struct BufferIdentifierSet;
struct ImageBufferSetPrepareBufferForDisplayInputData;
struct ImageBufferSetPrepareBufferForDisplayOutputData;
struct SharedPreferencesForWebProcess;
enum class SwapBuffersDisplayRequirement : uint8_t;

namespace ShapeDetection {
class ObjectHeap;
}

class RemoteRenderingBackend : private IPC::MessageSender, public IPC::StreamMessageReceiver, public CanMakeWeakPtr<RemoteRenderingBackend> {
public:
    static Ref<RemoteRenderingBackend> create(GPUConnectionToWebProcess&, RenderingBackendIdentifier, Ref<IPC::StreamServerConnection>&&);
    virtual ~RemoteRenderingBackend();
    void stopListeningForIPC();

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

    RemoteResourceCache& remoteResourceCache() { return m_remoteResourceCache; }
    RemoteSharedResourceCache& sharedResourceCache() { return m_sharedResourceCache; }
    Ref<RemoteSharedResourceCache> protectedSharedResourceCache() { return m_sharedResourceCache; }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    // Runs Function in RemoteRenderingBackend task queue.
    void dispatch(Function<void()>&&);

    IPC::StreamServerConnection& streamConnection() const { return m_streamConnection.get(); }

    GPUConnectionToWebProcess& gpuConnectionToWebProcess() { return m_gpuConnectionToWebProcess.get(); }

    void setSharedMemoryForGetPixelBuffer(RefPtr<WebCore::SharedMemory> memory) { m_getPixelBufferSharedMemory = WTFMove(memory); }
    RefPtr<WebCore::SharedMemory> sharedMemoryForGetPixelBuffer() const { return m_getPixelBufferSharedMemory; }

    IPC::StreamConnectionWorkQueue& workQueue() const { return m_workQueue; }
    Ref<IPC::StreamConnectionWorkQueue> protectedWorkQueue() const { return m_workQueue; }

    RefPtr<WebCore::ImageBuffer> imageBuffer(WebCore::RenderingResourceIdentifier);
    RefPtr<WebCore::ImageBuffer> takeImageBuffer(WebCore::RenderingResourceIdentifier);

    RefPtr<WebCore::ImageBuffer> allocateImageBuffer(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::ImageBufferPixelFormat, WebCore::ImageBufferCreationContext, WebCore::RenderingResourceIdentifier);

    void terminateWebProcess(ASCIILiteral message);

    RenderingBackendIdentifier identifier() { return m_renderingBackendIdentifier; }
private:
    friend class RemoteImageBufferSet;
    RemoteRenderingBackend(GPUConnectionToWebProcess&, RenderingBackendIdentifier, Ref<IPC::StreamServerConnection>&&);
    void startListeningForIPC();
    void workQueueInitialize();
    void workQueueUninitialize();

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    // Messages to be received.
    void createImageBuffer(const WebCore::FloatSize& logicalSize, WebCore::RenderingMode, WebCore::RenderingPurpose, float resolutionScale, const WebCore::DestinationColorSpace&, WebCore::ImageBufferPixelFormat, WebCore::RenderingResourceIdentifier);
    void releaseImageBuffer(WebCore::RenderingResourceIdentifier);
    void moveToSerializedBuffer(WebCore::RenderingResourceIdentifier);
    void moveToImageBuffer(WebCore::RenderingResourceIdentifier);
    void destroyGetPixelBufferSharedMemory();
    void cacheNativeImage(WebCore::ShareableBitmap::Handle&&, WebCore::RenderingResourceIdentifier);
    void cacheDecomposedGlyphs(Ref<WebCore::DecomposedGlyphs>&&);
    void cacheGradient(Ref<WebCore::Gradient>&&);
    void cacheFilter(Ref<WebCore::Filter>&&);
    void cacheFont(const WebCore::Font::Attributes&, WebCore::FontPlatformDataAttributes, std::optional<WebCore::RenderingResourceIdentifier>);
#if PLATFORM(COCOA)
    void cacheFontCustomPlatformData(WebCore::FontCustomPlatformSerializedData&&);
#else
    void cacheFontCustomPlatformData(Ref<WebCore::FontCustomPlatformData>&&);
#endif
    void releaseAllDrawingResources();
    void releaseAllImageResources();
    void releaseRenderingResource(WebCore::RenderingResourceIdentifier);
    void finalizeRenderingUpdate(RenderingUpdateID);
    void markSurfacesVolatile(MarkSurfacesAsVolatileRequestIdentifier, const Vector<std::pair<RemoteImageBufferSetIdentifier, OptionSet<BufferInSetType>>>&, bool forcePurge);
    void createRemoteImageBufferSet(WebKit::RemoteImageBufferSetIdentifier, WebCore::RenderingResourceIdentifier displayListIdentifier);
    void releaseRemoteImageBufferSet(WebKit::RemoteImageBufferSetIdentifier);

#if USE(GRAPHICS_LAYER_WC)
    void flush(IPC::Semaphore&&);
#endif

#if PLATFORM(COCOA)
    void prepareImageBufferSetsForDisplay(Vector<ImageBufferSetPrepareBufferForDisplayInputData> swapBuffersInput);
    void prepareImageBufferSetsForDisplaySync(Vector<ImageBufferSetPrepareBufferForDisplayInputData> swapBuffersInput, CompletionHandler<void(Vector<SwapBuffersDisplayRequirement>&&)>&&);

#endif

    void createRemoteBarcodeDetector(ShapeDetectionIdentifier, const WebCore::ShapeDetection::BarcodeDetectorOptions&);
    void releaseRemoteBarcodeDetector(ShapeDetectionIdentifier);
    void getRemoteBarcodeDetectorSupportedFormats(CompletionHandler<void(Vector<WebCore::ShapeDetection::BarcodeFormat>&&)>&&);
    void createRemoteFaceDetector(ShapeDetectionIdentifier, const WebCore::ShapeDetection::FaceDetectorOptions&);
    void releaseRemoteFaceDetector(ShapeDetectionIdentifier);
    void createRemoteTextDetector(ShapeDetectionIdentifier);
    void releaseRemoteTextDetector(ShapeDetectionIdentifier);

    void didFailCreateImageBuffer(WebCore::RenderingResourceIdentifier) WTF_REQUIRES_CAPABILITY(workQueue());
    void didCreateImageBuffer(Ref<WebCore::ImageBuffer>) WTF_REQUIRES_CAPABILITY(workQueue());

    void createDisplayListRecorder(RefPtr<WebCore::ImageBuffer>, WebCore::RenderingResourceIdentifier);
    void releaseDisplayListRecorder(WebCore::RenderingResourceIdentifier);

    Ref<ShapeDetection::ObjectHeap> protectedShapeDetectionObjectHeap() const;

#if PLATFORM(COCOA)
    bool shouldUseLockdownFontParser() const;
#endif

    void getImageBufferResourceLimitsForTesting(CompletionHandler<void(WebCore::ImageBufferResourceLimits)>&&);

    Ref<IPC::StreamConnectionWorkQueue> m_workQueue;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    Ref<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    Ref<RemoteSharedResourceCache> m_sharedResourceCache;
    RemoteResourceCache m_remoteResourceCache;
    WebCore::ProcessIdentity m_resourceOwner;
    RenderingBackendIdentifier m_renderingBackendIdentifier;
    RefPtr<WebCore::SharedMemory> m_getPixelBufferSharedMemory;

    HashMap<WebCore::RenderingResourceIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteDisplayListRecorder>> m_remoteDisplayLists WTF_GUARDED_BY_CAPABILITY(workQueue());
    HashMap<WebCore::RenderingResourceIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteImageBuffer>> m_remoteImageBuffers WTF_GUARDED_BY_CAPABILITY(workQueue());
    HashMap<WebKit::RemoteImageBufferSetIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteImageBufferSet>> m_remoteImageBufferSets WTF_GUARDED_BY_CAPABILITY(workQueue());
    Ref<ShapeDetection::ObjectHeap> m_shapeDetectionObjectHeap;
};

bool isSmallLayerBacking(const WebCore::ImageBufferParameters&);

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
