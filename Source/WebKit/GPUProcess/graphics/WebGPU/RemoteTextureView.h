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
#include "WebGPUIdentifier.h"
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace PAL::WebGPU {
class TextureView;
}

namespace WebKit {

namespace WebGPU {
class ObjectHeap;
}

class RemoteTextureView final : public IPC::StreamMessageReceiver {
public:
    static Ref<RemoteTextureView> create(PAL::WebGPU::TextureView& textureView, WebGPU::ObjectHeap& objectHeap)
    {
        return adoptRef(*new RemoteTextureView(textureView, objectHeap));
    }

    virtual ~RemoteTextureView();

private:
    friend class ObjectHeap;

    RemoteTextureView(PAL::WebGPU::TextureView&, WebGPU::ObjectHeap&);

    RemoteTextureView(const RemoteTextureView&) = delete;
    RemoteTextureView(RemoteTextureView&&) = delete;
    RemoteTextureView& operator=(const RemoteTextureView&) = delete;
    RemoteTextureView& operator=(RemoteTextureView&&) = delete;

    void didReceiveStreamMessage(IPC::StreamServerConnectionBase&, IPC::Decoder&) final;

    void setLabel(String&&);

    Ref<PAL::WebGPU::TextureView> m_backing;
    Ref<WebGPU::ObjectHeap> m_objectHeap;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
