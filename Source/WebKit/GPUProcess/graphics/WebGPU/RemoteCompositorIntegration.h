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

#include "RemoteGPU.h"
#include "StreamMessageReceiver.h"
#include "WebGPUIdentifier.h"
#include <WebCore/AlphaPremultiplication.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <WebCore/WebGPUIntegralTypes.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/MachSendRight.h>
#include <wtf/Vector.h>
#endif

namespace WebCore {
class DestinationColorSpace;
class ImageBuffer;
}

namespace WebCore::WebGPU {
class CompositorIntegration;
}

namespace IPC {
class Connection;
class StreamServerConnection;
}

namespace WebKit {

class RemoteGPU;

namespace WebGPU {
class ObjectHeap;
}

class RemoteCompositorIntegration final : public IPC::StreamMessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteCompositorIntegration);
public:
    static Ref<RemoteCompositorIntegration> create(WebCore::WebGPU::CompositorIntegration& compositorIntegration, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteCompositorIntegration(compositorIntegration, objectHeap, WTFMove(streamConnection), gpu, identifier));
    }

    virtual ~RemoteCompositorIntegration();

    // FIXME: Remove SUPPRESS_UNCOUNTED_ARG once https://github.com/llvm/llvm-project/pull/111198 lands.
    SUPPRESS_UNCOUNTED_ARG std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const { return m_gpu->sharedPreferencesForWebProcess(); }

    void stopListeningForIPC();

private:
    friend class WebGPU::ObjectHeap;

    RemoteCompositorIntegration(WebCore::WebGPU::CompositorIntegration&, WebGPU::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, RemoteGPU&, WebGPUIdentifier);

    RemoteCompositorIntegration(const RemoteCompositorIntegration&) = delete;
    RemoteCompositorIntegration(RemoteCompositorIntegration&&) = delete;
    RemoteCompositorIntegration& operator=(const RemoteCompositorIntegration&) = delete;
    RemoteCompositorIntegration& operator=(RemoteCompositorIntegration&&) = delete;

    WebCore::WebGPU::CompositorIntegration& backing() { return m_backing; }
    Ref<WebCore::WebGPU::CompositorIntegration> protectedBacking();

    Ref<IPC::StreamServerConnection> protectedStreamConnection() const;

    Ref<WebGPU::ObjectHeap> protectedObjectHeap() const { return m_objectHeap.get(); }
    Ref<RemoteGPU> protectedGPU() const { return m_gpu.get(); }

    RefPtr<IPC::Connection> connection() const;

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;
    void destruct();
    void paintCompositedResultsToCanvas(WebCore::RenderingResourceIdentifier, uint32_t, CompletionHandler<void()>&&);

#if PLATFORM(COCOA)
    void recreateRenderBuffers(int width, int height, WebCore::DestinationColorSpace&&, WebCore::AlphaPremultiplication, WebCore::WebGPU::TextureFormat, WebKit::WebGPUIdentifier deviceIdentifier, CompletionHandler<void(Vector<MachSendRight>&&)>&&);
#endif

    void prepareForDisplay(uint32_t frameIndex, CompletionHandler<void(bool)>&&);

    Ref<WebCore::WebGPU::CompositorIntegration> m_backing;
    WeakRef<WebGPU::ObjectHeap> m_objectHeap;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    WeakRef<RemoteGPU> m_gpu;
    WebGPUIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
