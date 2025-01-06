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
#include "WebGPUExtent3D.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUIntegralTypes.h>
#include <cstdint>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SharedMemoryHandle;
}

namespace WebCore::WebGPU {
class Queue;
}

namespace IPC {
class StreamServerConnection;
}

namespace WebKit {

namespace WebGPU {
struct ImageCopyExternalImage;
struct ImageCopyTexture;
struct ImageCopyTextureTagged;
struct ImageDataLayout;
class ObjectHeap;
}

class RemoteQueue final : public IPC::StreamMessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteQueue);
public:
    static Ref<RemoteQueue> create(WebCore::WebGPU::Queue& queue, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteQueue(queue, objectHeap, WTFMove(streamConnection), gpu, identifier));
    }

    virtual ~RemoteQueue();

    // FIXME: Remove SUPPRESS_UNCOUNTED_ARG once https://github.com/llvm/llvm-project/pull/111198 lands.
    SUPPRESS_UNCOUNTED_ARG std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const { return m_gpu->sharedPreferencesForWebProcess(); }

    void stopListeningForIPC();

private:
    friend class WebGPU::ObjectHeap;

    RemoteQueue(WebCore::WebGPU::Queue&, WebGPU::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, RemoteGPU&, WebGPUIdentifier);

    RemoteQueue(const RemoteQueue&) = delete;
    RemoteQueue(RemoteQueue&&) = delete;
    RemoteQueue& operator=(const RemoteQueue&) = delete;
    RemoteQueue& operator=(RemoteQueue&&) = delete;

    WebCore::WebGPU::Queue& backing() { return m_backing; }
    Ref<WebCore::WebGPU::Queue> protectedBacking();

    Ref<WebGPU::ObjectHeap> protectedObjectHeap() const;

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void submit(Vector<WebGPUIdentifier>&&);

    void onSubmittedWorkDone(CompletionHandler<void()>&&);

    void writeBuffer(
        WebGPUIdentifier,
        WebCore::WebGPU::Size64 bufferOffset,
        std::optional<WebCore::SharedMemoryHandle>&&,
        CompletionHandler<void(bool)>&&);

    void writeTexture(
        const WebGPU::ImageCopyTexture& destination,
        std::optional<WebCore::SharedMemoryHandle>&&,
        const WebGPU::ImageDataLayout&,
        const WebGPU::Extent3D& size,
        CompletionHandler<void(bool)>&&);

    void copyExternalImageToTexture(
        const WebGPU::ImageCopyExternalImage& source,
        const WebGPU::ImageCopyTextureTagged& destination,
        const WebGPU::Extent3D& copySize);

    void setLabel(String&&);
    void destruct();

    Ref<WebCore::WebGPU::Queue> m_backing;
    WeakRef<WebGPU::ObjectHeap> m_objectHeap;
    const Ref<IPC::StreamServerConnection> m_streamConnection;
    WeakRef<RemoteGPU> m_gpu;
    WebGPUIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
