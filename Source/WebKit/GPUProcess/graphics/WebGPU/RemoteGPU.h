/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "RemoteGPURequestAdapterResponse.h"
#include "RemoteVideoFrameIdentifier.h"
#include "SharedPreferencesForWebProcess.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include "WebGPUIdentifier.h"
#include "WebGPUObjectHeap.h"
#include "WebGPUSupportedFeatures.h"
#include "WebGPUSupportedLimits.h"
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadAssertions.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore::WebGPU {
class GPU;
struct PresentationContextDescriptor;
}

namespace IPC {
class Connection;
class StreamServerConnection;
}

namespace WebCore {
class MediaPlayer;
class NativeImage;
class VideoFrame;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteRenderingBackend;

namespace WebGPU {
class ObjectHeap;
struct RequestAdapterOptions;
}

class RemoteGPU final : public IPC::StreamMessageReceiver, public CanMakeWeakPtr<RemoteGPU> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteGPU);
public:
    static Ref<RemoteGPU> create(WebGPUIdentifier identifier, GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& serverConnection)
    {
        auto result = adoptRef(*new RemoteGPU(identifier, gpuConnectionToWebProcess, renderingBackend, WTFMove(serverConnection)));
        result->initialize();
        return result;
    }

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const { return m_sharedPreferencesForWebProcess; }

    virtual ~RemoteGPU();

    void stopListeningForIPC();

    void paintNativeImageToImageBuffer(WebCore::NativeImage&, WebCore::RenderingResourceIdentifier);
    RefPtr<GPUConnectionToWebProcess> gpuConnectionToWebProcess() const;

private:
    friend class WebGPU::ObjectHeap;

    RemoteGPU(WebGPUIdentifier, GPUConnectionToWebProcess&, RemoteRenderingBackend&, Ref<IPC::StreamServerConnection>&&);

    RemoteGPU(const RemoteGPU&) = delete;
    RemoteGPU(RemoteGPU&&) = delete;
    RemoteGPU& operator=(const RemoteGPU&) = delete;
    RemoteGPU& operator=(RemoteGPU&&) = delete;

    RefPtr<IPC::Connection> connection() const;

    void initialize();
    IPC::StreamConnectionWorkQueue& workQueue() const { return m_workQueue; }
    Ref<IPC::StreamConnectionWorkQueue> protectedWorkQueue() const { return m_workQueue; }
    void workQueueInitialize();
    void workQueueUninitialize();

    template<typename T>
    IPC::Error send(T&& message) const
    {
        // FIXME: Remove this suppression once https://github.com/llvm/llvm-project/pull/119336 is merged.
IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_BEGIN("alpha.webkit.UncountedCallArgsChecker")
        return Ref { *m_streamConnection }->send(std::forward<T>(message), m_identifier);
IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_END
    }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void requestAdapter(const WebGPU::RequestAdapterOptions&, WebGPUIdentifier, CompletionHandler<void(std::optional<RemoteGPURequestAdapterResponse>&&)>&&);

    void createPresentationContext(const WebGPU::PresentationContextDescriptor&, WebGPUIdentifier);

    void createCompositorIntegration(WebGPUIdentifier);

    void isValid(WebGPUIdentifier, CompletionHandler<void(bool, bool)>&&);

    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    SharedPreferencesForWebProcess m_sharedPreferencesForWebProcess;
    Ref<IPC::StreamConnectionWorkQueue> m_workQueue;
    RefPtr<IPC::StreamServerConnection> m_streamConnection;
    RefPtr<WebCore::WebGPU::GPU> m_backing WTF_GUARDED_BY_CAPABILITY(workQueue());
    Ref<WebGPU::ObjectHeap> m_objectHeap WTF_GUARDED_BY_CAPABILITY(workQueue());
    const WebGPUIdentifier m_identifier;
    Ref<RemoteRenderingBackend> m_renderingBackend;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
