/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "LayerHostingContext.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteVideoFrameIdentifier.h"
#include "SampleBufferDisplayLayerIdentifier.h"
#include "SharedVideoFrame.h"
#include <WebCore/SampleBufferDisplayLayer.h>
#include <wtf/MediaTime.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadAssertions.h>
#include <wtf/WeakRef.h>

namespace WebCore {
class ImageTransferSessionVT;
class LocalSampleBufferDisplayLayer;
enum class VideoFrameRotation : uint16_t;
};

namespace WebKit {
class GPUConnectionToWebProcess;

class RemoteSampleBufferDisplayLayer : public RefCounted<RemoteSampleBufferDisplayLayer>, public WebCore::SampleBufferDisplayLayerClient, public IPC::MessageReceiver, private IPC::MessageSender {
    WTF_MAKE_TZONE_ALLOCATED(RemoteSampleBufferDisplayLayer);
public:
    static RefPtr<RemoteSampleBufferDisplayLayer> create(GPUConnectionToWebProcess&, SampleBufferDisplayLayerIdentifier, Ref<IPC::Connection>&&);
    ~RemoteSampleBufferDisplayLayer();

    using WebCore::SampleBufferDisplayLayerClient::weakPtrFactory;
    using WebCore::SampleBufferDisplayLayerClient::WeakValueType;
    using WebCore::SampleBufferDisplayLayerClient::WeakPtrImplType;

    using LayerInitializationCallback = CompletionHandler<void(std::optional<LayerHostingContextID>)>;
    void initialize(bool hideRootLayer, WebCore::IntSize, bool shouldMaintainAspectRatio, bool canShowWhileLocked, LayerInitializationCallback&&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    CGRect bounds() const;
    void updateBoundsAndPosition(CGRect, std::optional<WTF::MachSendRight>&&);

    void ref() final { RefCounted::ref(); }
    void deref() final { RefCounted::deref(); }

private:
    RemoteSampleBufferDisplayLayer(GPUConnectionToWebProcess&, SampleBufferDisplayLayerIdentifier, Ref<IPC::Connection>&&);

#if !RELEASE_LOG_DISABLED
    void setLogIdentifier(String&&);
#endif
    void updateDisplayMode(bool hideDisplayLayer, bool hideRootLayer);
    void flush();
    void flushAndRemoveImage();
    void play();
    void pause();
    void enqueueVideoFrame(SharedVideoFrame&&);
    void clearVideoFrames();
    void setSharedVideoFrameSemaphore(IPC::Semaphore&&);
    void setSharedVideoFrameMemory(WebCore::SharedMemory::Handle&&);
    void setShouldMaintainAspectRatio(bool shouldMaintainAspectRatio);

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final { return m_identifier.toUInt64(); }

    // WebCore::SampleBufferDisplayLayerClient
    void sampleBufferDisplayLayerStatusDidFail() final;
#if PLATFORM(IOS_FAMILY)
    bool canShowWhileLocked() const final { return false; }
#endif

    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_gpuConnection WTF_GUARDED_BY_CAPABILITY(m_consumeThread);
    SampleBufferDisplayLayerIdentifier m_identifier;
    Ref<IPC::Connection> m_connection;
    RefPtr<WebCore::LocalSampleBufferDisplayLayer> m_sampleBufferDisplayLayer;
    std::unique_ptr<LayerHostingContext> m_layerHostingContext;
    SharedVideoFrameReader m_sharedVideoFrameReader;
    ThreadLikeAssertion m_consumeThread NO_UNIQUE_ADDRESS;
};

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
