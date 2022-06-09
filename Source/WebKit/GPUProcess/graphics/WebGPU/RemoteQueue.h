/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "StreamMessageReceiver.h"
#include "WebGPUExtent3D.h"
#include "WebGPUIdentifier.h"
#include <cstdint>
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace PAL::WebGPU {
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
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteQueue> create(PAL::WebGPU::Queue& queue, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteQueue(queue, objectHeap, WTFMove(streamConnection), identifier));
    }

    virtual ~RemoteQueue();

    void stopListeningForIPC();

private:
    friend class WebGPU::ObjectHeap;

    RemoteQueue(PAL::WebGPU::Queue&, WebGPU::ObjectHeap&, Ref<IPC::StreamServerConnection>&&, WebGPUIdentifier);

    RemoteQueue(const RemoteQueue&) = delete;
    RemoteQueue(RemoteQueue&&) = delete;
    RemoteQueue& operator=(const RemoteQueue&) = delete;
    RemoteQueue& operator=(RemoteQueue&&) = delete;

    PAL::WebGPU::Queue& backing() { return m_backing; }

    void didReceiveStreamMessage(IPC::StreamServerConnectionBase&, IPC::Decoder&) final;

    void submit(Vector<WebGPUIdentifier>&&);

    void onSubmittedWorkDone(WTF::CompletionHandler<void()>&&);

    void writeBuffer(
        WebGPUIdentifier,
        PAL::WebGPU::Size64 bufferOffset,
        Vector<uint8_t>&&);

    void writeTexture(
        const WebGPU::ImageCopyTexture& destination,
        Vector<uint8_t>&&,
        const WebGPU::ImageDataLayout&,
        const WebGPU::Extent3D& size);

    void copyExternalImageToTexture(
        const WebGPU::ImageCopyExternalImage& source,
        const WebGPU::ImageCopyTextureTagged& destination,
        const WebGPU::Extent3D& copySize);

    void setLabel(String&&);

    Ref<PAL::WebGPU::Queue> m_backing;
    WebGPU::ObjectHeap& m_objectHeap;
    Ref<IPC::StreamServerConnection> m_streamConnection;
    WebGPUIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
